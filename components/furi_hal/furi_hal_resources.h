/**
 * @file furi_hal_resources.h
 * Resources HAL API (ESP32 stub)
 */

#pragma once

#include <stddef.h>
#include <furi_hal_gpio.h>
#include <input.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Input Related Constants */
#define INPUT_DEBOUNCE_TICKS 4

/* Light */
typedef enum {
    LightRed = (1 << 0),
    LightGreen = (1 << 1),
    LightBlue = (1 << 2),
    LightBacklight = (1 << 3),
} Light;

typedef struct {
    const GpioPin* gpio;
    const InputKey key;
    const bool inverted;
    const char* name;
} InputPin;

extern const InputPin input_pins[];
extern const size_t input_pins_count;

/* Hardware buttons / battery ADC — actual pins come from board_delphus.h.
 * The Delphus board has no battery ADC (gpio_battery_sense is UINT16_MAX). */
extern const GpioPin gpio_button_boot;
extern const GpioPin gpio_battery_sense;

/* LCD pins (ST7567, SPI2_HOST) */
extern const GpioPin gpio_lcd_din;  /* SPI MOSI */
extern const GpioPin gpio_lcd_clk;  /* SPI SCLK */
extern const GpioPin gpio_lcd_dc;   /* RES / Data-Command */
extern const GpioPin gpio_lcd_cs;   /* Chip Select */
extern const GpioPin gpio_lcd_rst;  /* Reset */
extern const GpioPin gpio_lcd_bl;   /* Backlight */

/* SD Card pins (shared SPI2_HOST) */
extern const GpioPin gpio_sdcard_cs;
extern const GpioPin gpio_sdcard_miso;

/* Delphus external header:
 * PA7=MOSI(9), PA6=MISO(10), PA4=EXT CS1(14),
 * PB3=SCK(11), PB2=EXT CS2(21), PC3=EXT IO(1). */
extern const GpioPin gpio_ext_pc0;
extern const GpioPin gpio_ext_pc1;
extern const GpioPin gpio_ext_pc3; /* EXT IO  - GPIO1 */
extern const GpioPin gpio_ext_pb2; /* EXT CS2 - GPIO21 */
extern const GpioPin gpio_ext_pb3; /* SCK     - GPIO11 */
extern const GpioPin gpio_ext_pa4; /* EXT CS1 - GPIO14 */
extern const GpioPin gpio_ext_pa6; /* MISO    - GPIO10 */
extern const GpioPin gpio_ext_pa7; /* MOSI    - GPIO9 */
extern const GpioPin gpio_cc1101_cs;  /* CC1101 CSN - GPIO12 */
extern const GpioPin gpio_cc1101_g0;  /* CC1101 GDO0 - GPIO3 */
extern const GpioPin gpio_nrf24_cs; /* NRF24 CSN -- T-Embed: GPIO44 */
extern const GpioPin gpio_ibutton;
extern const GpioPin gpio_speaker;

#ifdef __cplusplus
}
#endif
