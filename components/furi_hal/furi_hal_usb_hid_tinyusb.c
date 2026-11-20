#include "furi_hal_usb.h"
#include "furi_hal_usb_hid.h"
#include "furi_hal_usb_hid_backend.h"
#include "furi_hal_usb_hid_u2f.h"

#include <furi.h>
#include <string.h>
#include <stdio.h>

#include "tinyusb.h"
#include "class/hid/hid_device.h"

#define TAG "FuriHalUsbHid"

#define REPORT_ID_KEYBOARD 1
#define REPORT_ID_MOUSE    2
#define REPORT_ID_CONSUMER 3
#define REPORT_ID_U2F      4

#define HID_EP_IN       0x81
#define HID_EP_BUF_SIZE HID_U2F_PACKET_LEN
#define HID_POLL_MS     5

static const uint8_t hid_report_descriptor[] = {
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(REPORT_ID_KEYBOARD)),
    TUD_HID_REPORT_DESC_MOUSE(HID_REPORT_ID(REPORT_ID_MOUSE)),
    TUD_HID_REPORT_DESC_CONSUMER(HID_REPORT_ID(REPORT_ID_CONSUMER)),
    /* U2F CTAP: 64-byte vendor IN/OUT report */
    0x06, 0x00, 0xFF, /* Usage Page (Vendor Defined 0xFF00) */
    0x09, 0x01,       /* Usage (0x01) */
    0xA1, 0x01,       /* Collection (Application) */
    0x85, REPORT_ID_U2F, /*   Report ID (4) */
    0x75, 0x08,       /*   Report Size (8) */
    0x95, HID_U2F_PACKET_LEN, /*   Report Count (64) */
    0x09, 0x01,       /*   Usage (0x01) */
    0x81, 0x02,       /*   Input (Data,Var,Abs) */
    0x09, 0x01,       /*   Usage (0x01) */
    0x91, 0x02,       /*   Output (Data,Var,Abs) */
    0xC0,             /* End Collection */
};

#define HID_CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN)

static const uint8_t hid_configuration_descriptor[] = {
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, HID_CONFIG_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    TUD_HID_DESCRIPTOR(
        0,
        4,
        false,
        sizeof(hid_report_descriptor),
        HID_EP_IN,
        HID_EP_BUF_SIZE,
        HID_POLL_MS),
};

static tusb_desc_device_t hid_device_descriptor = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = 0,
    .bDeviceSubClass = 0,
    .bDeviceProtocol = 0,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = HID_VID_DEFAULT,
    .idProduct = HID_PID_DEFAULT,
    .bcdDevice = 0x0100,
    .iManufacturer = 1,
    .iProduct = 2,
    .iSerialNumber = 3,
    .bNumConfigurations = 1,
};

static char s_manuf[HID_MANUF_PRODUCT_NAME_LEN + 1];
static char s_product[HID_MANUF_PRODUCT_NAME_LEN + 1];
static char s_serial[17];
static const char* s_string_descriptor[5];

typedef struct {
    bool installed;
    bool mounted;
    uint8_t led_state;
    uint8_t modifiers;
    uint8_t keys[HID_KB_MAX_KEYS];
    uint8_t mouse_buttons;
    uint16_t consumer[HID_CONSUMER_MAX_KEYS];
} HidState;

static HidState s_state = {0};
static FuriMutex* s_state_mutex = NULL;
static HidStateCallback s_user_cb = NULL;
static void* s_user_ctx = NULL;

/* U2F HID transport */
static uint8_t s_u2f_request[HID_U2F_PACKET_LEN];
static volatile uint16_t s_u2f_request_len = 0;
static HidU2fCallback s_u2f_cb = NULL;
static void* s_u2f_ctx = NULL;

static void hid_state_lock(void) {
    if(s_state_mutex) furi_mutex_acquire(s_state_mutex, FuriWaitForever);
}

static void hid_state_unlock(void) {
    if(s_state_mutex) furi_mutex_release(s_state_mutex);
}

static void hid_publish_mount(bool mounted) {
    s_state.mounted = mounted;
    if(!mounted) {
        memset(s_state.keys, 0, sizeof(s_state.keys));
        memset(s_state.consumer, 0, sizeof(s_state.consumer));
        s_state.modifiers = 0;
        s_state.mouse_buttons = 0;
        s_state.led_state = 0;
    }
    if(s_user_cb) s_user_cb(mounted, s_user_ctx);
}

/* TinyUSB mount callbacks - invoked from TinyUSB task */
void tud_mount_cb(void) {
    hid_state_lock();
    hid_publish_mount(true);
    hid_state_unlock();
    if(s_u2f_cb) s_u2f_cb(HidU2fConnected, s_u2f_ctx);
}

void tud_umount_cb(void) {
    hid_state_lock();
    hid_publish_mount(false);
    hid_state_unlock();
    s_u2f_request_len = 0;
    if(s_u2f_cb) s_u2f_cb(HidU2fDisconnected, s_u2f_ctx);
}

void tud_suspend_cb(bool remote_wakeup_en) {
    (void)remote_wakeup_en;
    hid_state_lock();
    hid_publish_mount(false);
    hid_state_unlock();
}

void tud_resume_cb(void) {
    hid_state_lock();
    hid_publish_mount(tud_mounted());
    hid_state_unlock();
}

/* TinyUSB HID callbacks */
uint8_t const* tud_hid_descriptor_report_cb(uint8_t instance) {
    (void)instance;
    return hid_report_descriptor;
}

/* Exposed for the Composite Device (HID + CDC + MSC) descriptor in
 * furi_hal_usb_tinyusb_composite.c which needs to patch wDescriptorLength. */
const uint8_t* furi_hal_usb_hid_report_desc(size_t* out_len) {
    if(out_len) *out_len = sizeof(hid_report_descriptor);
    return hid_report_descriptor;
}

uint16_t tud_hid_get_report_cb(
    uint8_t instance,
    uint8_t report_id,
    hid_report_type_t report_type,
    uint8_t* buffer,
    uint16_t reqlen) {
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)reqlen;
    return 0;
}

void tud_hid_set_report_cb(
    uint8_t instance,
    uint8_t report_id,
    hid_report_type_t report_type,
    uint8_t const* buffer,
    uint16_t bufsize) {
    (void)instance;
    if(report_type == HID_REPORT_TYPE_OUTPUT && report_id == REPORT_ID_KEYBOARD &&
       bufsize >= 1) {
        s_state.led_state = buffer[0];
    }

    if(report_type == HID_REPORT_TYPE_OUTPUT && report_id == REPORT_ID_U2F && bufsize >= 1) {
        uint16_t len = bufsize > HID_U2F_PACKET_LEN ? HID_U2F_PACKET_LEN : bufsize;
        memcpy(s_u2f_request, buffer, len);
        s_u2f_request_len = len;
        if(s_u2f_cb) s_u2f_cb(HidU2fRequest, s_u2f_ctx);
    }
}

void furi_hal_hid_u2f_set_callback(HidU2fCallback callback, void* context) {
    s_u2f_cb = callback;
    s_u2f_ctx = context;
}

uint32_t furi_hal_hid_u2f_get_request(uint8_t* buffer) {
    if(!buffer || s_u2f_request_len == 0) return 0;
    uint16_t len = s_u2f_request_len;
    memcpy(buffer, s_u2f_request, len);
    s_u2f_request_len = 0;
    return len;
}

void furi_hal_hid_u2f_send_response(const uint8_t* buffer, uint16_t len) {
    if(!buffer || !len) return;
    uint32_t start = furi_get_tick();
    while(s_state.mounted && !tud_hid_ready()) {
        if(furi_get_tick() - start > 100) return;
        furi_delay_tick(1);
    }
    if(s_state.mounted) tud_hid_report(REPORT_ID_U2F, buffer, len);
}

/* Backend start/stop - called from furi_hal_usb.c */
bool furi_hal_usb_hid_backend_start(const FuriHalUsbHidConfig* cfg) {
    if(s_state.installed) {
        /* Already installed. This happens when re-entering BadUsb after a config-
         * menu visit: leaving the work scene calls set_config(NULL) -> backend_stop,
         * which sets mounted=false, and returning calls set_config(usb_hid) ->
         * backend_start. The TinyUSB stack and the physical USB connection stay up
         * the whole time (we never tear the stack down on this port), so resync our
         * mounted flag with the real device state instead of leaving it stuck at
         * false — otherwise BadUsb shows "Connect to device" until a USB replug. */
        hid_state_lock();
        hid_publish_mount(tud_mounted());
        hid_state_unlock();
        return true;
    }

    if(!s_state_mutex) {
        s_state_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    }

    const char* manuf = (cfg && cfg->manuf[0]) ? cfg->manuf : "Flipper Devices Inc.";
    const char* product = (cfg && cfg->product[0]) ? cfg->product : "Flipper Zero";
    uint16_t vid = (cfg && cfg->vid) ? (uint16_t)cfg->vid : HID_VID_DEFAULT;
    uint16_t pid = (cfg && cfg->pid) ? (uint16_t)cfg->pid : HID_PID_DEFAULT;

    snprintf(s_manuf, sizeof(s_manuf), "%s", manuf);
    snprintf(s_product, sizeof(s_product), "%s", product);
    snprintf(s_serial, sizeof(s_serial), "FZESP32");

    hid_device_descriptor.idVendor = vid;
    hid_device_descriptor.idProduct = pid;

    s_string_descriptor[0] = (const char[]){0x09, 0x04};
    s_string_descriptor[1] = s_manuf;
    s_string_descriptor[2] = s_product;
    s_string_descriptor[3] = s_serial;
    s_string_descriptor[4] = "HID";

    tinyusb_config_t tusb_cfg = {
        .device_descriptor = &hid_device_descriptor,
        .string_descriptor = s_string_descriptor,
        .string_descriptor_count =
            sizeof(s_string_descriptor) / sizeof(s_string_descriptor[0]),
        .external_phy = false,
#if (TUD_OPT_HIGH_SPEED)
        .fs_configuration_descriptor = hid_configuration_descriptor,
        .hs_configuration_descriptor = hid_configuration_descriptor,
        .qualifier_descriptor = NULL,
#else
        .configuration_descriptor = hid_configuration_descriptor,
#endif
    };

    esp_err_t err = tinyusb_driver_install(&tusb_cfg);
    if(err != ESP_OK) {
        FURI_LOG_E(TAG, "tinyusb_driver_install failed: %d", err);
        return false;
    }

    s_state.installed = true;
    FURI_LOG_I(TAG, "TinyUSB HID installed vid=%04x pid=%04x", vid, pid);
    return true;
}

void furi_hal_usb_hid_backend_stop(void) {
    /* esp_tinyusb in ESP-IDF v5 does not expose a reliable uninstall.
     * We reset state and leave the stack running; re-entry to usb_hid
     * short-circuits via the installed flag. */
    hid_state_lock();
    if(s_state.mounted) hid_publish_mount(false);
    hid_state_unlock();
}

/* Public HID API */
bool furi_hal_hid_is_connected(void) {
    return s_state.mounted;
}

uint8_t furi_hal_hid_get_led_state(void) {
    return s_state.led_state;
}

void furi_hal_hid_set_state_callback(HidStateCallback cb, void* ctx) {
    s_user_cb = cb;
    s_user_ctx = ctx;
    if(cb) cb(s_state.mounted, ctx);
}

/* Wait until the HID IN endpoint can accept a new report.
 *
 * tud_hid_*_report() only queues into the endpoint FIFO; the report is "in
 * flight" (tud_hid_ready() == false) until the host polls it (every HID_POLL_MS).
 * BadUsb fires kb_press() immediately followed by kb_release() with no delay, so
 * without this wait the release report is dropped while the press is still in
 * flight -> the host never sees key-up -> auto-repeat / garbled output. This is
 * especially visible on the composite device (HID shares the bus with CDC+MSC).
 *
 * Must be called with the state lock held; the lock is released transiently
 * while delaying so TinyUSB mount/umount callbacks can still run. Returns true
 * if the endpoint is ready to accept a report. */
static bool hid_wait_tx_ready_locked(void) {
    uint32_t timeout_ms = 100;
    while(s_state.mounted && !tud_hid_ready()) {
        if(timeout_ms-- == 0) break;
        hid_state_unlock();
        furi_delay_ms(1);
        hid_state_lock();
    }
    return s_state.mounted && tud_hid_ready();
}

static bool send_keyboard_report_locked(void) {
    if(!hid_wait_tx_ready_locked()) return false;
    return tud_hid_keyboard_report(REPORT_ID_KEYBOARD, s_state.modifiers, s_state.keys);
}

bool furi_hal_hid_kb_press(uint16_t button) {
    uint8_t keycode = button & 0xFF;
    uint8_t mods = (button >> 8) & 0xFF;

    hid_state_lock();
    s_state.modifiers |= mods;
    if(keycode) {
        bool present = false;
        for(int i = 0; i < HID_KB_MAX_KEYS; i++) {
            if(s_state.keys[i] == keycode) {
                present = true;
                break;
            }
        }
        if(!present) {
            for(int i = 0; i < HID_KB_MAX_KEYS; i++) {
                if(s_state.keys[i] == 0) {
                    s_state.keys[i] = keycode;
                    break;
                }
            }
        }
    }
    bool result = send_keyboard_report_locked();
    hid_state_unlock();
    return result;
}

bool furi_hal_hid_kb_release(uint16_t button) {
    uint8_t keycode = button & 0xFF;
    uint8_t mods = (button >> 8) & 0xFF;

    hid_state_lock();
    s_state.modifiers &= ~mods;
    if(keycode) {
        uint8_t compact[HID_KB_MAX_KEYS] = {0};
        int idx = 0;
        for(int i = 0; i < HID_KB_MAX_KEYS; i++) {
            if(s_state.keys[i] && s_state.keys[i] != keycode) {
                compact[idx++] = s_state.keys[i];
            }
        }
        memcpy(s_state.keys, compact, sizeof(s_state.keys));
    }
    bool result = send_keyboard_report_locked();
    hid_state_unlock();
    return result;
}

bool furi_hal_hid_kb_release_all(void) {
    hid_state_lock();
    s_state.modifiers = 0;
    memset(s_state.keys, 0, sizeof(s_state.keys));
    bool result = send_keyboard_report_locked();
    hid_state_unlock();
    return result;
}

static bool send_mouse_report_locked(int8_t dx, int8_t dy, int8_t scroll) {
    if(!hid_wait_tx_ready_locked()) return false;
    return tud_hid_mouse_report(
        REPORT_ID_MOUSE, s_state.mouse_buttons, dx, dy, scroll, 0);
}

bool furi_hal_hid_mouse_move(int8_t dx, int8_t dy) {
    hid_state_lock();
    bool result = send_mouse_report_locked(dx, dy, 0);
    hid_state_unlock();
    return result;
}

bool furi_hal_hid_mouse_press(uint8_t button) {
    hid_state_lock();
    s_state.mouse_buttons |= button;
    bool result = send_mouse_report_locked(0, 0, 0);
    hid_state_unlock();
    return result;
}

bool furi_hal_hid_mouse_release(uint8_t button) {
    hid_state_lock();
    s_state.mouse_buttons &= ~button;
    bool result = send_mouse_report_locked(0, 0, 0);
    hid_state_unlock();
    return result;
}

bool furi_hal_hid_mouse_scroll(int8_t delta) {
    hid_state_lock();
    bool result = send_mouse_report_locked(0, 0, delta);
    hid_state_unlock();
    return result;
}

static bool send_consumer_report_locked(void) {
    if(!hid_wait_tx_ready_locked()) return false;
    /* Standard TUD_HID_REPORT_DESC_CONSUMER emits one 16-bit usage.
     * We pick the most-recently pressed key that is still active. */
    uint16_t usage = 0;
    for(int i = HID_CONSUMER_MAX_KEYS - 1; i >= 0; i--) {
        if(s_state.consumer[i]) {
            usage = s_state.consumer[i];
            break;
        }
    }
    return tud_hid_report(REPORT_ID_CONSUMER, &usage, sizeof(usage));
}

bool furi_hal_hid_consumer_key_press(uint16_t button) {
    hid_state_lock();
    bool already = false;
    for(int i = 0; i < HID_CONSUMER_MAX_KEYS; i++) {
        if(s_state.consumer[i] == button) {
            already = true;
            break;
        }
    }
    if(!already) {
        for(int i = 0; i < HID_CONSUMER_MAX_KEYS; i++) {
            if(s_state.consumer[i] == 0) {
                s_state.consumer[i] = button;
                break;
            }
        }
    }
    bool result = send_consumer_report_locked();
    hid_state_unlock();
    return result;
}

bool furi_hal_hid_consumer_key_release(uint16_t button) {
    hid_state_lock();
    for(int i = 0; i < HID_CONSUMER_MAX_KEYS; i++) {
        if(s_state.consumer[i] == button) s_state.consumer[i] = 0;
    }
    bool result = send_consumer_report_locked();
    hid_state_unlock();
    return result;
}

bool furi_hal_hid_consumer_key_release_all(void) {
    hid_state_lock();
    memset(s_state.consumer, 0, sizeof(s_state.consumer));
    bool result = send_consumer_report_locked();
    hid_state_unlock();
    return result;
}
