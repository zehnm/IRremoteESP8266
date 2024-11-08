#pragma once

#ifdef ARDUINO
#include <Arduino.h>
#else

// pull in everything required for IDF
// - stripped down Arduino library classes
// - using std::string, as for unit tests

#include <iostream>
#include <string>

#include "esp32-hal.h"

#ifndef F
// Create a no-op F() macro so the code base still compiles outside of the
// Arduino framework. Thus we can safely use the Arduino 'F()' macro through-out
// the code base. That macro stores constants in Flash (PROGMEM) memory.
// See: https://github.com/crankyoldgit/IRremoteESP8266/issues/667
#define F(x) x
#endif  // F

typedef std::string String;

#endif
