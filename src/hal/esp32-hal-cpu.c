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

// Source: https://github.com/espressif/arduino-esp32/blob/idf-release/v5.4/cores/esp32/esp32-hal-cpu.c
// Stripped down version, only required code for IRremoteESP8266 is included

#include "esp_attr.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "soc/rtc.h"

#include "sdkconfig.h"
#if !defined(CONFIG_IDF_TARGET_ESP32C2) && !defined(CONFIG_IDF_TARGET_ESP32C6) && \
    !defined(CONFIG_IDF_TARGET_ESP32H2) && !defined(CONFIG_IDF_TARGET_ESP32P4)
#include "soc/rtc_cntl_reg.h"
#include "soc/syscon_reg.h"
#endif
#include "esp32-hal-cpu.h"
#include "esp32-hal.h"
#include "esp_system.h"
#include "soc/efuse_reg.h"
#ifdef ESP_IDF_VERSION_MAJOR  // IDF 4+
#if CONFIG_IDF_TARGET_ESP32   // ESP32/PICO-D4
#include "esp32/rom/rtc.h"

#include "xtensa_timer.h"
#elif CONFIG_IDF_TARGET_ESP32S2
#include "esp32s2/rom/rtc.h"

#include "xtensa_timer.h"
#elif CONFIG_IDF_TARGET_ESP32S3
#include "esp32s3/rom/rtc.h"

#include "xtensa_timer.h"
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

static const char *TAG = "IR";

typedef struct apb_change_cb_s {
    struct apb_change_cb_s *prev;
    struct apb_change_cb_s *next;
    void                   *arg;
    apb_change_cb_t         cb;
} apb_change_t;

static apb_change_t     *apb_change_callbacks = NULL;
static SemaphoreHandle_t apb_change_lock = NULL;

static void initApbChangeCallback() {
    static volatile bool initialized = false;
    if (!initialized) {
        initialized = true;
        apb_change_lock = xSemaphoreCreateMutex();
        if (!apb_change_lock) {
            initialized = false;
        }
    }
}

bool addApbChangeCallback(void *arg, apb_change_cb_t cb) {
    initApbChangeCallback();
    apb_change_t *c = (apb_change_t *)malloc(sizeof(apb_change_t));
    if (!c) {
        ESP_LOGE(TAG, "Callback Object Malloc Failed");
        return false;
    }
    c->next = NULL;
    c->prev = NULL;
    c->arg = arg;
    c->cb = cb;
    xSemaphoreTake(apb_change_lock, portMAX_DELAY);
    if (apb_change_callbacks == NULL) {
        apb_change_callbacks = c;
    } else {
        apb_change_t *r = apb_change_callbacks;
        // look for duplicate callbacks
        while ((r != NULL) && !((r->cb == cb) && (r->arg == arg))) {
            r = r->next;
        }
        if (r) {
            ESP_LOGE(TAG, "duplicate func=%8p arg=%8p", c->cb, c->arg);
            free(c);
            xSemaphoreGive(apb_change_lock);
            return false;
        } else {
            c->next = apb_change_callbacks;
            apb_change_callbacks->prev = c;
            apb_change_callbacks = c;
        }
    }
    xSemaphoreGive(apb_change_lock);
    return true;
}

bool removeApbChangeCallback(void *arg, apb_change_cb_t cb) {
    initApbChangeCallback();
    xSemaphoreTake(apb_change_lock, portMAX_DELAY);
    apb_change_t *r = apb_change_callbacks;
    // look for matching callback
    while ((r != NULL) && !((r->cb == cb) && (r->arg == arg))) {
        r = r->next;
    }
    if (r == NULL) {
        ESP_LOGE(TAG, "not found func=%8p arg=%8p", cb, arg);
        xSemaphoreGive(apb_change_lock);
        return false;
    } else {
        // patch links
        if (r->prev) {
            r->prev->next = r->next;
        } else {  // this is first link
            apb_change_callbacks = r->next;
        }
        if (r->next) {
            r->next->prev = r->prev;
        }
        free(r);
    }
    xSemaphoreGive(apb_change_lock);
    return true;
}

static uint32_t calculateApb(rtc_cpu_freq_config_t *conf) {
#if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2
    if (conf->freq_mhz >= 80) {
        return 80 * MHZ;
    }
    return (conf->source_freq_mhz * MHZ) / conf->div;
#else
    return APB_CLK_FREQ;
#endif
}

uint32_t getApbFrequency() {
    rtc_cpu_freq_config_t conf;
    rtc_clk_cpu_freq_get_config(&conf);
    return calculateApb(&conf);
}
