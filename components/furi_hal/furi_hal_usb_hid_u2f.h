#pragma once

#include <stdint.h>

#include <furi_hal_usb.h>

#ifdef __cplusplus
extern "C" {
#endif

/* U2F CTAP HID packets are 64 bytes (one USB HID report). */
#define HID_U2F_PACKET_LEN 64

typedef enum {
    HidU2fDisconnected = 0,
    HidU2fConnected,
    HidU2fRequest,
} HidU2fEvent;

typedef void (*HidU2fCallback)(HidU2fEvent event, void* context);

void furi_hal_hid_u2f_set_callback(HidU2fCallback callback, void* context);
uint32_t furi_hal_hid_u2f_get_request(uint8_t* buffer);
void furi_hal_hid_u2f_send_response(const uint8_t* buffer, uint16_t len);

#ifdef __cplusplus
}
#endif
