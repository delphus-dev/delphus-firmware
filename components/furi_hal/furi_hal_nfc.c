/**
 * @file furi_hal_nfc.c
 * @brief NFC HAL placeholder for the Delphus board.
 *
 * The PN532 (I2C and SPI) backend has been removed. This board uses an
 * ST25R3916, which will get its own HAL implementation; until then the
 * complete furi_hal_nfc API is kept as stubs so the NFC application and
 * protocol stack still build. Every operation reports "no NFC hardware".
 */

#include "furi_hal_nfc.h"
#include <furi.h>

#define TAG "FuriHalNfc"

FuriHalNfcError furi_hal_nfc_init(void) {
    FURI_LOG_I(TAG, "NFC HAL: no supported NFC chip (ST25R3916 port pending)");
    return FuriHalNfcErrorNone;
}

FuriHalNfcError furi_hal_nfc_is_hal_ready(void) {
    return FuriHalNfcErrorCommunication;
}

FuriHalNfcError furi_hal_nfc_acquire(void) {
    return FuriHalNfcErrorCommunication;
}

FuriHalNfcError furi_hal_nfc_release(void) {
    return FuriHalNfcErrorNone;
}

FuriHalNfcError furi_hal_nfc_low_power_mode_start(void) {
    return FuriHalNfcErrorNone;
}

FuriHalNfcError furi_hal_nfc_low_power_mode_stop(void) {
    return FuriHalNfcErrorNone;
}

FuriHalNfcError furi_hal_nfc_set_mode(FuriHalNfcMode mode, FuriHalNfcTech tech) {
    UNUSED(mode);
    UNUSED(tech);
    return FuriHalNfcErrorCommunication;
}

FuriHalNfcError furi_hal_nfc_reset_mode(void) {
    return FuriHalNfcErrorNone;
}

FuriHalNfcError furi_hal_nfc_field_detect_start(void) {
    return FuriHalNfcErrorNone;
}

FuriHalNfcError furi_hal_nfc_field_detect_stop(void) {
    return FuriHalNfcErrorNone;
}

bool furi_hal_nfc_field_is_present(void) {
    return false;
}

FuriHalNfcError furi_hal_nfc_poller_field_on(void) {
    return FuriHalNfcErrorCommunication;
}

FuriHalNfcEvent furi_hal_nfc_poller_wait_event(uint32_t timeout_ms) {
    UNUSED(timeout_ms);
    return FuriHalNfcEventTimeout;
}

FuriHalNfcEvent furi_hal_nfc_listener_wait_event(uint32_t timeout_ms) {
    UNUSED(timeout_ms);
    return FuriHalNfcEventTimeout;
}

FuriHalNfcError furi_hal_nfc_poller_tx(const uint8_t* tx_data, size_t tx_bits) {
    UNUSED(tx_data);
    UNUSED(tx_bits);
    return FuriHalNfcErrorCommunication;
}

FuriHalNfcError furi_hal_nfc_poller_rx(uint8_t* rx_data, size_t rx_data_size, size_t* rx_bits) {
    UNUSED(rx_data);
    UNUSED(rx_data_size);
    UNUSED(rx_bits);
    return FuriHalNfcErrorCommunication;
}

FuriHalNfcError furi_hal_nfc_listener_tx(const uint8_t* tx_data, size_t tx_bits) {
    UNUSED(tx_data);
    UNUSED(tx_bits);
    return FuriHalNfcErrorCommunication;
}

FuriHalNfcError furi_hal_nfc_listener_rx(uint8_t* rx_data, size_t rx_data_size, size_t* rx_bits) {
    UNUSED(rx_data);
    UNUSED(rx_data_size);
    UNUSED(rx_bits);
    return FuriHalNfcErrorCommunication;
}

void furi_hal_nfc_emu_set_ndef(const uint8_t* msg, size_t len) {
    UNUSED(msg);
    UNUSED(len);
}

FuriHalNfcError furi_hal_nfc_listener_sleep(void) {
    return FuriHalNfcErrorNone;
}

FuriHalNfcError furi_hal_nfc_listener_idle(void) {
    return FuriHalNfcErrorNone;
}

FuriHalNfcError furi_hal_nfc_listener_enable_rx(void) {
    return FuriHalNfcErrorNone;
}

FuriHalNfcError furi_hal_nfc_trx_reset(void) {
    return FuriHalNfcErrorNone;
}

FuriHalNfcError furi_hal_nfc_event_start(void) {
    return FuriHalNfcErrorNone;
}

FuriHalNfcError furi_hal_nfc_event_stop(void) {
    return FuriHalNfcErrorNone;
}

FuriHalNfcError furi_hal_nfc_abort(void) {
    return FuriHalNfcErrorNone;
}

void furi_hal_nfc_timer_fwt_start(uint32_t time_fc) {
    UNUSED(time_fc);
}

void furi_hal_nfc_timer_fwt_stop(void) {
}

void furi_hal_nfc_timer_block_tx_start(uint32_t time_fc) {
    UNUSED(time_fc);
}

void furi_hal_nfc_timer_block_tx_start_us(uint32_t time_us) {
    UNUSED(time_us);
}

void furi_hal_nfc_timer_block_tx_stop(void) {
}

bool furi_hal_nfc_timer_block_tx_is_running(void) {
    return false;
}

FuriHalNfcError furi_hal_nfc_iso14443a_poller_trx_short_frame(FuriHalNfcaShortFrame frame) {
    UNUSED(frame);
    return FuriHalNfcErrorCommunication;
}

FuriHalNfcError furi_hal_nfc_iso14443a_tx_sdd_frame(const uint8_t* tx_data, size_t tx_bits) {
    UNUSED(tx_data);
    UNUSED(tx_bits);
    return FuriHalNfcErrorCommunication;
}

FuriHalNfcError
    furi_hal_nfc_iso14443a_rx_sdd_frame(uint8_t* rx_data, size_t rx_data_size, size_t* rx_bits) {
    UNUSED(rx_data);
    UNUSED(rx_data_size);
    UNUSED(rx_bits);
    return FuriHalNfcErrorCommunication;
}

FuriHalNfcError
    furi_hal_nfc_iso14443a_poller_tx_custom_parity(const uint8_t* tx_data, size_t tx_bits) {
    UNUSED(tx_data);
    UNUSED(tx_bits);
    return FuriHalNfcErrorCommunication;
}

FuriHalNfcError furi_hal_nfc_iso14443a_listener_set_col_res_data(
    uint8_t* uid,
    uint8_t uid_len,
    uint8_t* atqa,
    uint8_t sak) {
    UNUSED(uid);
    UNUSED(uid_len);
    UNUSED(atqa);
    UNUSED(sak);
    return FuriHalNfcErrorCommunication;
}

FuriHalNfcError furi_hal_nfc_iso14443a_listener_tx_custom_parity(
    const uint8_t* tx_data,
    const uint8_t* tx_parity,
    size_t tx_bits) {
    UNUSED(tx_data);
    UNUSED(tx_parity);
    UNUSED(tx_bits);
    return FuriHalNfcErrorCommunication;
}

FuriHalNfcError furi_hal_nfc_iso15693_listener_tx_sof(void) {
    return FuriHalNfcErrorCommunication;
}

FuriHalNfcError furi_hal_nfc_iso15693_detect_mode(void) {
    return FuriHalNfcErrorCommunication;
}

FuriHalNfcError furi_hal_nfc_iso15693_force_1outof4(void) {
    return FuriHalNfcErrorCommunication;
}

FuriHalNfcError furi_hal_nfc_iso15693_force_1outof256(void) {
    return FuriHalNfcErrorCommunication;
}

FuriHalNfcError furi_hal_nfc_felica_listener_set_sensf_res_data(
    const uint8_t* idm,
    const uint8_t idm_len,
    const uint8_t* pmm,
    const uint8_t pmm_len,
    const uint16_t sys_code) {
    UNUSED(idm);
    UNUSED(idm_len);
    UNUSED(pmm);
    UNUSED(pmm_len);
    UNUSED(sys_code);
    return FuriHalNfcErrorCommunication;
}

FuriHalNfcError furi_hal_nfc_pn532_mf_auth(
    uint8_t block_num,
    const uint8_t* key,
    uint8_t key_type,
    const uint8_t* uid,
    uint8_t uid_len) {
    UNUSED(block_num);
    UNUSED(key);
    UNUSED(key_type);
    UNUSED(uid);
    UNUSED(uid_len);
    return FuriHalNfcErrorCommunication;
}

bool furi_hal_nfc_pn532_mf_is_authed(void) {
    return false;
}

void furi_hal_nfc_pn532_mf_deauth(void) {
}
