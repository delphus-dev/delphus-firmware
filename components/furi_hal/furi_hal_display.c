/**
 * @file furi_hal_display.c
 * Display HAL for Delphus — ST7567 128x64 mono panel over SPI/esp_lcd.
 *
 * The Flipper 128x64 1bpp framebuffer is sent page-by-page as-is: no LUT
 * scaling, centering margins or RGB565 conversion.
 */

#include "furi_hal_display.h"
#include "furi_hal_light.h"
#include "furi_hal_resources.h"
#include "furi_hal_spi_bus.h"
#include "boards/board.h"

#include <string.h>
#include <esp_log.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <driver/gpio.h>
#include <driver/spi_master.h>
#include <esp_lcd_panel_io.h>
#include <freertos/semphr.h>

static const char* TAG = "FuriHalDisplay";

#define LCD_H_RES BOARD_LCD_H_RES
#define LCD_V_RES BOARD_LCD_V_RES
#define FB_WIDTH  128

#define LCD_SPI_HOST   BOARD_LCD_SPI_HOST
#define LCD_SPI_FREQ   BOARD_LCD_SPI_FREQ_HZ
#define LCD_CMD_BITS   BOARD_LCD_CMD_BITS
#define LCD_PARAM_BITS BOARD_LCD_PARAM_BITS

#ifndef BOARD_LCD_CONTRAST
#define BOARD_LCD_CONTRAST 26
#endif

static esp_lcd_panel_io_handle_t st7567_io_handle = NULL;
static uint8_t* st7567_dma_buf = NULL;
static uint8_t st7567_contrast = BOARD_LCD_CONTRAST;
static SemaphoreHandle_t lcd_flush_done = NULL;

static bool furi_hal_display_flush_done_callback(
    esp_lcd_panel_io_handle_t panel_io,
    esp_lcd_panel_io_event_data_t* edata,
    void* user_ctx) {
    UNUSED(panel_io);
    UNUSED(edata);
    UNUSED(user_ctx);

    BaseType_t yield = pdFALSE;
    if(lcd_flush_done) {
        xSemaphoreGiveFromISR(lcd_flush_done, &yield);
    }

    return yield == pdTRUE;
}

static void furi_hal_display_prepare_flush(void) {
    if(!lcd_flush_done) return;
    xSemaphoreTake(lcd_flush_done, 0);
}

static void furi_hal_display_wait_flush(void) {
    if(!lcd_flush_done) return;
    if(xSemaphoreTake(lcd_flush_done, pdMS_TO_TICKS(250)) != pdTRUE) {
        ESP_LOGW(TAG, "Timed out waiting for LCD flush");
    }
}

void furi_hal_display_init(void) {
    ESP_LOGI(TAG, "Initializing ST7567 display for %s", BOARD_NAME);
    furi_hal_spi_bus_init();

    if(!lcd_flush_done) {
        lcd_flush_done = xSemaphoreCreateBinary();
        ESP_ERROR_CHECK(lcd_flush_done ? ESP_OK : ESP_ERR_NO_MEM);
    }

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = gpio_lcd_din.pin,
        .miso_io_num = gpio_sdcard_miso.pin,
        .sclk_io_num = gpio_lcd_clk.pin,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 2048, /* one 128-byte page plus command overhead */
    };
    esp_err_t spi_err = spi_bus_initialize(LCD_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    /* ESP_ERR_INVALID_STATE is OK — the bus may already be initialized by
     * SubGHz on the shared SPI bus. */
    if(spi_err != ESP_OK && spi_err != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(spi_err);
    }

    /* Hardware reset pulse on RESX */
    if(gpio_lcd_rst.pin != UINT16_MAX) {
        gpio_config_t rst_cfg = {
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = 1ULL << gpio_lcd_rst.pin,
        };
        gpio_config(&rst_cfg);
        gpio_set_level((gpio_num_t)gpio_lcd_rst.pin, 1);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level((gpio_num_t)gpio_lcd_rst.pin, 0);
        vTaskDelay(pdMS_TO_TICKS(20));
        gpio_set_level((gpio_num_t)gpio_lcd_rst.pin, 1);
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    esp_lcd_panel_io_spi_config_t st7567_io_cfg = {
        .dc_gpio_num = gpio_lcd_dc.pin,
        .cs_gpio_num = gpio_lcd_cs.pin,
        .pclk_hz = LCD_SPI_FREQ,
        .lcd_cmd_bits = LCD_CMD_BITS,
        .lcd_param_bits = LCD_PARAM_BITS,
        .spi_mode = 0,
        .trans_queue_depth = 16,
        .on_color_trans_done = furi_hal_display_flush_done_callback,
        .user_ctx = NULL,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(LCD_SPI_HOST, &st7567_io_cfg, &st7567_io_handle));

    /* ST7567 initialization command sequence */
    esp_lcd_panel_io_tx_param(st7567_io_handle, 0xE2 /* Soft Reset */, NULL, 0);
    vTaskDelay(pdMS_TO_TICKS(10));

    /* Bias: 0xA2 (1/9 bias) */
    esp_lcd_panel_io_tx_param(st7567_io_handle, 0xA2, NULL, 0);

    /* Segment direction (ADC select): 0xA0 normal, 0xA1 reverse */
    uint8_t seg_dir = BOARD_LCD_MIRROR_X ? 0xA1 : 0xA0;
    esp_lcd_panel_io_tx_param(st7567_io_handle, seg_dir, NULL, 0);

    /* COM direction: 0xC0 normal, 0xC8 reverse */
    uint8_t com_dir = BOARD_LCD_MIRROR_Y ? 0xC0 : 0xC8;
    esp_lcd_panel_io_tx_param(st7567_io_handle, com_dir, NULL, 0);

    /* Power Control: Booster ON (0x2C), Regulator ON (0x2E), Follower ON (0x2F) */
    esp_lcd_panel_io_tx_param(st7567_io_handle, 0x2C, NULL, 0);
    vTaskDelay(pdMS_TO_TICKS(5));
    esp_lcd_panel_io_tx_param(st7567_io_handle, 0x2E, NULL, 0);
    vTaskDelay(pdMS_TO_TICKS(5));
    esp_lcd_panel_io_tx_param(st7567_io_handle, 0x2F, NULL, 0);
    vTaskDelay(pdMS_TO_TICKS(5));

    /* Regulation Ratio: 0x20..0x27 (0x25 is standard) */
    esp_lcd_panel_io_tx_param(st7567_io_handle, 0x25, NULL, 0);

    /* Electronic Volume (Contrast): 0x81 followed by contrast value (0x00..0x3F) */
    esp_lcd_panel_io_tx_param(st7567_io_handle, 0x81, NULL, 0);
    esp_lcd_panel_io_tx_param(st7567_io_handle, st7567_contrast, NULL, 0);

    /* Display start line: 0x40 (Line 0) */
    esp_lcd_panel_io_tx_param(st7567_io_handle, 0x40, NULL, 0);

    /* Invert mode: 0xA6 normal, 0xA7 inverse */
    esp_lcd_panel_io_tx_param(st7567_io_handle, BOARD_LCD_INVERT_COLOR ? 0xA7 : 0xA6, NULL, 0);

    /* All pixels normal: 0xA4 */
    esp_lcd_panel_io_tx_param(st7567_io_handle, 0xA4, NULL, 0);

    /* Display ON: 0xAF */
    esp_lcd_panel_io_tx_param(st7567_io_handle, 0xAF, NULL, 0);

    /* Allocate DMA buffer and clear all 8 pages of display RAM */
    st7567_dma_buf = heap_caps_malloc(1024, MALLOC_CAP_DMA);
    if(!st7567_dma_buf) {
        st7567_dma_buf = malloc(1024);
    }
    if(st7567_dma_buf) {
        memset(st7567_dma_buf, 0, 1024);
        for(uint8_t page = 0; page < 8; page++) {
            esp_lcd_panel_io_tx_param(st7567_io_handle, 0xB0 | page, NULL, 0);
            esp_lcd_panel_io_tx_param(st7567_io_handle, 0x10, NULL, 0);
            esp_lcd_panel_io_tx_param(st7567_io_handle, 0x00, NULL, 0);
            furi_hal_display_prepare_flush();
            esp_lcd_panel_io_tx_color(st7567_io_handle, -1, st7567_dma_buf, FB_WIDTH);
            furi_hal_display_wait_flush();
        }
    }

    ESP_LOGI(TAG, "ST7567 Mono Display Initialized (128x64)");
}

void furi_hal_display_commit(const uint8_t* data, uint32_t size) {
    UNUSED(size);
    if(!st7567_io_handle || !data) return;

    furi_hal_spi_bus_lock();
    if(st7567_dma_buf) {
        memcpy(st7567_dma_buf, data, 1024);
    }
    const uint8_t* src = st7567_dma_buf ? st7567_dma_buf : data;
    uint8_t col_offset = BOARD_LCD_GAP_X;

    for(uint8_t page = 0; page < 8; page++) {
        esp_lcd_panel_io_tx_param(st7567_io_handle, 0xB0 | page, NULL, 0);
        esp_lcd_panel_io_tx_param(st7567_io_handle, 0x10 | ((col_offset >> 4) & 0x0F), NULL, 0);
        esp_lcd_panel_io_tx_param(st7567_io_handle, 0x00 | (col_offset & 0x0F), NULL, 0);
        if(page == 7) {
            furi_hal_display_prepare_flush();
        }
        esp_lcd_panel_io_tx_color(st7567_io_handle, -1, src + (page * FB_WIDTH), FB_WIDTH);
    }
    furi_hal_display_wait_flush();
    furi_hal_spi_bus_unlock();
}

void furi_hal_display_set_backlight(uint8_t brightness) {
    furi_hal_light_set(LightBacklight, brightness);
}

bool furi_hal_display_is_backlight_on(void) {
    return furi_hal_light_is_backlight_on();
}

bool furi_hal_display_has_contrast(void) {
    return true;
}

void furi_hal_display_set_contrast(uint8_t contrast) {
    if(contrast > 0x3F) contrast = 0x3F;
    st7567_contrast = contrast;
    if(st7567_io_handle) {
        furi_hal_spi_bus_lock();
        esp_lcd_panel_io_tx_param(st7567_io_handle, 0x81, NULL, 0);
        esp_lcd_panel_io_tx_param(st7567_io_handle, contrast, NULL, 0);
        furi_hal_spi_bus_unlock();
    }
}

uint8_t furi_hal_display_get_contrast(void) {
    return st7567_contrast;
}

void furi_hal_display_sleep(void) {
    if(!st7567_io_handle) return;
    furi_hal_spi_bus_lock();
    esp_lcd_panel_io_tx_param(st7567_io_handle, 0xAE /* Display OFF */, NULL, 0);
    furi_hal_spi_bus_unlock();
}

uint16_t furi_hal_display_get_h_res(void) {
    return LCD_H_RES;
}

uint16_t furi_hal_display_get_v_res(void) {
    return LCD_V_RES;
}

esp_lcd_panel_handle_t furi_hal_display_get_panel_handle(void) {
    /* The ST7567 is driven through esp_lcd panel IO, not an esp_lcd panel.
     * Kept for API compatibility; callers must check for NULL. */
    return NULL;
}
