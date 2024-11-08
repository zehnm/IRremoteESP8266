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

// Source: https://github.com/espressif/arduino-esp32/blob/09a6770320b75c219053aa19d630afe1a7c61147/cores/esp32/esp32-hal-gpio.c
// Stripped down version

#include "esp32-hal-gpio.h"
// #include "esp32-hal-periman.h"
#include "hal/gpio_hal.h"
#include "soc/soc_caps.h"
#include "esp_log.h"

static const char *TAG = "IR";

typedef void (*voidFuncPtr)(void);
typedef void (*voidFuncPtrArg)(void *);
typedef struct {
  voidFuncPtr fn;
  void *arg;
  bool functional;
} InterruptHandle_t;
static InterruptHandle_t __pinInterruptHandlers[SOC_GPIO_PIN_COUNT] = {
  0,
};

#include "driver/rtc_io.h"

extern void ARDUINO_ISR_ATTR __pinMode(uint8_t pin, uint8_t mode) {
  if (pin >= SOC_GPIO_PIN_COUNT) {
    ESP_LOGE(TAG, "Invalid IO %i selected", pin);
    return;
  }

  gpio_hal_context_t gpiohal;
  gpiohal.dev = GPIO_LL_GET_HW(GPIO_PORT_0);

  gpio_config_t conf = {
    .pin_bit_mask = (1ULL << pin),              /*!< GPIO pin: set with bit mask, each bit maps to a GPIO */
    .mode = GPIO_MODE_DISABLE,                  /*!< GPIO mode: set input/output mode                     */
    .pull_up_en = GPIO_PULLUP_DISABLE,          /*!< GPIO pull-up                                         */
    .pull_down_en = GPIO_PULLDOWN_DISABLE,      /*!< GPIO pull-down                                       */
    .intr_type = gpiohal.dev->pin[pin].int_type /*!< GPIO interrupt type - previously set                 */
  };
  if (mode < 0x20) {  //io
    conf.mode = mode & (INPUT | OUTPUT);
    if (mode & OPEN_DRAIN) {
      conf.mode |= GPIO_MODE_DEF_OD;
    }
    if (mode & PULLUP) {
      conf.pull_up_en = GPIO_PULLUP_ENABLE;
    }
    if (mode & PULLDOWN) {
      conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
    }
  }
  if (gpio_config(&conf) != ESP_OK) {
    ESP_LOGE(TAG, "IO %i config failed", pin);
    return;
  }
}

extern void ARDUINO_ISR_ATTR __digitalWrite(uint8_t pin, uint8_t val) {
  gpio_set_level((gpio_num_t)pin, val);
}

extern int ARDUINO_ISR_ATTR __digitalRead(uint8_t pin) {
  return gpio_get_level((gpio_num_t)pin);
}

static void ARDUINO_ISR_ATTR __onPinInterrupt(void *arg) {
  InterruptHandle_t *isr = (InterruptHandle_t *)arg;
  if (isr->fn) {
    if (isr->arg) {
      ((voidFuncPtrArg)isr->fn)(isr->arg);
    } else {
      isr->fn();
    }
  }
}

extern void cleanupFunctional(void *arg);

extern void __attachInterruptFunctionalArg(uint8_t pin, voidFuncPtrArg userFunc, void *arg, int intr_type, bool functional) {
  static bool interrupt_initialized = false;

  // makes sure that pin -1 (255) will never work -- this follows Arduino standard
  if (pin >= SOC_GPIO_PIN_COUNT) {
    return;
  }

  if (!interrupt_initialized) {
    esp_err_t err = gpio_install_isr_service((int)ARDUINO_ISR_FLAG);
    interrupt_initialized = (err == ESP_OK) || (err == ESP_ERR_INVALID_STATE);
  }
  if (!interrupt_initialized) {
    ESP_LOGE(TAG, "IO %i ISR Service Failed To Start", pin);
    return;
  }

  // if new attach without detach remove old info
  if (__pinInterruptHandlers[pin].functional && __pinInterruptHandlers[pin].arg) {
    cleanupFunctional(__pinInterruptHandlers[pin].arg);
  }
  __pinInterruptHandlers[pin].fn = (voidFuncPtr)userFunc;
  __pinInterruptHandlers[pin].arg = arg;
  __pinInterruptHandlers[pin].functional = functional;

  gpio_set_intr_type((gpio_num_t)pin, (gpio_int_type_t)(intr_type & 0x7));
  if (intr_type & 0x8) {
    gpio_wakeup_enable((gpio_num_t)pin, (gpio_int_type_t)(intr_type & 0x7));
  }
  gpio_isr_handler_add((gpio_num_t)pin, __onPinInterrupt, &__pinInterruptHandlers[pin]);

  //FIX interrupts on peripherals outputs (eg. LEDC,...)
  //Enable input in GPIO register
  gpio_hal_context_t gpiohal;
  gpiohal.dev = GPIO_LL_GET_HW(GPIO_PORT_0);
  gpio_hal_input_enable(&gpiohal, pin);
}

extern void __attachInterruptArg(uint8_t pin, voidFuncPtrArg userFunc, void *arg, int intr_type) {
  __attachInterruptFunctionalArg(pin, userFunc, arg, intr_type, false);
}

extern void __attachInterrupt(uint8_t pin, voidFuncPtr userFunc, int intr_type) {
  __attachInterruptFunctionalArg(pin, (voidFuncPtrArg)userFunc, NULL, intr_type, false);
}

extern void __detachInterrupt(uint8_t pin) {
  gpio_isr_handler_remove((gpio_num_t)pin);  //remove handle and disable isr for pin
  gpio_wakeup_disable((gpio_num_t)pin);

  if (__pinInterruptHandlers[pin].functional && __pinInterruptHandlers[pin].arg) {
    cleanupFunctional(__pinInterruptHandlers[pin].arg);
  }
  __pinInterruptHandlers[pin].fn = NULL;
  __pinInterruptHandlers[pin].arg = NULL;
  __pinInterruptHandlers[pin].functional = false;

  gpio_set_intr_type((gpio_num_t)pin, GPIO_INTR_DISABLE);
}

extern void enableInterrupt(uint8_t pin) {
  gpio_intr_enable((gpio_num_t)pin);
}

extern void disableInterrupt(uint8_t pin) {
  gpio_intr_disable((gpio_num_t)pin);
}

extern void pinMode(uint8_t pin, uint8_t mode) __attribute__((weak, alias("__pinMode")));
extern void digitalWrite(uint8_t pin, uint8_t val) __attribute__((weak, alias("__digitalWrite")));
extern int digitalRead(uint8_t pin) __attribute__((weak, alias("__digitalRead")));
extern void attachInterrupt(uint8_t pin, voidFuncPtr handler, int mode) __attribute__((weak, alias("__attachInterrupt")));
extern void attachInterruptArg(uint8_t pin, voidFuncPtrArg handler, void *arg, int mode) __attribute__((weak, alias("__attachInterruptArg")));
extern void detachInterrupt(uint8_t pin) __attribute__((weak, alias("__detachInterrupt")));