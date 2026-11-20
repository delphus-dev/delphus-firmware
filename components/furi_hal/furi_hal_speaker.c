/**
 * @file furi_hal_speaker.c
 * Speaker HAL — PWM (LEDC) tone generation on a single GPIO buzzer.
 *
 * Keeps the STM32 Flipper Zero API (exclusive ownership, start/stop,
 * cubic volume curve) but drives the buzzer with an LEDC square wave instead
 * of I2S: no DMA buffers, no background writer thread, no BCLK/WS pins.
 *
 * Boards without a speaker get no-op stubs.
 */

#include "furi_hal_speaker.h"
#include "boards/board.h"
#include <furi.h>

#define TAG "FuriHalSpeaker"

#if BOARD_HAS_SPEAKER

#include <driver/ledc.h>
#include <driver/gpio.h>
#include <esp_err.h>

/* Backlight uses LEDC_TIMER_0 / CHANNEL_0 (see furi_hal_light.c). */
#define SPEAKER_LEDC_TIMER   LEDC_TIMER_1
#define SPEAKER_LEDC_CHANNEL LEDC_CHANNEL_1
#define SPEAKER_LEDC_SPEED   LEDC_LOW_SPEED_MODE
#define SPEAKER_LEDC_RES     LEDC_TIMER_8_BIT
#define SPEAKER_DEFAULT_FREQ 1000U

static FuriMutex* speaker_mutex = NULL;
static bool speaker_ready = false;
static uint32_t speaker_frequency_hz = SPEAKER_DEFAULT_FREQ;

/** Cubic curve: same perceptual scaling as the original firmware. */
static inline float speaker_volume_curve(float v) {
    if(v < 0.0f) v = 0.0f;
    if(v > 1.0f) v = 1.0f;
    return v * v * v;
}

/* 0..128 duty (0..50%). A 50% square wave is the loudest for a passive buzzer;
 * going above that only adds DC and does not increase perceived volume. */
static uint32_t speaker_duty(float volume) {
    return (uint32_t)(speaker_volume_curve(volume) * 128.0f);
}

static void speaker_push(uint32_t frequency_hz, float volume) {
    if(!speaker_ready) return;
    ledc_set_freq(SPEAKER_LEDC_SPEED, SPEAKER_LEDC_TIMER, frequency_hz);
    ledc_set_duty(SPEAKER_LEDC_SPEED, SPEAKER_LEDC_CHANNEL, speaker_duty(volume));
    ledc_update_duty(SPEAKER_LEDC_SPEED, SPEAKER_LEDC_CHANNEL);
}

void furi_hal_speaker_init(void) {
    furi_assert(speaker_mutex == NULL);
    speaker_mutex = furi_mutex_alloc(FuriMutexTypeNormal);

    if(BOARD_PIN_SPEAKER_DOUT == UINT16_MAX || BOARD_PIN_SPEAKER_DOUT >= GPIO_NUM_MAX) {
        FURI_LOG_W(TAG, "Speaker pin not mapped");
        return;
    }

    ledc_timer_config_t timer_cfg = {
        .speed_mode = SPEAKER_LEDC_SPEED,
        .duty_resolution = SPEAKER_LEDC_RES,
        .timer_num = SPEAKER_LEDC_TIMER,
        .freq_hz = SPEAKER_DEFAULT_FREQ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_cfg));

    ledc_channel_config_t channel_cfg = {
        .gpio_num = (gpio_num_t)BOARD_PIN_SPEAKER_DOUT,
        .speed_mode = SPEAKER_LEDC_SPEED,
        .channel = SPEAKER_LEDC_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = SPEAKER_LEDC_TIMER,
        .duty = 0,
        .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&channel_cfg));
    speaker_ready = true;
    speaker_frequency_hz = SPEAKER_DEFAULT_FREQ;

    FURI_LOG_I(TAG, "PWM speaker initialized on GPIO%d", BOARD_PIN_SPEAKER_DOUT);
}

void furi_hal_speaker_deinit(void) {
    furi_check(speaker_mutex != NULL);
    if(speaker_ready) {
        ledc_stop(SPEAKER_LEDC_SPEED, SPEAKER_LEDC_CHANNEL, 0);
        speaker_ready = false;
    }
    furi_mutex_free(speaker_mutex);
    speaker_mutex = NULL;
}

bool furi_hal_speaker_acquire(uint32_t timeout) {
    furi_check(!FURI_IS_IRQ_MODE());
    return furi_mutex_acquire(speaker_mutex, timeout) == FuriStatusOk;
}

void furi_hal_speaker_release(void) {
    furi_check(!FURI_IS_IRQ_MODE());
    furi_check(furi_hal_speaker_is_mine());
    furi_hal_speaker_stop();
    furi_check(furi_mutex_release(speaker_mutex) == FuriStatusOk);
}

bool furi_hal_speaker_is_mine(void) {
    return FURI_IS_IRQ_MODE() ||
           (furi_mutex_get_owner(speaker_mutex) == furi_thread_get_current_id());
}

void furi_hal_speaker_start(float frequency, float volume) {
    furi_check(furi_hal_speaker_is_mine());

    if(volume <= 0.0f || !speaker_ready) {
        furi_hal_speaker_stop();
        return;
    }

    if(frequency < 1.0f) frequency = 1.0f;
    speaker_frequency_hz = (uint32_t)frequency;
    speaker_push(speaker_frequency_hz, volume);
}

void furi_hal_speaker_set_volume(float volume) {
    furi_check(furi_hal_speaker_is_mine());
    speaker_push(speaker_frequency_hz, volume);
}

void furi_hal_speaker_stop(void) {
    furi_check(furi_hal_speaker_is_mine());
    if(speaker_ready) {
        ledc_set_duty(SPEAKER_LEDC_SPEED, SPEAKER_LEDC_CHANNEL, 0);
        ledc_update_duty(SPEAKER_LEDC_SPEED, SPEAKER_LEDC_CHANNEL);
    }
}

void furi_hal_speaker_start_gdo_mirror(const GpioPin* gdo_pin, float volume) {
    furi_check(furi_hal_speaker_is_mine());
    /* 1-bit GPIO mirroring was an I2S-only feature of the T-Embed port.
     * The PWM buzzer cannot reproduce raw RF demodulator audio; keep the API
     * and fall back to a plain tone. */
    (void)gdo_pin;
    furi_hal_speaker_start(1000.0f, volume);
}

#else /* !BOARD_HAS_SPEAKER */

void furi_hal_speaker_init(void) {
}

void furi_hal_speaker_deinit(void) {
}

bool furi_hal_speaker_acquire(uint32_t timeout) {
    (void)timeout;
    return true;
}

void furi_hal_speaker_release(void) {
}

bool furi_hal_speaker_is_mine(void) {
    return true;
}

void furi_hal_speaker_start(float frequency, float volume) {
    (void)frequency;
    (void)volume;
}

void furi_hal_speaker_set_volume(float volume) {
    (void)volume;
}

void furi_hal_speaker_stop(void) {
}

void furi_hal_speaker_start_gdo_mirror(const GpioPin* gdo_pin, float volume) {
    (void)gdo_pin;
    (void)volume;
}

#endif /* BOARD_HAS_SPEAKER */
