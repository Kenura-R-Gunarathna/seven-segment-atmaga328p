#pragma once

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>

void millis_init(void);
uint32_t millis(void);
