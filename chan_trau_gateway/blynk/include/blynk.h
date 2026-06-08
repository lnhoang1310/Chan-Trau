#ifndef BLYNK_H
#define BLYNK_H

#include <stdbool.h>
#include <stdint.h>

bool blynk_begin(const char *token);

bool blynk_virtual_write_int(
        uint8_t pin,
        int value);

bool blynk_virtual_write_float(
        uint8_t pin,
        float value);

bool blynk_virtual_write_string(
        uint8_t pin,
        const char *value);

bool blynk_location(
        float latitude,
        float longitude);

#endif