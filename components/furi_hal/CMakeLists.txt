# The only supported board is delphus (see boards/ and targets/).
if(NOT DEFINED FLIPPER_BOARD)
    set(FLIPPER_BOARD "delphus" CACHE STRING "Target board for Flipper ESP32 port")
endif()
message(STATUS "Flipper board: ${FLIPPER_BOARD}")

set(FURI_HAL_SRCS
    "furi_hal.c"
    "furi_hal_cortex.c"
    "furi_hal_memory.c"
    "furi_hal_rtc.c"
    "furi_hal_power.c"
    "furi_hal_debug.c"
    "furi_hal_interrupt.c"
    "furi_hal_bt.c"
    "furi_hal_crypto.c"
    "furi_hal_flash.c"
    "furi_hal_info.c"
    "furi_hal_region.c"
    "furi_hal_version.c"
    "furi_hal_gpio.c"
    "furi_hal_resources.c"
    "furi_hal_usb.c"
    "furi_hal_usb_cdc.c"
    "furi_hal_light.c"
    "furi_hal_vibro.c"
    "furi_hal_speaker.c"
    "furi_hal_display.c"
    "furi_hal_spi_bus.c"
    "furi_hal_spi.c"
    "furi_hal_subghz.c"
    "furi_hal_pin_arbiter.c"
    "furi_hal_random.c"
    "furi_hal_sd.c"
    "furi_hal_infrared.c"
    "furi_hal_rfid.c"
    "furi_hal_ina219.c"
    "fatfs_compat.c"
)

set(FURI_HAL_REQUIRES
    furi toolbox input driver esp_lcd fatfs sdmmc datetime
    ble_hid ble_serial ble_profile furi_ble flipper_protobuf
    esp_adc esp_driver_rmt esp_driver_i2s nvs_flash mbedtls
)

# HID backend: TinyUSB on SoCs with USB-OTG, stub elsewhere.
if(IDF_TARGET STREQUAL "esp32s3" OR IDF_TARGET STREQUAL "esp32s2")
    list(APPEND FURI_HAL_SRCS
        "furi_hal_usb_hid_tinyusb.c"
        "furi_hal_usb_tinyusb_composite.c"
        "furi_hal_usb_msc.c"
    )
    list(APPEND FURI_HAL_REQUIRES espressif__esp_tinyusb)
else()
    list(APPEND FURI_HAL_SRCS "furi_hal_usb_hid_stub.c")
endif()

# ST25R3916 NFC front-end (ported from the Flipper F7 HAL / upstream
# Sor3nt Flipper-Zero-ESP32-Port). Plain register driver with boot-time
# oscillator wake and retries; shares SPI with the LCD/SD/CC1101.
list(APPEND FURI_HAL_SRCS
    "furi_hal_nfc_st25r3916.c"
    "furi_hal_nfc_event.c"
    "furi_hal_nfc_irq.c"
    "furi_hal_nfc_timer.c"
    "furi_hal_nfc_felica.c"
    "furi_hal_nfc_iso14443a.c"
    "furi_hal_nfc_iso14443b.c"
    "furi_hal_nfc_iso15693.c"
    "st25r3916.c"
    "st25r3916_reg.c"
)

idf_component_register(
    SRCS ${FURI_HAL_SRCS}
    INCLUDE_DIRS "." ".." "../.." "boards" "nfc_rfal" "nfc_compat/include"
    REQUIRES ${FURI_HAL_REQUIRES}
)

# Set board include path as compile definition
target_compile_definitions(${COMPONENT_LIB} PUBLIC
    BOARD_INCLUDE="board_${FLIPPER_BOARD}.h"
)

target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wno-error=implicit-function-declaration
    -Wno-error=builtin-declaration-mismatch
    -Wno-error=deprecated-declarations
    -Wno-error=unused-variable
)
