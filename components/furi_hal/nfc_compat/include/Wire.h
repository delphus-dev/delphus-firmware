/* I2C mode is not used on Delphus (ST25R3916 is on SPI2). This stub exists
 * only so the library's optional I2C branches compile and link. */
#pragma once

#include <stdint.h>
#include <stddef.h>

class TwoWire {
public:
    void beginTransmission(uint8_t address) { (void)address; }
    size_t write(uint8_t data) { (void)data; return 1; }
    uint8_t endTransmission(bool stop = true) { (void)stop; return 0; }
    uint8_t requestFrom(uint8_t address, uint8_t length) {
        (void)address;
        (void)length;
        return 0;
    }
    int available(void) { return 0; }
    int read(void) { return -1; }
};
