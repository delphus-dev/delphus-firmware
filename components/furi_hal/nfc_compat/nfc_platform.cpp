/* ESP-IDF platform layer for the Arduino-flavoured ST25R3916/RFAL libraries. */
#include "Arduino.h"
#include "SPI.h"

#include <string.h>
#include <esp_timer.h>
#include <esp_rom_sys.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <furi_hal_spi.h>
#include "boards/board.h"

/* The NFC chip lives on the same SPI2 bus as LCD/SD/CC1101. */
static GpioPin nfc_cs_pin = {.port = NULL, .pin = BOARD_PIN_NFC_CS};
static FuriHalSpiBusHandle nfc_spi_handle = {
    .bus = &furi_hal_spi_bus_subghz,
    .miso = NULL,
    .mosi = NULL,
    .sck = NULL,
    .cs = &nfc_cs_pin,
    .device = NULL,
    .initialized = false,
    .frequency_hz = 5000000,
    .mode = 1, /* SPI_MODE1: CPOL=0, CPHA=1 */
};
static bool nfc_platform_ready = false;

extern "C" void nfc_platform_init(void) {
    if(nfc_platform_ready) return;

    static GpioPin nfc_mosi = {.port = NULL, .pin = BOARD_PIN_LCD_MOSI};
    static GpioPin nfc_miso = {.port = NULL, .pin = BOARD_PIN_SD_MISO};
    static GpioPin nfc_sck = {.port = NULL, .pin = BOARD_PIN_LCD_SCLK};
    nfc_spi_handle.mosi = &nfc_mosi;
    nfc_spi_handle.miso = &nfc_miso;
    nfc_spi_handle.sck = &nfc_sck;

    furi_hal_spi_bus_handle_init(&nfc_spi_handle);
    nfc_platform_ready = nfc_spi_handle.initialized;
}

extern "C" void nfc_platform_deinit(void) {
    if(!nfc_platform_ready) return;
    detachInterrupt(BOARD_PIN_NFC_IRQ);
    furi_hal_spi_bus_handle_deinit(&nfc_spi_handle);
    nfc_platform_ready = false;
}

/* ---- Time -------------------------------------------------------------- */

extern "C" unsigned long millis(void) {
    return (unsigned long)(esp_timer_get_time() / 1000ULL);
}

extern "C" void delay(unsigned long ms) {
    if(ms == 0) return;
    vTaskDelay(pdMS_TO_TICKS(ms));
}

extern "C" void delayMicroseconds(unsigned int us) {
    esp_rom_delay_us(us);
}

/* ---- GPIO -------------------------------------------------------------- */

extern "C" void pinMode(int pin, uint8_t mode) {
    if(pin < 0 || pin >= GPIO_NUM_MAX) return;
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << pin,
        .mode = (mode == OUTPUT) ? GPIO_MODE_OUTPUT : GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg);
}

extern "C" void digitalWrite(int pin, uint8_t value) {
    if(pin < 0 || pin >= GPIO_NUM_MAX) return;
    gpio_set_level((gpio_num_t)pin, value ? 1 : 0);
}

extern "C" int digitalRead(int pin) {
    if(pin < 0 || pin >= GPIO_NUM_MAX) return LOW;
    return gpio_get_level((gpio_num_t)pin) ? HIGH : LOW;
}

static void (*s_irq_handler)(void) = NULL;

static void IRAM_ATTR nfc_isr_dispatch(void* arg) {
    (void)arg;
    if(s_irq_handler) s_irq_handler();
}

extern "C" void attachInterrupt(int pin, void (*handler)(void), int mode) {
    (void)mode; /* ST25R3916 IRQ is active high / rising edge */
    if(pin < 0 || pin >= GPIO_NUM_MAX || !handler) return;
    s_irq_handler = handler;
    gpio_install_isr_service(0); /* ESP_ERR_INVALID_STATE if already installed */
    gpio_isr_handler_add((gpio_num_t)pin, nfc_isr_dispatch, NULL);
}

extern "C" void detachInterrupt(int pin) {
    if(pin < 0 || pin >= GPIO_NUM_MAX) return;
    gpio_isr_handler_remove((gpio_num_t)pin);
    s_irq_handler = NULL;
}

/* ---- SPI --------------------------------------------------------------- */

SPIClass::SPIClass() : length(0), inplace(NULL) {
}

void SPIClass::beginTransaction(SPISettings settings) {
    (void)settings;
    length = 0;
    inplace = NULL;
    nfc_platform_init();
}

uint8_t SPIClass::transfer(uint8_t data) {
    if(length >= sizeof(tx_buffer)) return 0;
    tx_buffer[length++] = data;
    return 0; /* real response is collected in endTransaction() */
}

void SPIClass::transfer(void* buffer, size_t len) {
    if(!buffer || !len) return;
    if(length + len > sizeof(tx_buffer)) len = sizeof(tx_buffer) - length;
    memcpy(tx_buffer + length, buffer, len);
    inplace = (uint8_t*)buffer;
    length += len;
}

void SPIClass::endTransaction(void) {
    if(!nfc_platform_ready || length == 0) {
        length = 0;
        inplace = NULL;
        return;
    }
    memset(rx_buffer, 0, length);
    furi_hal_spi_bus_trx(&nfc_spi_handle, tx_buffer, rx_buffer, length, 100);
    if(inplace) memcpy(inplace, rx_buffer, length);
    length = 0;
    inplace = NULL;
}
