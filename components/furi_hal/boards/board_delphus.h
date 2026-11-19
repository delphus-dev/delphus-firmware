/**
 * @file board_delphus.h
 * Board definition: Delphus (ESP32-S3 with ST7567 128x64 Mono LCD)
 *
 * MCU:      ESP32-S3
 * Display:  ST7567 128x64 mono LCD via SPI2 (CS4 / RST5 / RES6 / BL45)
 * Input:    6 GPIO buttons (OK0 / Back46 / Left42 / Up41 / Down40 / Right39)
 * SubGHz:   CC1101 on shared SPI2 (CS12, GDO0 IO3)
 * SD Card:  SPI2 (CS13)
 * NFC:      ST25R3916 on SPI2 (CS47 / IRQ48) — HAL not ported yet
 * RFID:     RDM6300 UART1 (TX21 / RX48 / PULL47)
 * RGB:      single WS2812 on IO38
 * I2C:      SCL IO18 / SDA IO8
 * IR:       TX IO17 / RX IO16
 * Vibro:    IO7
 * Buzzer:   IO15
 * iButton:  IO2
 *
 * NOTE: IO47/IO48 are shared between the NFC and RFID headers.
 */

#pragma once

/* ---- Board metadata ---- */
#define BOARD_NAME        "Delphus"
#define BOARD_TARGET      "esp32s3"

/* ---- Hardware Button Pins ---- */
#define BOARD_PIN_BUTTON_BOOT   0   /* OK button / BOOT button */
#define BOARD_PIN_BUTTON_KEY    46  /* Back button */
#define BOARD_PIN_BATTERY_ADC   UINT16_MAX

/* ---- Direct 6 GPIO Buttons ---- */
#define BOARD_PIN_BTN_OK        0
#define BOARD_PIN_BTN_BACK      46
#define BOARD_PIN_BTN_LEFT      42
#define BOARD_PIN_BTN_UP        41
#define BOARD_PIN_BTN_DOWN      40
#define BOARD_PIN_BTN_RIGHT     39

/* ---- LCD Pins (ST7567 128x64 Mono LCD via SPI2) ---- */
#define BOARD_LCD_IS_ST7567     1
#define BOARD_PIN_LCD_MOSI      9
#define BOARD_PIN_LCD_SCLK      11
#define BOARD_PIN_LCD_DC        6   /* RES / DC (Data/Command) */
#define BOARD_PIN_LCD_CS        4
#define BOARD_PIN_LCD_RST       5
#define BOARD_PIN_LCD_BL        45

/* ---- LCD Display Configuration ---- */
#define BOARD_LCD_H_RES         128
#define BOARD_LCD_V_RES         64
#define BOARD_LCD_SPI_HOST      SPI2_HOST
#define BOARD_LCD_SPI_FREQ_HZ   (10 * 1000 * 1000)
#define BOARD_LCD_CMD_BITS      8
#define BOARD_LCD_PARAM_BITS    8
#define BOARD_LCD_MIRROR_X      false
#define BOARD_LCD_MIRROR_Y      false
#define BOARD_LCD_INVERT_COLOR  false
#define BOARD_LCD_GAP_X         0
#define BOARD_LCD_BL_ACTIVE_LOW false

#define BOARD_LCD_CONTRAST      26  /* Default ST7567 contrast (0..63) */

/* ---- SD Card Pins (shared SPI2 bus with LCD + CC1101 + NFC) ---- */
#define BOARD_PIN_SD_CS         13
#define BOARD_PIN_SD_MISO       10

/* ---- SubGHz / CC1101 (shared SPI2 bus with LCD + SD) ---- */
#define BOARD_PIN_CC1101_SCK    11
#define BOARD_PIN_CC1101_CSN    12
#define BOARD_PIN_CC1101_MISO   10
#define BOARD_PIN_CC1101_MOSI   9
#define BOARD_PIN_CC1101_GDO0   3
#define BOARD_PIN_CC1101_GDO2   UINT16_MAX
#define BOARD_PIN_CC1101_SW1    UINT16_MAX
#define BOARD_PIN_CC1101_SW0    UINT16_MAX
#define BOARD_CC1101_SPI_SHARED 1

/* ---- NFC: ST25R3916 on SPI2 (pinout only; HAL port is pending) ---- */
#define BOARD_PIN_NFC_MOSI      9
#define BOARD_PIN_NFC_MISO      10
#define BOARD_PIN_NFC_SCK       11
#define BOARD_PIN_NFC_CS        47
#define BOARD_PIN_NFC_IRQ       48

/* ---- NRF24L01 — NOT PRESENT (GPIO 43/44 are UART Console) ---- */
#define BOARD_PIN_NRF24_SCK     UINT16_MAX
#define BOARD_PIN_NRF24_MISO    UINT16_MAX
#define BOARD_PIN_NRF24_MOSI    UINT16_MAX
#define BOARD_PIN_NRF24_CSN     UINT16_MAX
#define BOARD_PIN_NRF24_CE      UINT16_MAX
#define BOARD_HAS_NRF24         0

/* ---- External GPIO header ----
 * 1 = MOSI (IO9), 2 = MISO (IO10), 3 = EXT CS1 (IO14),
 * 4 = SCK (IO11), 5 = EXT CS2 (IO21), 6 = EXT IO (IO1)
 */
#define BOARD_PIN_EXT_MOSI      9
#define BOARD_PIN_EXT_MISO      10
#define BOARD_PIN_EXT_SCK       11
#define BOARD_PIN_EXT_CS1       14
#define BOARD_PIN_EXT_CS2       21
#define BOARD_PIN_EXT_IO        1

/* ---- Power Enable ---- */
#define BOARD_PIN_PWR_EN        UINT16_MAX

/* ---- IR ---- */
#define BOARD_PIN_IR_TX         17
#define BOARD_PIN_IR_RX         16

/* ---- iButton ---- */
#define BOARD_PIN_IBUTTON       2

/* ---- Speaker / Buzzer ---- */
#define BOARD_PIN_SPEAKER_DOUT  15
#define BOARD_PIN_SPEAKER_BCLK  UINT16_MAX
#define BOARD_PIN_SPEAKER_WCLK  UINT16_MAX

/* ---- WS2812 RGB LED (single pixel on IO38) ---- */
#define BOARD_PIN_WS2812_DATA   38
#define BOARD_WS2812_LED_COUNT  1

/* ---- Vibro Motor ---- */
#define BOARD_PIN_VIBRO         7

/* ---- RFID: RDM6300 on UART1 (shares IO47/IO48 with NFC) ---- */
#define BOARD_HAS_RFID          1
#define BOARD_RFID_UART_NUM     1
#define BOARD_PIN_RFID_TX       21
#define BOARD_PIN_RFID_RX       48
#define BOARD_PIN_RFID_PULL     47
#define BOARD_RFID_PULL_ACTIVE_HIGH 1

/* ---- I2C (SCL / SDA) ---- */
#define BOARD_PIN_I2C_SCL       18
#define BOARD_PIN_I2C_SDA       8

/* ---- Features ---- */
#define BOARD_HAS_SD_CARD       1
#define BOARD_HAS_BLE           1
#define BOARD_HAS_RGB_LED       1
#define BOARD_HAS_VIBRO         1
#define BOARD_HAS_SPEAKER       0
#define BOARD_HAS_IR            1
#define BOARD_HAS_IBUTTON       1
#define BOARD_HAS_NFC           0   /* ST25R3916 HAL is a separate task */
#define BOARD_HAS_SUBGHZ        1
#define BOARD_HAS_MIC           0

#define BQ27220_ADDR                    0x55
#define BQ25896_CHARGE_LIMIT            1280
#define FURI_HAL_POWER_VIRTUAL_CAPACITY_MAH (1000U)
