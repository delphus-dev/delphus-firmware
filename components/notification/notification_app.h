/**
 * @file notification_app.h
 * @brief ESP32 notification app internals — exposes NotificationSettings and
 *        NotificationApp so that the settings UI can read/write them directly.
 *
 * The WS2812 is a status LED only (notification sequences drive it); there
 * are no user-configurable colours or effects.
 */
#pragma once

#include <furi.h>
#include "notification.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LayerInternal = 0,
    LayerNotification = 1,
    LayerMAX = 2,
} NotificationLedLayerIndex;

typedef struct {
    uint8_t value[LayerMAX];
    NotificationLedLayerIndex index;
} NotificationDisplayLayer;

typedef struct {
    float display_brightness;
    uint32_t display_off_delay_ms;
    float speaker_volume;
    bool vibro_on;
    float night_shift;
    uint32_t night_shift_start;
    uint32_t night_shift_end;
    /* LCD contrast value (0..63) for monochrome displays (e.g. ST7567).
     * Defaults to BOARD_LCD_CONTRAST (26). */
    uint8_t lcd_contrast;
} NotificationSettings;

struct NotificationApp {
    FuriMessageQueue* queue;
    FuriPubSub* event_record;
    FuriTimer* display_timer;
    FuriTimer* night_shift_timer;
    FuriTimer* night_shift_demo_timer;

    NotificationDisplayLayer display;
    bool display_led_lock;

    NotificationSettings settings;
    float current_night_shift;
};

/** Save current settings (enqueues a save message to the notification thread). */
void notification_message_save_settings(NotificationApp* app);

/** Night-shift timer control (used by settings app). */
void night_shift_timer_start(NotificationApp* app);
void night_shift_timer_stop(NotificationApp* app);

#ifdef __cplusplus
}
#endif
