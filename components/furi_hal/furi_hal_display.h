/**
 * @file furi_hal_display.h
 * Display HAL API (Delphus, ST7567 128x64 mono via esp_lcd panel IO)
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <esp_lcd_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Initialize the ST7567 128x64 mono display (SPI + esp_lcd panel IO).
 */
void furi_hal_display_init(void);

/** Send the u8g2 128x64 1bpp framebuffer to the ST7567 as-is (8 pages x
 *  128 bytes). No scaling, margins or colour conversion.
 *
 * @param      data  pointer to u8g2 framebuffer data (1024 bytes)
 * @param      size  size of framebuffer data in bytes (unused)
 */
void furi_hal_display_commit(const uint8_t* data, uint32_t size);

/** Set display backlight brightness
 *
 * @param      brightness  brightness level [0-255]
 */
void furi_hal_display_set_backlight(uint8_t brightness);

/** Check if display backlight is currently turned on
 *
 * @return     true if backlight brightness > 0, false if off / asleep
 */
bool furi_hal_display_is_backlight_on(void);

/** Check if display supports contrast adjustment (e.g. ST7567) */
bool furi_hal_display_has_contrast(void);

/** Set display contrast (for monochrome displays like ST7567)
 *
 * @param      contrast  contrast value [0..63] (0x00..0x3F)
 */
void furi_hal_display_set_contrast(uint8_t contrast);

/** Get current display contrast
 *
 * @return     contrast value [0..63]
 */
uint8_t furi_hal_display_get_contrast(void);

/** Put the display into sleep mode (ST7567 display off).
 *
 * Intended for the power-off / deep-sleep path. Safe to call without holding
 * the SPI bus lock — it grabs the lock internally.
 */
void furi_hal_display_sleep(void);

/** Get the native display dimensions (128x64).
 */
uint16_t furi_hal_display_get_h_res(void);
uint16_t furi_hal_display_get_v_res(void);

/** Kept for API compatibility: always returns NULL because the ST7567 is
 * driven via esp_lcd panel IO and has no esp_lcd panel handle. Callers must
 * check for NULL before using it.
 */
esp_lcd_panel_handle_t furi_hal_display_get_panel_handle(void);

#ifdef __cplusplus
}
#endif
