#include "furi_hal_version.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include <esp_mac.h>
#include <mbedtls/sha512.h>

/* -------------------------------------------------------------------------
 * Unique device name derived from the factory eFuse MAC.
 *
 * Ported 1:1 from flipogen.py: the MAC is turned into a 12-digit hex serial,
 * hashed with SHA-512, and the digest nibbles deterministically pick phonetic
 * syllables, consonants, vowels and digits to build a short "Pokemon-like"
 * name. Every board gets a stable, unique-ish name without user interaction.
 *
 * The user can still override it via Settings -> Desktop -> Change Flipper
 * Name (/ext/dolphin/name.settings).
 * -------------------------------------------------------------------------*/

static const char* const s_syl_cv[] = {
    "ka", "ra", "ta", "la", "na", "ma", "sa", "za", "fa", "ga", "ha", "pa", "va",
    "ke", "re", "te", "le", "ne", "me", "se", "ze", "fe", "ge", "he", "pe", "ve",
    "ki", "ri", "ti", "li", "ni", "mi", "si", "zi", "fi", "gi", "hi", "pi", "vi",
    "ko", "ro", "to", "lo", "no", "mo", "so", "zo", "fo", "go", "ho", "po", "vo",
    "ku", "ru", "tu", "lu", "nu", "mu", "su", "zu", "fu", "gu", "hu", "pu", "vu",
    "ky", "ry", "ty", "ly", "ny", "my", "sy", "zy", "fy", "gy", "hy", "py", "vy",
};
#define SYL_CV_COUNT (sizeof(s_syl_cv) / sizeof(s_syl_cv[0]))

static const char s_syl_c[] = "bcdfghjklmnpqrstvwxyz";
static const char s_syl_v[] = "aeiouy";
static const char s_digits[] = "0123456789";

static uint32_t hex_digit(char c) {
    if(c >= '0' && c <= '9') return c - '0';
    if(c >= 'a' && c <= 'f') return (uint32_t)(c - 'a' + 10);
    if(c >= 'A' && c <= 'F') return (uint32_t)(c - 'A' + 10);
    return 0;
}

/* Python's int(h[a:b], 16) on the SHA-512 hex digest. */
static uint32_t hex_slice(const char* h, int a, int b) {
    uint32_t v = 0;
    for(int i = a; i < b; i++) v = (v << 4) | hex_digit(h[i]);
    return v;
}

static const char* pick_str(const char* h, int a, int b, const char* const* arr, size_t n) {
    return arr[hex_slice(h, a, b) % n];
}

static char pick_char(const char* h, int a, int b, const char arr[], size_t n) {
    return arr[hex_slice(h, a, b) % n];
}

/* Soft l33t, deterministic from the digest slice at position pos. */
static char l33t(char c, const char* h, int pos) {
    uint32_t v = hex_slice(h, pos, pos + 2) % 10;
    if(c == 'a' && v == 0) return '4';
    if(c == 'e' && v == 1) return '3';
    if(c == 'o' && v == 2) return '0';
    if(c == 'i' && v == 3) return '1';
    return c;
}

static bool is_digit_char(char c) {
    return c >= '0' && c <= '9';
}

/* Build the name the same way flipogen.py::generate_name() does. */
static void derive_name_from_mac(const uint8_t mac[6], char* out, size_t out_size) {
    char serial[13];
    snprintf(
        serial,
        sizeof(serial),
        "%02X%02X%02X%02X%02X%02X",
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    unsigned char digest[64];
    if(mbedtls_sha512((const unsigned char*)serial, strlen(serial), digest, 0) != 0) {
        snprintf(out, out_size, "Flipper");
        return;
    }

    char h[129];
    static const char hex_chars[] = "0123456789abcdef";
    for(int i = 0; i < 64; i++) {
        h[i * 2] = hex_chars[digest[i] >> 4];
        h[i * 2 + 1] = hex_chars[digest[i] & 0x0F];
    }
    h[128] = '\0';

    char raw[8];
    int pos = 0;
    uint32_t pattern = hex_slice(h, 0, 2) % 4;

    if(pattern == 0) {
        /* CV + CC + d + C */
        const char* p = pick_str(h, 2, 6, s_syl_cv, SYL_CV_COUNT);
        raw[pos++] = p[0];
        raw[pos++] = p[1];
        raw[pos++] = pick_char(h, 6, 8, s_syl_c, sizeof(s_syl_c) - 1);
        raw[pos++] = pick_char(h, 8, 10, s_syl_c, sizeof(s_syl_c) - 1);
        raw[pos++] = pick_char(h, 10, 12, s_digits, sizeof(s_digits) - 1);
        raw[pos++] = pick_char(h, 12, 14, s_syl_c, sizeof(s_syl_c) - 1);
    } else if(pattern == 1) {
        /* CV + C + V + d + C */
        const char* p = pick_str(h, 2, 6, s_syl_cv, SYL_CV_COUNT);
        raw[pos++] = p[0];
        raw[pos++] = p[1];
        raw[pos++] = pick_char(h, 6, 8, s_syl_c, sizeof(s_syl_c) - 1);
        raw[pos++] = pick_char(h, 8, 10, s_syl_v, sizeof(s_syl_v) - 1);
        raw[pos++] = pick_char(h, 10, 12, s_digits, sizeof(s_digits) - 1);
        raw[pos++] = pick_char(h, 12, 14, s_syl_c, sizeof(s_syl_c) - 1);
    } else if(pattern == 2) {
        /* CV + CV + d + C */
        const char* p1 = pick_str(h, 2, 6, s_syl_cv, SYL_CV_COUNT);
        const char* p2 = pick_str(h, 6, 10, s_syl_cv, SYL_CV_COUNT);
        raw[pos++] = p1[0];
        raw[pos++] = p1[1];
        raw[pos++] = p2[0];
        raw[pos++] = p2[1];
        raw[pos++] = pick_char(h, 10, 12, s_digits, sizeof(s_digits) - 1);
        raw[pos++] = pick_char(h, 12, 14, s_syl_c, sizeof(s_syl_c) - 1);
    } else {
        /* CV + C + d + CV */
        const char* p1 = pick_str(h, 2, 6, s_syl_cv, SYL_CV_COUNT);
        const char* p2 = pick_str(h, 10, 14, s_syl_cv, SYL_CV_COUNT);
        raw[pos++] = p1[0];
        raw[pos++] = p1[1];
        raw[pos++] = pick_char(h, 6, 8, s_syl_c, sizeof(s_syl_c) - 1);
        raw[pos++] = pick_char(h, 8, 10, s_digits, sizeof(s_digits) - 1);
        raw[pos++] = p2[0];
        raw[pos++] = p2[1];
    }
    raw[pos] = '\0';

    char name[9];
    for(int i = 0; i < pos; i++) {
        name[i] = l33t(raw[i], h, 14 + 2 * i);
    }
    name[pos] = '\0';
    if(name[0] >= 'a' && name[0] <= 'z') name[0] = (char)(name[0] - 'a' + 'A');

    /* Python's "if sum(c.isdigit()) > 2" guard (normally never triggered). */
    size_t digit_count = 0;
    for(int i = 0; i < pos; i++) {
        if(is_digit_char(name[i])) digit_count++;
    }
    if(digit_count > 2) {
        int idx = (int)(hex_slice(h, 40, 42) % (uint32_t)pos);
        if(is_digit_char(name[idx])) {
            name[idx] = pick_char(h, 42, 44, s_syl_c, sizeof(s_syl_c) - 1);
        }
    }

    /* Generated names are always 6 chars (within the 4..8 filter). */
    snprintf(out, out_size, "%s", name);
}

typedef struct {
    char name[FURI_HAL_VERSION_ARRAY_NAME_LENGTH];
    char device_name[FURI_HAL_VERSION_DEVICE_NAME_LENGTH];
    char ble_local_name[FURI_HAL_BT_ADV_NAME_LENGTH + 1];
    uint8_t ble_mac[6];
    uint8_t uid[6];
} FuriHalVersionState;

static FuriHalVersionState furi_hal_version = {
    .name = "ESP32",
    .device_name = "Furi ESP32",
    .ble_local_name = "ESP32",
    .ble_mac = {0},
    .uid = {0},
};

static void furi_hal_version_refresh_names(const char* name) {
    snprintf(furi_hal_version.name, sizeof(furi_hal_version.name), "%s", name);
    snprintf(
        furi_hal_version.device_name,
        sizeof(furi_hal_version.device_name),
        "Furi %s",
        furi_hal_version.name);
    snprintf(
        furi_hal_version.ble_local_name,
        sizeof(furi_hal_version.ble_local_name),
        "%s",
        furi_hal_version.name);

    version_set_custom_name((Version*)version_get(), furi_hal_version.name);
}

void furi_hal_version_init(void) {
    /* Read factory MAC from eFuse — used as hardware UID and BLE address. */
    esp_efuse_mac_get_default(furi_hal_version.uid);
    memcpy(furi_hal_version.ble_mac, furi_hal_version.uid, sizeof(furi_hal_version.uid));

    /* Determine effective name:
     *   1. If a custom name was already loaded (e.g., from namechanger), use it.
     *   2. Otherwise build a syllable name from the eFuse MAC (flipogen.py). */
    const char* custom = version_get_custom_name(NULL);
    char generated[FURI_HAL_VERSION_ARRAY_NAME_LENGTH];
    const char* effective;
    if(custom && custom[0]) {
        effective = custom;
    } else {
        derive_name_from_mac(
            furi_hal_version.uid, generated, sizeof(generated));
        effective = generated;
    }
    furi_hal_version_refresh_names(effective);
}

bool furi_hal_version_do_i_belong_here(void) {
    return true;
}

const char* furi_hal_version_get_model_name(void) {
    return "Flipper Zero";
}

const char* furi_hal_version_get_model_code(void) {
    return "ESP32-S3";
}

const char* furi_hal_version_get_fcc_id(void) {
    return "N/A";
}

const char* furi_hal_version_get_ic_id(void) {
    return "N/A";
}

const char* furi_hal_version_get_mic_id(void) {
    return "N/A";
}

const char* furi_hal_version_get_srrc_id(void) {
    return "N/A";
}

const char* furi_hal_version_get_ncc_id(void) {
    return "N/A";
}

FuriHalVersionOtpVersion furi_hal_version_get_otp_version(void) {
    return FuriHalVersionOtpVersionEmpty;
}

uint8_t furi_hal_version_get_hw_version(void) {
    return 1;
}

uint8_t furi_hal_version_get_hw_target(void) {
    return version_get_target(NULL);
}

uint8_t furi_hal_version_get_hw_body(void) {
    return 0;
}

FuriHalVersionColor furi_hal_version_get_hw_color(void) {
    return FuriHalVersionColorUnknown;
}

uint8_t furi_hal_version_get_hw_connect(void) {
    return 0;
}

FuriHalVersionRegion furi_hal_version_get_hw_region(void) {
    return FuriHalVersionRegionWorld;
}

const char* furi_hal_version_get_hw_region_name(void) {
    return "WW";
}

FuriHalVersionRegion furi_hal_version_get_hw_region_otp(void) {
    return FuriHalVersionRegionWorld;
}

const char* furi_hal_version_get_hw_region_name_otp(void) {
    return "WW";
}

FuriHalVersionDisplay furi_hal_version_get_hw_display(void) {
    return FuriHalVersionDisplayUnknown;
}

uint32_t furi_hal_version_get_hw_timestamp(void) {
    return 0;
}

const char* furi_hal_version_get_name_ptr(void) {
    return furi_hal_version.name;
}

const char* furi_hal_version_get_device_name_ptr(void) {
    return furi_hal_version.device_name;
}

const char* furi_hal_version_get_ble_local_device_name_ptr(void) {
    return furi_hal_version.ble_local_name;
}

void furi_hal_version_set_name(const char* name) {
    /* Accept NULL or empty string — fall back to the MAC-derived name. */
    if(name && name[0]) {
        furi_hal_version_refresh_names(name);
    } else {
        char generated[FURI_HAL_VERSION_ARRAY_NAME_LENGTH];
        derive_name_from_mac(furi_hal_version.uid, generated, sizeof(generated));
        furi_hal_version_refresh_names(generated);
    }
}

const uint8_t* furi_hal_version_get_ble_mac(void) {
    return furi_hal_version.ble_mac;
}

const struct Version* furi_hal_version_get_firmware_version(void) {
    return version_get();
}

size_t furi_hal_version_uid_size(void) {
    return sizeof(furi_hal_version.uid);
}

const uint8_t* furi_hal_version_uid(void) {
    return furi_hal_version.uid;
}

const uint8_t* furi_hal_version_uid_default(void) {
    return furi_hal_version.uid;
}
