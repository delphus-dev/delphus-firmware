/**
 * @file furi_hal_crypto.c
 * Software "enclave" for ESP32.
 *
 * STM32 Flipper uses a hardware crypto accelerator + OTP keys. Here we
 * derive per-slot AES keys from the factory-burned eFuse MAC (UID) via
 * HMAC-SHA256 and use mbedTLS for AES-256-CBC. Optionally, an imported key
 * can be stored in NVS and overrides the derived one.
 *
 * Security note: this protects data at rest (e.g. an SD card pulled from the
 * device), but anyone with access to the device/firmware can read the MAC and
 * recompute the keys. It is obfuscation, not a hardware secure enclave.
 */

#include "furi_hal_crypto.h"
#include "furi_hal_version.h"

#include <string.h>
#include <stdio.h>

#include <esp_log.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <mbedtls/aes.h>
#include <mbedtls/md.h>

#define TAG "FuriHalCrypto"

#define CRYPTO_NVS_NAMESPACE "furi_crypto"
#define CRYPTO_MAX_SLOT      100u
#define CRYPTO_KEY_SIZE      32u
#define CRYPTO_IV_SIZE       16u

static mbedtls_aes_context aes_enc;
static mbedtls_aes_context aes_dec;
static uint8_t current_iv[CRYPTO_IV_SIZE];
static bool aes_ready = false;

static void crypto_lock(void) {
    /* All callers run from Furi threads; the AES contexts are only touched
     * between load/unload pairs within one thread (same as stock firmware). */
}

/** Derive a deterministic per-device key for a slot from the eFuse MAC. */
static void crypto_derive_key(uint8_t slot, uint8_t out[CRYPTO_KEY_SIZE]) {
    const uint8_t* uid = furi_hal_version_uid();
    uint8_t seed[32] = {0};
    size_t seed_len = 0;

    if(uid) {
        memcpy(seed, uid, 6);
        seed_len = 6;
    }
    int written = snprintf(
        (char*)seed + seed_len,
        sizeof(seed) - seed_len,
        "delphus-crypto-slot-%u",
        (unsigned)slot);
    if(written > 0) seed_len += (size_t)written;

    const mbedtls_md_info_t* md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    int ret = mbedtls_md_hmac(
        md, seed, seed_len,
        (const uint8_t*)"delphus-enclave-key", 19,
        out);
    if(ret != 0) memset(out, 0, CRYPTO_KEY_SIZE);
    memset(seed, 0, sizeof(seed));
}

static bool crypto_nvs_get(uint8_t slot, uint8_t* data, size_t* size) {
    char name[8];
    snprintf(name, sizeof(name), "k%u", (unsigned)slot);
    nvs_handle_t handle;
    if(nvs_open(CRYPTO_NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) return false;
    size_t len = *size;
    bool ok = nvs_get_blob(handle, name, data, &len) == ESP_OK && len >= 16 && len <= 32;
    if(ok) *size = len;
    nvs_close(handle);
    return ok;
}

static bool crypto_get_key(uint8_t slot, uint8_t* key, size_t* key_len) {
    if(slot == 0 || slot > CRYPTO_MAX_SLOT) return false;
    size_t want = CRYPTO_KEY_SIZE;
    if(crypto_nvs_get(slot, key, &want)) {
        *key_len = want;
        return true;
    }
    /* Slots 1 (SubGHz keystore) and 11 (U2F) are provisioned by derivation. */
    if(slot == 1 || slot == FURI_HAL_CRYPTO_ENCLAVE_UNIQUE_KEY_SLOT) {
        crypto_derive_key(slot, key);
        *key_len = CRYPTO_KEY_SIZE;
        return true;
    }
    return false;
}

static bool crypto_set_aes_key(const uint8_t* key, size_t key_len, const uint8_t* iv) {
    if(key_len != 16 && key_len != 32) return false;
    mbedtls_aes_init(&aes_enc);
    mbedtls_aes_init(&aes_dec);
    if(mbedtls_aes_setkey_enc(&aes_enc, key, (unsigned)key_len * 8) != 0) return false;
    if(mbedtls_aes_setkey_dec(&aes_dec, key, (unsigned)key_len * 8) != 0) return false;
    memset(current_iv, 0, sizeof(current_iv));
    if(iv) memcpy(current_iv, iv, CRYPTO_IV_SIZE);
    aes_ready = true;
    return true;
}

void furi_hal_crypto_init(void) {
    mbedtls_aes_init(&aes_enc);
    mbedtls_aes_init(&aes_dec);
    ESP_LOGI(TAG, "Software crypto initialized (eFuse MAC derived keys)");
}

bool furi_hal_crypto_enclave_verify(uint8_t* keys_nb, uint8_t* valid_keys_nb) {
    if(keys_nb) *keys_nb = CRYPTO_MAX_SLOT;
    if(valid_keys_nb) *valid_keys_nb = CRYPTO_MAX_SLOT;
    return true;
}

bool furi_hal_crypto_enclave_ensure_key(uint8_t key_slot) {
    if(key_slot == 0 || key_slot > CRYPTO_MAX_SLOT) return false;
    uint8_t key[CRYPTO_KEY_SIZE];
    size_t len = sizeof(key);
    if(crypto_nvs_get(key_slot, key, &len)) return true;
    if(key_slot == 1 || key_slot == FURI_HAL_CRYPTO_ENCLAVE_UNIQUE_KEY_SLOT) return true;
    return false;
}

bool furi_hal_crypto_enclave_store_key(FuriHalCryptoKey* key, uint8_t* slot) {
    if(!key || !slot || !key->data) return false;
    if(*slot == 0 || *slot > CRYPTO_MAX_SLOT) return false;
    size_t len = (key->size == FuriHalCryptoKeySize128) ? 16 : 32;

    nvs_handle_t handle;
    if(nvs_open(CRYPTO_NVS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK) return false;
    char name[8];
    snprintf(name, sizeof(name), "k%u", (unsigned)*slot);
    esp_err_t err = nvs_set_blob(handle, name, key->data, len);
    if(err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);
    if(err != ESP_OK) return false;
    memset(key->data, 0, len);
    return true;
}

bool furi_hal_crypto_enclave_load_key(uint8_t slot, const uint8_t* iv) {
    crypto_lock();
    uint8_t key[CRYPTO_KEY_SIZE];
    size_t len = sizeof(key);
    if(!crypto_get_key(slot, key, &len)) {
        memset(key, 0, sizeof(key));
        return false;
    }
    bool ok = crypto_set_aes_key(key, len, iv);
    memset(key, 0, sizeof(key));
    return ok;
}

bool furi_hal_crypto_enclave_unload_key(uint8_t slot) {
    (void)slot;
    aes_ready = false;
    memset(current_iv, 0, sizeof(current_iv));
    mbedtls_aes_free(&aes_enc);
    mbedtls_aes_free(&aes_dec);
    return true;
}

bool furi_hal_crypto_load_key(const uint8_t* key, const uint8_t* iv) {
    return crypto_set_aes_key(key, CRYPTO_KEY_SIZE, iv);
}

bool furi_hal_crypto_unload_key(void) {
    return furi_hal_crypto_enclave_unload_key(0);
}

bool furi_hal_crypto_encrypt(const uint8_t* input, uint8_t* output, size_t size) {
    if(!aes_ready || !input || !output || size == 0 || (size % 16) != 0) return false;
    uint8_t iv[CRYPTO_IV_SIZE];
    memcpy(iv, current_iv, sizeof(iv));
    int ret = mbedtls_aes_crypt_cbc(
        &aes_enc, MBEDTLS_AES_ENCRYPT, size, iv, input, output);
    memset(iv, 0, sizeof(iv));
    return ret == 0;
}

bool furi_hal_crypto_decrypt(const uint8_t* input, uint8_t* output, size_t size) {
    if(!aes_ready || !input || !output || size == 0 || (size % 16) != 0) return false;
    uint8_t iv[CRYPTO_IV_SIZE];
    memcpy(iv, current_iv, sizeof(iv));
    int ret = mbedtls_aes_crypt_cbc(
        &aes_dec, MBEDTLS_AES_DECRYPT, size, iv, input, output);
    memset(iv, 0, sizeof(iv));
    return ret == 0;
}
