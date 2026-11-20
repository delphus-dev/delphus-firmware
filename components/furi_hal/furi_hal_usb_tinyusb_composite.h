#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Lazy one-shot installer for the TinyUSB Composite Device used on
 * ESP32-S3-class boards (HID + CDC-ACM with IAD).
 *
 * `esp_tinyusb` does not expose a reliable `uninstall`. Once installed, the
 * Composite stays up for the rest of the boot cycle. VID/PID/strings can only
 * be set on the *first* call; later calls return true but log a warning.
 *
 * Returns true if installed (either now or previously). Pass NULLs/zeros for
 * fields that should fall back to defaults.
 */
bool furi_hal_usb_composite_install(
    uint16_t vid,
    uint16_t pid,
    const char* manuf,
    const char* product);

bool furi_hal_usb_composite_is_installed(void);

/**
 * Tear down the Composite and route the internal USB FSLS PHY back to the
 * USB-Serial-JTAG controller, restoring the flash/console port without a
 * reboot. ESP32-S3/S2 only; returns false on boards without USB-OTG.
 *
 * Full symmetric teardown: deinits TinyUSB, frees the CDC-ACM wrapper (so a
 * later install re-initialises CDC cleanly), deletes the OTG PHY and routes the
 * shared PHY back to USJ. Re-install afterwards works without a reboot.
 */
bool furi_hal_usb_composite_uninstall(void);

/** Route the shared internal USB FSLS PHY back to USB-Serial-JTAG without
 *  touching a possibly-installed TinyUSB stack. The mux is in the RTC always-on
 *  domain and survives a soft reset, so this must be called at boot to recover
 *  the flash/console port after a composite was active on a previous boot. */
void furi_hal_usb_composite_restore_serial_jtag(void);

/** Raw USB-Serial-JTAG console access (only while the composite is NOT
 *  installed). Returns number of bytes read/written, 0 otherwise. */
size_t furi_hal_usb_serial_jtag_read(uint8_t* buf, size_t len);
size_t furi_hal_usb_serial_jtag_write(const uint8_t* buf, size_t len);

#ifdef __cplusplus
}
#endif
