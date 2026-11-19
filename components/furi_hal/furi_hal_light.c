/**
 * @file furi_hal_light.c
 * Light control HAL — LCD backlight (LEDC PWM) + WS2812 RGB ring (RMT).
 *
 * The T-Embed CC1101 PLUS has two distinct light hardware:
 *   - Single-channel white LCD backlight on BOARD_PIN_LCD_BL (LEDC PWM)
 *   - 8x WS2812 RGB LED ring on BOARD_PIN_WS2812_DATA (driven via RMT,
 *     power-gated by BOARD_PIN_PWR_EN which is asserted in furi_hal_init_early)
 *
 * The RGB ring is treated as one logical RGB LED — all 8 LEDs mirror the
 * same color. This matches the Light enum (Red/Green/Blue bit flags) and
 * mirrors how Bruce drives the same hardware.
 */

#include "furi_hal_light.h"
#include "furi_hal_resources.h"
#include "boards/board.h"

#include <driver/ledc.h>
#include <driver/gpio.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#ifdef BOARD_PIN_WS2812_DATA
#include <led_strip.h>
#endif

#define BACKLIGHT_LEDC_TIMER    LEDC_TIMER_0
#define BACKLIGHT_LEDC_CHANNEL  LEDC_CHANNEL_0
#define BACKLIGHT_LEDC_SPEED    LEDC_LOW_SPEED_MODE
#define BACKLIGHT_LEDC_FREQ     5000
#define BACKLIGHT_LEDC_DUTY_RES LEDC_TIMER_8_BIT

static const char* TAG = "FuriHalLight";

#ifdef BOARD_PIN_WS2812_DATA
static led_strip_handle_t led_strip = NULL;
/* Cached per-channel brightness (0-255). The Light enum lets callers update
 * one channel at a time (e.g. furi_hal_light_set(LightRed, 0xFF)); we cache
 * each channel and push the combined RGB to all WS2812 pixels on every
 * change. */
static uint8_t led_r = 0, led_g = 0, led_b = 0;

/* Status-LED blinking layer (original Flipper behaviour): while active it
 * owns the strip; the ambient colour/effect only updates the base layer and
 * is restored when blinking stops. */
static volatile bool led_blink_active = false;
static uint8_t blink_r = 0, blink_g = 0, blink_b = 0;
static uint16_t blink_on_ms = 10, blink_period_ms = 100;
static TaskHandle_t blink_task_handle = NULL;
#endif

static uint32_t furi_hal_light_backlight_duty_from_value(uint8_t value) {
#if BOARD_LCD_BL_ACTIVE_LOW
    /* Active-low backlight (e.g. P-channel high-side switch) */
    return UINT8_MAX - value;
#else
    return value;
#endif
}

#ifdef BOARD_PIN_WS2812_DATA
static void ws2812_push_raw(uint8_t r, uint8_t g, uint8_t b) {
    if(!led_strip) return;
    for(uint16_t i = 0; i < BOARD_WS2812_LED_COUNT; i++) {
        led_strip_set_pixel(led_strip, i, r, g, b);
    }
    led_strip_refresh(led_strip);
}

static void furi_hal_light_ws2812_push(void) {
    ws2812_push_raw(led_r, led_g, led_b);
}

static void ws2812_blink_task(void* arg) {
    (void)arg;
    while(led_blink_active) {
        ws2812_push_raw(blink_r, blink_g, blink_b);
        vTaskDelay(pdMS_TO_TICKS(blink_on_ms ? blink_on_ms : 1));
        if(!led_blink_active) break;
        uint16_t off_ms = (blink_period_ms > blink_on_ms) ? (blink_period_ms - blink_on_ms) : 1;
        ws2812_push_raw(0, 0, 0);
        vTaskDelay(pdMS_TO_TICKS(off_ms ? off_ms : 1));
    }
    /* Restore the base (ambient / charging) colour. */
    ws2812_push_raw(led_r, led_g, led_b);
    blink_task_handle = NULL;
    vTaskDelete(NULL);
}

static void ws2812_blink_stop_internal(void) {
    if(!led_blink_active && !blink_task_handle) return;
    led_blink_active = false;
    for(int i = 0; i < 20 && blink_task_handle; i++) vTaskDelay(1);
    if(blink_task_handle) {
        vTaskDelete(blink_task_handle);
        blink_task_handle = NULL;
    }
    ws2812_push_raw(led_r, led_g, led_b);
}

static void furi_hal_light_ws2812_init(void) {
    led_strip_config_t strip_config = {
        .strip_gpio_num = BOARD_PIN_WS2812_DATA,
        .max_leds = BOARD_WS2812_LED_COUNT,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags = { .invert_out = false },
    };
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,  /* 10 MHz */
        .mem_block_symbols = 0,             /* default */
        .flags = { .with_dma = false },
    };
    esp_err_t err = led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip);
    if(err != ESP_OK) {
        ESP_LOGE(TAG, "WS2812 RMT init failed: %s", esp_err_to_name(err));
        led_strip = NULL;
        return;
    }
    led_strip_clear(led_strip);
    /* Strip starts dark; the notification service drives it as a status LED. */
    ESP_LOGI(TAG, "WS2812 ring (%d LEDs) on GPIO%d initialized",
        BOARD_WS2812_LED_COUNT, BOARD_PIN_WS2812_DATA);
}
#endif

void furi_hal_light_init(void) {
    if(gpio_lcd_bl.pin != UINT16_MAX) {
        /* Release any deep-sleep GPIO hold on the backlight pin */
        gpio_hold_dis((gpio_num_t)gpio_lcd_bl.pin);

        /* Configure LEDC timer for backlight PWM */
        ledc_timer_config_t timer_conf = {
            .speed_mode = BACKLIGHT_LEDC_SPEED,
            .duty_resolution = BACKLIGHT_LEDC_DUTY_RES,
            .timer_num = BACKLIGHT_LEDC_TIMER,
            .freq_hz = BACKLIGHT_LEDC_FREQ,
            .clk_cfg = LEDC_AUTO_CLK,
        };
        ESP_ERROR_CHECK(ledc_timer_config(&timer_conf));

        /* Configure LEDC channel on backlight pin */
        ledc_channel_config_t channel_conf = {
            .speed_mode = BACKLIGHT_LEDC_SPEED,
            .channel = BACKLIGHT_LEDC_CHANNEL,
            .timer_sel = BACKLIGHT_LEDC_TIMER,
            .intr_type = LEDC_INTR_DISABLE,
            .gpio_num = gpio_lcd_bl.pin,
            .duty = furi_hal_light_backlight_duty_from_value(255),
            .hpoint = 0,
        };
        ESP_ERROR_CHECK(ledc_channel_config(&channel_conf));

        ESP_LOGI(TAG, "Backlight PWM initialized on GPIO%d", gpio_lcd_bl.pin);
    }

#if defined(BOARD_PIN_WS2812_DATA) && (BOARD_PIN_WS2812_DATA != UINT16_MAX) && (BOARD_WS2812_LED_COUNT > 0)
    furi_hal_light_ws2812_init();
#endif
}

static volatile uint8_t backlight_current_duty = 255;

void furi_hal_light_set(Light light, uint8_t value) {
    if((light & LightBacklight) && (gpio_lcd_bl.pin != UINT16_MAX)) {
        backlight_current_duty = value;
        ledc_set_duty(
            BACKLIGHT_LEDC_SPEED,
            BACKLIGHT_LEDC_CHANNEL,
            furi_hal_light_backlight_duty_from_value(value));
        ledc_update_duty(BACKLIGHT_LEDC_SPEED, BACKLIGHT_LEDC_CHANNEL);
    }
#if defined(BOARD_PIN_WS2812_DATA) && (BOARD_PIN_WS2812_DATA != UINT16_MAX) && (BOARD_WS2812_LED_COUNT > 0)
    bool rgb_changed = false;
    if(light & LightRed)   { led_r = value; rgb_changed = true; }
    if(light & LightGreen) { led_g = value; rgb_changed = true; }
    if(light & LightBlue)  { led_b = value; rgb_changed = true; }
    /* While a status blink owns the strip, only update the base colour. */
    if(rgb_changed && !led_blink_active) furi_hal_light_ws2812_push();
#endif
}

bool furi_hal_light_is_backlight_on(void) {
    if(gpio_lcd_bl.pin == UINT16_MAX) {
        return true;
    }
    return backlight_current_duty > 0;
}

void furi_hal_light_blink_start(Light light, uint8_t brightness, uint16_t on_time, uint16_t period) {
#if defined(BOARD_PIN_WS2812_DATA) && (BOARD_PIN_WS2812_DATA != UINT16_MAX) && (BOARD_WS2812_LED_COUNT > 0)
    ws2812_blink_stop_internal();
    blink_r = (light & LightRed) ? brightness : 0;
    blink_g = (light & LightGreen) ? brightness : 0;
    blink_b = (light & LightBlue) ? brightness : 0;
    blink_on_ms = on_time;
    blink_period_ms = period;
    led_blink_active = true;
    xTaskCreate(ws2812_blink_task, "LedBlink", 2048, NULL, 2, &blink_task_handle);
#else
    (void)light;
    (void)brightness;
    (void)on_time;
    (void)period;
#endif
}

void furi_hal_light_blink_stop(void) {
#if defined(BOARD_PIN_WS2812_DATA) && (BOARD_PIN_WS2812_DATA != UINT16_MAX) && (BOARD_WS2812_LED_COUNT > 0)
    ws2812_blink_stop_internal();
#endif
}

void furi_hal_light_blink_set_color(Light light) {
#if defined(BOARD_PIN_WS2812_DATA) && (BOARD_PIN_WS2812_DATA != UINT16_MAX) && (BOARD_WS2812_LED_COUNT > 0)
    blink_r = (light & LightRed) ? UINT8_MAX : 0;
    blink_g = (light & LightGreen) ? UINT8_MAX : 0;
    blink_b = (light & LightBlue) ? UINT8_MAX : 0;
    if(led_blink_active) ws2812_push_raw(blink_r, blink_g, blink_b);
#else
    (void)light;
#endif
}

void furi_hal_light_sequence(const char* sequence) {
    (void)sequence;
}

#if defined(BOARD_PIN_WS2812_DATA) && (BOARD_PIN_WS2812_DATA != UINT16_MAX) && (BOARD_WS2812_LED_COUNT > 0)
void furi_hal_light_set_rgb_all(uint8_t r, uint8_t g, uint8_t b) {
    led_r = r;
    led_g = g;
    led_b = b;
    /* Blink layer owns the strip; base colour is restored on blink stop. */
    if(led_blink_active) return;
    ws2812_push_raw(r, g, b);
}

void furi_hal_light_set_pixel(uint16_t idx, uint8_t r, uint8_t g, uint8_t b) {
    if(!led_strip || idx >= BOARD_WS2812_LED_COUNT) return;
    if(idx == 0) {
        led_r = r;
        led_g = g;
        led_b = b;
    }
    led_strip_set_pixel(led_strip, idx, r, g, b);
}

void furi_hal_light_refresh(void) {
    if(!led_strip || led_blink_active) return;
    led_strip_refresh(led_strip);
}

uint16_t furi_hal_light_pixel_count(void) {
    return BOARD_WS2812_LED_COUNT;
}

void furi_hal_light_get_rgb(uint8_t* r, uint8_t* g, uint8_t* b) {
    if(r) *r = led_r;
    if(g) *g = led_g;
    if(b) *b = led_b;
}
#else
void furi_hal_light_set_rgb_all(uint8_t r, uint8_t g, uint8_t b) {
    (void)r; (void)g; (void)b;
}
void furi_hal_light_set_pixel(uint16_t idx, uint8_t r, uint8_t g, uint8_t b) {
    (void)idx; (void)r; (void)g; (void)b;
}
void furi_hal_light_refresh(void) {}
uint16_t furi_hal_light_pixel_count(void) { return 0; }
void furi_hal_light_get_rgb(uint8_t* r, uint8_t* g, uint8_t* b) {
    if(r) *r = 0;
    if(g) *g = 0;
    if(b) *b = 0;
}
#endif
