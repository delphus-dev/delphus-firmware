/**
 * @file furi_hal_resources.c
 * Resources HAL — pin definitions driven by board header
 */

#include "furi_hal_resources.h"
#include "boards/board.h"

const InputPin input_pins[] = {};
const size_t input_pins_count = 0;

/* Hardware Button Pins */
const GpioPin gpio_button_boot  = {.port = NULL, .pin = BOARD_PIN_BUTTON_BOOT};
const GpioPin gpio_battery_sense = {.port = NULL, .pin = BOARD_PIN_BATTERY_ADC};

/* LCD Pins */
const GpioPin gpio_lcd_din = {.port = NULL, .pin = BOARD_PIN_LCD_MOSI};
const GpioPin gpio_lcd_clk = {.port = NULL, .pin = BOARD_PIN_LCD_SCLK};
const GpioPin gpio_lcd_dc  = {.port = NULL, .pin = BOARD_PIN_LCD_DC};
const GpioPin gpio_lcd_cs  = {.port = NULL, .pin = BOARD_PIN_LCD_CS};
const GpioPin gpio_lcd_rst = {.port = NULL, .pin = BOARD_PIN_LCD_RST};
const GpioPin gpio_lcd_bl  = {.port = NULL, .pin = BOARD_PIN_LCD_BL};

/* SD Card Pins */
const GpioPin gpio_sdcard_cs   = {.port = NULL, .pin = BOARD_PIN_SD_CS};
const GpioPin gpio_sdcard_miso = {.port = NULL, .pin = BOARD_PIN_SD_MISO};

/* Delphus external header:
 * PA7=MOSI(9), PA6=MISO(10), PA4=EXT CS1(14), PB3=SCK(11),
 * PB2=EXT CS2(21), PC3=EXT IO(1).
 * CC1101 CS is a separate pin (12) — do not alias it to the header CS. */
const GpioPin gpio_ext_pc0  = {.port = NULL, .pin = UINT16_MAX};
const GpioPin gpio_ext_pc1  = {.port = NULL, .pin = UINT16_MAX};
const GpioPin gpio_ext_pc3  = {.port = NULL, .pin = BOARD_PIN_EXT_IO};
const GpioPin gpio_ext_pb2  = {.port = NULL, .pin = BOARD_PIN_EXT_CS2};
const GpioPin gpio_ext_pb3  = {.port = NULL, .pin = BOARD_PIN_EXT_SCK};
const GpioPin gpio_ext_pa4  = {.port = NULL, .pin = BOARD_PIN_EXT_CS1};
const GpioPin gpio_ext_pa6  = {.port = NULL, .pin = BOARD_PIN_EXT_MISO};
const GpioPin gpio_ext_pa7  = {.port = NULL, .pin = BOARD_PIN_EXT_MOSI};
const GpioPin gpio_cc1101_cs = {.port = NULL, .pin = BOARD_PIN_CC1101_CSN};
const GpioPin gpio_cc1101_g0 = {.port = NULL, .pin = BOARD_PIN_CC1101_GDO0};

#ifdef BOARD_PIN_NRF24_CSN
const GpioPin gpio_nrf24_cs = {.port = NULL, .pin = BOARD_PIN_NRF24_CSN};
#else
const GpioPin gpio_nrf24_cs = {.port = NULL, .pin = UINT16_MAX};
#endif
const GpioPin gpio_ibutton  = {.port = NULL, .pin = UINT16_MAX};
const GpioPin gpio_speaker  = {.port = NULL, .pin = UINT16_MAX};
