/* Minimal Arduino compatibility header for the ST25R3916/RFAL libraries.
 * Only the handful of APIs actually used by those libraries is provided. */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <math.h>

#define HIGH    1
#define LOW     0
#define OUTPUT  1
#define INPUT   0
#define RISING  3

#ifdef __cplusplus
extern "C" {
#endif

unsigned long millis(void);
void delay(unsigned long ms);
void delayMicroseconds(unsigned int us);
void pinMode(int pin, uint8_t mode);
void digitalWrite(int pin, uint8_t value);
int digitalRead(int pin);
void attachInterrupt(int pin, void (*handler)(void), int mode);
void detachInterrupt(int pin);

#ifdef __cplusplus
}
#endif
