// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Source: https://github.com/espressif/arduino-esp32/blob/idf-release/v5.4/cores/esp32/esp32-hal-cpu.h
// Stripped down version, only required code for IRremoteESP8266 is included

#ifndef _ESP32_HAL_CPU_H_
#define _ESP32_HAL_CPU_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

typedef enum { APB_BEFORE_CHANGE, APB_AFTER_CHANGE } apb_change_ev_t;

typedef void (*apb_change_cb_t)(void *arg, apb_change_ev_t ev_type, uint32_t old_apb, uint32_t new_apb);

bool addApbChangeCallback(void *arg, apb_change_cb_t cb);
bool removeApbChangeCallback(void *arg, apb_change_cb_t cb);

uint32_t getApbFrequency();  // In Hz

#ifdef __cplusplus
}
#endif

#endif /* _ESP32_HAL_CPU_H_ */