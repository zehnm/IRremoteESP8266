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

// Source: https://github.com/espressif/arduino-esp32/blob/idf-release/v5.4/cores/esp32/esp32-hal-misc.c
// Stripped down version, only required code for IRremoteESP8266 is included

#include "esp_attr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "sdkconfig.h"
#ifdef CONFIG_APP_ROLLBACK_ENABLE
#include "esp_ota_ops.h"
#endif  // CONFIG_APP_ROLLBACK_ENABLE
#include <sys/time.h>

#include "esp_private/startup_internal.h"
#include "soc/rtc.h"
#if !defined(CONFIG_IDF_TARGET_ESP32C2) && !defined(CONFIG_IDF_TARGET_ESP32C6) && \
    !defined(CONFIG_IDF_TARGET_ESP32H2) && !defined(CONFIG_IDF_TARGET_ESP32P4)
#include "soc/rtc_cntl_reg.h"
#include "soc/syscon_reg.h"
#endif
#include "esp32-hal.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#ifdef ESP_IDF_VERSION_MAJOR  // IDF 4+

#if CONFIG_IDF_TARGET_ESP32  // ESP32/PICO-D4
#include "esp32/rom/rtc.h"
#elif CONFIG_IDF_TARGET_ESP32S2
#include "esp32s2/rom/rtc.h"
#elif CONFIG_IDF_TARGET_ESP32S3
#include "esp32s3/rom/rtc.h"
#elif CONFIG_IDF_TARGET_ESP32C2
#include "esp32c2/rom/rtc.h"
#elif CONFIG_IDF_TARGET_ESP32C3
#include "esp32c3/rom/rtc.h"
#elif CONFIG_IDF_TARGET_ESP32C6
#include "esp32c6/rom/rtc.h"
#elif CONFIG_IDF_TARGET_ESP32H2
#include "esp32h2/rom/rtc.h"
#elif CONFIG_IDF_TARGET_ESP32P4
#include "esp32p4/rom/rtc.h"

#else
#error Target CONFIG_IDF_TARGET is not supported
#endif

#else  // ESP32 Before IDF 4.0
#include "rom/rtc.h"
#endif

void __yield() {
    vPortYield();
}

void yield() __attribute__((weak, alias("__yield")));

unsigned long ARDUINO_ISR_ATTR micros() {
    return (unsigned long)(esp_timer_get_time());
}

unsigned long ARDUINO_ISR_ATTR millis() {
    return (unsigned long)(esp_timer_get_time() / 1000ULL);
}

void delay(uint32_t ms) {
    vTaskDelay(ms / portTICK_PERIOD_MS);
}

void ARDUINO_ISR_ATTR delayMicroseconds(uint32_t us) {
    uint64_t m = (uint64_t)esp_timer_get_time();
    if (us) {
        uint64_t e = (m + us);
        if (m > e) {  // overflow
            while ((uint64_t)esp_timer_get_time() > e) {
                NOP();
            }
        }
        while ((uint64_t)esp_timer_get_time() < e) {
            NOP();
        }
    }
}
