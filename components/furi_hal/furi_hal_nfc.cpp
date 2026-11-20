/**
 * @file furi_hal_nfc.cpp
 * Furi NFC HAL implementation on ST25R3916 + ST RFAL.
 *
 * The RFAL libraries are C++ and Arduino-flavoured, so the whole bridge is
 * compiled as C++ and exposes only the plain C API from furi_hal_nfc.h.
 */

extern "C" {
#include "furi_hal_nfc.h"
}

#include "rfal_rfst25r3916.h"
#include "rfal_nfc.h"
#include <furi.h>
#include <esp_timer.h>
#include "boards/board.h"
#include "furi_hal_pin_arbiter.h"

#define TAG "FuriHalNfc"

extern "C" void nfc_platform_init(void);
extern "C" void nfc_platform_deinit(void);

namespace {

SPIClass nfc_spi;
RfalRfST25R3916Class* nfc_hw = nullptr;
bool nfc_ready = false;
FuriMutex* nfc_mutex = nullptr;
FuriEventFlag* nfc_events = nullptr;
esp_timer_handle_t fwt_timer = nullptr;
esp_timer_handle_t block_timer = nullptr;
volatile bool block_running = false;

uint8_t last_rx[256];
uint16_t last_rx_bytes = 0;

FuriHalNfcError hal_err(ReturnCode err) {
    if(err == ST_ERR_NONE) return FuriHalNfcErrorNone;
    if(err == ST_ERR_BUSY) return FuriHalNfcErrorBusy;
    if(err == ST_ERR_TIMEOUT) return FuriHalNfcErrorCommunicationTimeout;
    if(err == ST_ERR_INTERNAL) return FuriHalNfcErrorCommunication;
    return FuriHalNfcErrorCommunication;
}

void post_events(uint32_t mask) {
    if(nfc_events) furi_event_flag_set(nfc_events, mask);
}

void fwt_callback(void* arg) {
    (void)arg;
    post_events(FuriHalNfcEventTimerFwtExpired);
}

void block_callback(void* arg) {
    (void)arg;
    block_running = false;
    post_events(FuriHalNfcEventTimerBlockTxExpired);
}

void make_timers() {
    if(fwt_timer || block_timer) return;
    esp_timer_create_args_t fwt = {
        .callback = fwt_callback, .arg = nullptr, .name = "nfc_fwt"};
    esp_timer_create(&fwt, &fwt_timer);
    esp_timer_create_args_t blk = {
        .callback = block_callback, .arg = nullptr, .name = "nfc_blocktx"};
    esp_timer_create(&blk, &block_timer);
}

uint64_t fc_to_us(uint32_t fc) {
    /* 13.56 MHz carrier: 1 fc ≈ 0.0737 us */
    return ((uint64_t)fc * 1000000ULL) / 13560000ULL;
}

uint32_t wait_mask(uint32_t timeout_ms) {
    if(!nfc_events) return FuriHalNfcEventTimeout;
    uint32_t mask = 0;
    uint32_t got = furi_event_flag_wait(
        nfc_events, 0x1FFF, FuriFlagWaitAny,
        timeout_ms == FURI_HAL_NFC_EVENT_WAIT_FOREVER ? FuriWaitForever : furi_ms_to_ticks(timeout_ms));
    if(got) {
        mask = got;
        furi_event_flag_clear(nfc_events, mask);
    }
    return mask ? mask : FuriHalNfcEventTimeout;
}

} // namespace

extern "C" {

FuriHalNfcError furi_hal_nfc_init(void) {
    if(nfc_mutex) return FuriHalNfcErrorNone;
    nfc_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    nfc_events = furi_event_flag_alloc();
    make_timers();
    /* IO47/IO48 are shared with RFID: no hardware is touched at boot. */
    FURI_LOG_I(TAG, "NFC HAL ready (ST25R3916 pins on demand)");
    return FuriHalNfcErrorNone;
}

FuriHalNfcError furi_hal_nfc_is_hal_ready(void) {
    return nfc_ready ? FuriHalNfcErrorNone : FuriHalNfcErrorCommunication;
}

FuriHalNfcError furi_hal_nfc_acquire(void) {
    if(!nfc_mutex) return FuriHalNfcErrorCommunication;
    if(!furi_hal_shared_pins_claim(FuriHalSharedPinNfc)) return FuriHalNfcErrorBusy;

    if(!nfc_hw) {
        nfc_platform_init();
        nfc_hw = new RfalRfST25R3916Class(
            &nfc_spi, BOARD_PIN_NFC_CS, BOARD_PIN_NFC_IRQ, 5000000);
        ReturnCode err = nfc_hw->rfalInitialize();
        if(err != ST_ERR_NONE) {
            FURI_LOG_E(TAG, "rfalInitialize failed: 0x%04X", (unsigned)err);
            nfc_hw = nullptr;
            furi_hal_shared_pins_release(FuriHalSharedPinNfc);
            return FuriHalNfcErrorCommunication;
        }
        nfc_ready = true;
        FURI_LOG_I(TAG, "ST25R3916 initialized (CS=%d IRQ=%d)",
                   BOARD_PIN_NFC_CS, BOARD_PIN_NFC_IRQ);
    }
    nfc_hw->st25r3916OscOn();

    if(furi_mutex_acquire(nfc_mutex, FuriWaitForever) != FuriStatusOk) {
        furi_hal_shared_pins_release(FuriHalSharedPinNfc);
        return FuriHalNfcErrorBusy;
    }
    return FuriHalNfcErrorNone;
}

FuriHalNfcError furi_hal_nfc_release(void) {
    if(!nfc_mutex) return FuriHalNfcErrorCommunication;
    furi_hal_nfc_trx_reset();
    furi_mutex_release(nfc_mutex);
    furi_hal_shared_pins_release(FuriHalSharedPinNfc);
    return FuriHalNfcErrorNone;
}

FuriHalNfcError furi_hal_nfc_low_power_mode_start(void) {
    if(!nfc_hw) return FuriHalNfcErrorCommunication;
    nfc_hw->rfalFieldOff();
    return FuriHalNfcErrorNone;
}

FuriHalNfcError furi_hal_nfc_low_power_mode_stop(void) {
    if(!nfc_hw) return FuriHalNfcErrorCommunication;
    nfc_hw->st25r3916OscOn();
    return FuriHalNfcErrorNone;
}

FuriHalNfcError furi_hal_nfc_set_mode(FuriHalNfcMode mode, FuriHalNfcTech tech) {
    if(!nfc_hw) return FuriHalNfcErrorCommunication;
    rfalMode rm = RFAL_MODE_POLL_NFCA;
    rfalBitRate br = RFAL_BR_106;

    if(mode == FuriHalNfcModeListener) {
        switch(tech) {
        case FuriHalNfcTechIso14443a: rm = RFAL_MODE_LISTEN_NFCA; break;
        case FuriHalNfcTechIso14443b: rm = RFAL_MODE_LISTEN_NFCB; break;
        case FuriHalNfcTechFelica: rm = RFAL_MODE_LISTEN_NFCF; br = RFAL_BR_212; break;
        default: return FuriHalNfcErrorCommunication;
        }
    } else {
        switch(tech) {
        case FuriHalNfcTechIso14443a: rm = RFAL_MODE_POLL_NFCA; break;
        case FuriHalNfcTechIso14443b: rm = RFAL_MODE_POLL_NFCB; break;
        case FuriHalNfcTechIso15693: rm = RFAL_MODE_POLL_NFCV; break;
        case FuriHalNfcTechFelica: rm = RFAL_MODE_POLL_NFCF; br = RFAL_BR_212; break;
        default: return FuriHalNfcErrorCommunication;
        }
    }
    nfc_hw->rfalSetErrorHandling(RFAL_ERRORHANDLING_NONE);
    return hal_err(nfc_hw->rfalSetMode(rm, br, br));
}

FuriHalNfcError furi_hal_nfc_reset_mode(void) {
    if(!nfc_hw) return FuriHalNfcErrorCommunication;
    nfc_hw->rfalFieldOff();
    nfc_hw->rfalListenStop();
    last_rx_bytes = 0;
    return FuriHalNfcErrorNone;
}

FuriHalNfcError furi_hal_nfc_field_detect_start(void) {
    return nfc_hw ? FuriHalNfcErrorNone : FuriHalNfcErrorCommunication;
}

FuriHalNfcError furi_hal_nfc_field_detect_stop(void) {
    return FuriHalNfcErrorNone;
}

bool furi_hal_nfc_field_is_present(void) {
    return nfc_hw && nfc_hw->rfalIsExtFieldOn();
}

FuriHalNfcError furi_hal_nfc_poller_field_on(void) {
    if(!nfc_hw) return FuriHalNfcErrorCommunication;
    return hal_err(nfc_hw->rfalFieldOnAndStartGT());
}

FuriHalNfcEvent furi_hal_nfc_poller_wait_event(uint32_t timeout_ms) {
    return (FuriHalNfcEvent)wait_mask(timeout_ms);
}

FuriHalNfcEvent furi_hal_nfc_listener_wait_event(uint32_t timeout_ms) {
    return (FuriHalNfcEvent)wait_mask(timeout_ms);
}

FuriHalNfcError furi_hal_nfc_poller_tx(const uint8_t* tx_data, size_t tx_bits) {
    if(!nfc_hw || !tx_data) return FuriHalNfcErrorCommunication;
    uint16_t len = (uint16_t)((tx_bits + 7) / 8);
    uint16_t act = 0;
    uint32_t flags = RFAL_TXRX_FLAGS_DEFAULT | RFAL_TXRX_FLAGS_PAR_RX_KEEP;
    ReturnCode err = nfc_hw->rfalTransceiveBlockingTxRx(
        (uint8_t*)tx_data, len, last_rx, sizeof(last_rx), &act, flags, 0);
    last_rx_bytes = (act <= sizeof(last_rx)) ? act : sizeof(last_rx);
    if(err == ST_ERR_NONE) {
        post_events(FuriHalNfcEventTxEnd | FuriHalNfcEventRxStart |
                    (last_rx_bytes ? FuriHalNfcEventRxEnd : FuriHalNfcEventTimerFwtExpired));
        return FuriHalNfcErrorNone;
    }
    post_events(FuriHalNfcEventTxEnd | FuriHalNfcEventTimerFwtExpired);
    return hal_err(err);
}

FuriHalNfcError furi_hal_nfc_poller_rx(uint8_t* rx_data, size_t rx_size, size_t* rx_bits) {
    if(!rx_data || !rx_bits) return FuriHalNfcErrorCommunication;
    size_t n = last_rx_bytes < rx_size ? last_rx_bytes : rx_size;
    memcpy(rx_data, last_rx, n);
    *rx_bits = n * 8;
    return FuriHalNfcErrorNone;
}

FuriHalNfcError furi_hal_nfc_listener_tx(const uint8_t* tx_data, size_t tx_bits) {
    return furi_hal_nfc_poller_tx(tx_data, tx_bits);
}

FuriHalNfcError furi_hal_nfc_listener_rx(uint8_t* rx_data, size_t rx_size, size_t* rx_bits) {
    return furi_hal_nfc_poller_rx(rx_data, rx_size, rx_bits);
}

FuriHalNfcError furi_hal_nfc_listener_sleep(void) {
    return nfc_hw ? hal_err(nfc_hw->rfalListenSleepStart(
          RFAL_LM_STATE_SLEEP_A, last_rx, sizeof(last_rx), &last_rx_bytes))
                  : FuriHalNfcErrorCommunication;
}

FuriHalNfcError furi_hal_nfc_listener_idle(void) {
    return nfc_hw ? hal_err(nfc_hw->rfalListenSetState(RFAL_LM_STATE_IDLE))
                  : FuriHalNfcErrorCommunication;
}

FuriHalNfcError furi_hal_nfc_listener_enable_rx(void) {
    return FuriHalNfcErrorNone;
}

FuriHalNfcError furi_hal_nfc_trx_reset(void) {
    last_rx_bytes = 0;
    if(esp_timer_is_active(fwt_timer)) esp_timer_stop(fwt_timer);
    if(esp_timer_is_active(block_timer)) esp_timer_stop(block_timer);
    block_running = false;
    return FuriHalNfcErrorNone;
}

FuriHalNfcError furi_hal_nfc_event_start(void) {
    if(nfc_events) furi_event_flag_clear(nfc_events, 0x1FFF);
    return FuriHalNfcErrorNone;
}

FuriHalNfcError furi_hal_nfc_event_stop(void) {
    return FuriHalNfcErrorNone;
}

FuriHalNfcError furi_hal_nfc_abort(void) {
    post_events(FuriHalNfcEventAbortRequest);
    return FuriHalNfcErrorNone;
}

void furi_hal_nfc_timer_fwt_start(uint32_t time_fc) {
    if(!fwt_timer) return;
    esp_timer_stop(fwt_timer);
    esp_timer_start_once(fwt_timer, fc_to_us(time_fc));
}

void furi_hal_nfc_timer_fwt_stop(void) {
    if(fwt_timer) esp_timer_stop(fwt_timer);
}

void furi_hal_nfc_timer_block_tx_start(uint32_t time_fc) {
    furi_hal_nfc_timer_block_tx_start_us((uint32_t)fc_to_us(time_fc));
}

void furi_hal_nfc_timer_block_tx_start_us(uint32_t time_us) {
    if(!block_timer) return;
    esp_timer_stop(block_timer);
    block_running = true;
    esp_timer_start_once(block_timer, time_us);
}

void furi_hal_nfc_timer_block_tx_stop(void) {
    if(block_timer) esp_timer_stop(block_timer);
    block_running = false;
}

bool furi_hal_nfc_timer_block_tx_is_running(void) {
    return block_running;
}

FuriHalNfcError furi_hal_nfc_iso14443a_poller_trx_short_frame(FuriHalNfcaShortFrame frame) {
    if(!nfc_hw) return FuriHalNfcErrorCommunication;
    uint16_t len = 0;
    ReturnCode err = nfc_hw->rfalISO14443ATransceiveShortFrame(
        frame == FuriHalNfcaShortFrameAllReq ? RFAL_14443A_SHORTFRAME_CMD_WUPA
                                             : RFAL_14443A_SHORTFRAME_CMD_REQA,
        last_rx, sizeof(last_rx), &len, 0);
    last_rx_bytes = len;
    if(err == ST_ERR_NONE) post_events(FuriHalNfcEventTxEnd | FuriHalNfcEventRxStart |
                                       (len ? FuriHalNfcEventRxEnd : FuriHalNfcEventTimerFwtExpired));
    return hal_err(err);
}

FuriHalNfcError furi_hal_nfc_iso14443a_tx_sdd_frame(const uint8_t* tx_data, size_t tx_bits) {
    if(!nfc_hw || !tx_data) return FuriHalNfcErrorCommunication;
    uint8_t buf[64];
    uint8_t bytes = (uint8_t)((tx_bits + 7) / 8);
    uint8_t bits = (uint8_t)(tx_bits % 8 ? tx_bits % 8 : 8);
    memcpy(buf, tx_data, bytes);
    uint16_t len = 0;
    ReturnCode err = nfc_hw->rfalISO14443ATransceiveAnticollisionFrame(
        buf, &bytes, &bits, &len, 0);
    last_rx_bytes = len;
    if(err == ST_ERR_NONE) post_events(FuriHalNfcEventTxEnd | FuriHalNfcEventRxStart |
                                       (len ? FuriHalNfcEventRxEnd : FuriHalNfcEventTimerFwtExpired));
    return hal_err(err);
}

FuriHalNfcError furi_hal_nfc_iso14443a_poller_tx_custom_parity(const uint8_t* tx_data, size_t tx_bits) {
    if(!nfc_hw || !tx_data) return FuriHalNfcErrorCommunication;
    uint16_t len = (uint16_t)((tx_bits + 7) / 8);
    uint16_t act = 0;
    ReturnCode err = nfc_hw->rfalTransceiveBlockingTxRx(
        (uint8_t*)tx_data, len, last_rx, sizeof(last_rx), &act,
        RFAL_TXRX_FLAGS_DEFAULT | RFAL_TXRX_FLAGS_PAR_TX_NONE | RFAL_TXRX_FLAGS_PAR_RX_KEEP, 0);
    last_rx_bytes = act;
    post_events(err == ST_ERR_NONE
                    ? (FuriHalNfcEventTxEnd | FuriHalNfcEventRxStart |
                       (act ? FuriHalNfcEventRxEnd : FuriHalNfcEventTimerFwtExpired))
                    : (FuriHalNfcEventTxEnd | FuriHalNfcEventTimerFwtExpired));
    return hal_err(err);
}

FuriHalNfcError furi_hal_nfc_iso14443a_listener_set_col_res_data(
    uint8_t* uid, uint8_t uid_len, uint8_t* atqa, uint8_t sak) {
    if(!nfc_hw || !uid || !atqa) return FuriHalNfcErrorCommunication;
    rfalLmConfPA conf = {};
    conf.nfcidLen = static_cast<rfalLmNfcidLen>(uid_len);
    memcpy(conf.nfcid, uid, uid_len);
    conf.SENS_RES[0] = atqa[0];
    conf.SENS_RES[1] = atqa[1];
    conf.SEL_RES = sak;
    return hal_err(nfc_hw->rfalListenStart(
        RFAL_LM_MASK_NFCA, &conf, nullptr, nullptr, last_rx, sizeof(last_rx), &last_rx_bytes));
}

FuriHalNfcError furi_hal_nfc_iso14443a_listener_tx_custom_parity(
    const uint8_t* tx_data, const uint8_t*, size_t tx_bits) {
    return furi_hal_nfc_iso14443a_poller_tx_custom_parity(tx_data, tx_bits);
}

FuriHalNfcError furi_hal_nfc_iso14443a_rx_sdd_frame(uint8_t*, size_t, size_t*) {
    return FuriHalNfcErrorCommunication;
}

FuriHalNfcError furi_hal_nfc_iso15693_listener_tx_sof(void) {
    if(!nfc_hw) return FuriHalNfcErrorCommunication;
    uint16_t len = 0;
    ReturnCode err = nfc_hw->rfalISO15693TransceiveEOF(last_rx, sizeof(last_rx), &len);
    last_rx_bytes = len;
    return hal_err(err);
}

FuriHalNfcError furi_hal_nfc_iso15693_detect_mode(void) { return FuriHalNfcErrorNone; }
FuriHalNfcError furi_hal_nfc_iso15693_force_1outof4(void) { return FuriHalNfcErrorNone; }
FuriHalNfcError furi_hal_nfc_iso15693_force_1outof256(void) { return FuriHalNfcErrorNone; }

FuriHalNfcError furi_hal_nfc_felica_listener_set_sensf_res_data(
    const uint8_t* idm, const uint8_t idm_len, const uint8_t* pmm, const uint8_t pmm_len,
    const uint16_t sys_code) {
    if(!nfc_hw || !idm || !pmm) return FuriHalNfcErrorCommunication;
    rfalLmConfPF conf = {};
    memcpy(conf.SENSF_RES, idm, idm_len < 8 ? idm_len : 8);
    conf.SC[0] = (uint8_t)(sys_code >> 8);
    conf.SC[1] = (uint8_t)sys_code;
    (void)pmm;
    (void)pmm_len;
    return hal_err(nfc_hw->rfalListenStart(
        RFAL_LM_MASK_NFCF, nullptr, nullptr, &conf, last_rx, sizeof(last_rx), &last_rx_bytes));
}

void furi_hal_nfc_emu_set_ndef(const uint8_t* msg, size_t len) {
    (void)msg;
    (void)len;
}

/* Legacy PN532 helpers are intentionally unsupported on this board. */
FuriHalNfcError furi_hal_nfc_pn532_mf_auth(uint8_t, const uint8_t*, uint8_t, const uint8_t*, uint8_t) {
    return FuriHalNfcErrorCommunication;
}
bool furi_hal_nfc_pn532_mf_is_authed(void) { return false; }
void furi_hal_nfc_pn532_mf_deauth(void) {}

} // extern "C"
