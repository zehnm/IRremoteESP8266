/*
 Arduino.h - Main include file for the Arduino SDK
 Copyright (c) 2005-2013 Arduino Team.  All right reserved.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

// Stripped down version
// Source:
// https://github.com/espressif/arduino-esp32/blob/b31c9361ec73b043ff9175b4cd48388637a7171b/cores/esp32/esp32-hal.h

#ifndef HAL_ESP32_HAL_H_
#define HAL_ESP32_HAL_H_

#include <inttypes.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_sleep.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "sdkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef F_CPU
#if CONFIG_IDF_TARGET_ESP32  // ESP32/PICO-D4
#define F_CPU (CONFIG_ESP32_DEFAULT_CPU_FREQ_MHZ * 1000000U)
#elif CONFIG_IDF_TARGET_ESP32S2
#define F_CPU (CONFIG_ESP32S2_DEFAULT_CPU_FREQ_MHZ * 1000000U)
#endif
#endif

#if CONFIG_ARDUINO_ISR_IRAM
#define ARDUINO_ISR_ATTR IRAM_ATTR
#define ARDUINO_ISR_FLAG ESP_INTR_FLAG_IRAM
#else
#define ARDUINO_ISR_ATTR
#define ARDUINO_ISR_FLAG (0)
#endif

#ifndef ARDUINO_RUNNING_CORE
#define ARDUINO_RUNNING_CORE CONFIG_ARDUINO_RUNNING_CORE
#endif

#ifndef ARDUINO_EVENT_RUNNING_CORE
#define ARDUINO_EVENT_RUNNING_CORE CONFIG_ARDUINO_EVENT_RUNNING_CORE
#endif

// forward declaration from freertos/portmacro.h
void vPortYield(void);
void yield(void);
#define optimistic_yield(u)

#define ESP_REG(addr) *((volatile uint32_t *)(addr))
#define NOP() asm volatile("nop")

#include "esp32-hal-cpu.h"
#include "esp32-hal-gpio.h"
#include "esp32-hal-timer.h"
#include "esp_arduino_version.h"

unsigned long micros();
unsigned long millis();

/// @brief Delay for given milliseconds.
///
/// Attention: uses vTaskDelay and the actual time that the task remains
/// blocked depends on the tick rate!
/// With the default tick rate of 100Hz, the resolution is 10ms.
/// Any time below that value might not delay the task at all.
/// @param ms milliseconds to wait.
void delay(uint32_t ms);

/// @brief **Busy wait** for the given microseconds.
/// @param us microseconds to wait.
void delayMicroseconds(uint32_t us);

#ifdef __cplusplus
}
#endif

#endif /* HAL_ESP32_HAL_H_ */