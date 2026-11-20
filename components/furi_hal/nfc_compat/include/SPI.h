/* Minimal Arduino SPIClass shim: transactions are buffered and sent to the
 * shared ESP32 SPI bus with a single CS assertion in endTransaction(). */
#pragma once

#include <stdint.h>
#include <stddef.h>

#define MSBFIRST 1
#define SPI_MODE0 0
#define SPI_MODE1 1
#define SPI_MODE2 2
#define SPI_MODE3 3

class SPISettings {
public:
    SPISettings(uint32_t clock = 5000000, uint8_t order = MSBFIRST, uint8_t mode = SPI_MODE1)
        : clock(clock), order(order), mode(mode) {}
    uint32_t clock;
    uint8_t order;
    uint8_t mode;
};

class SPIClass {
public:
    SPIClass();
    void beginTransaction(SPISettings settings);
    uint8_t transfer(uint8_t data);
    void transfer(void* buffer, size_t length);
    void endTransaction();

private:
    uint8_t tx_buffer[1024];
    uint8_t rx_buffer[1024];
    size_t length;
    uint8_t* inplace;
};
