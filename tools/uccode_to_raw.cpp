// Custom version of the code_to_raw tool for Unfolded Circle Remote Two.
// Convert an IR hex code to raw timing and additional IR information to
// json output.
//
// Original file header of code_to_raw:
// Quick and dirty tool to convert a protocol's (hex) codes to raw timings.
// Copyright 2021 David Conran

#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <iostream>
#include <string>
#include "IRac.h"
#include "IRsend.h"
#include "IRsend_test.h"
#include "IRutils.h"


String resultToRaw(const decode_results * const results,
                   const uint16_t repeat);

void usage_error(char *name) {
  std::cerr << "Usage: " << name << " UC_CODE" << std::endl;
  std::cerr << std::endl;
  std::cerr << "  UC_CODE: <PROTOCOL>;<CODE>;<BITS>;<REPEAT>" << std::endl;
  std::cerr << std::endl;
  std::cerr << "  Example: " << name << " \"12;0xE242;16;2\"" << std::endl;
}

int main(int argc, char *argv[]) {
  int argv_offset = 1;
  uint64_t code = 0;
  uint8_t state[kStateSizeMax] = {0};  // All array elements are set to 0.
  decode_type_t input_type = decode_type_t::UNKNOWN;

  // Check the invocation/calling usage.
  if (argc != 2) {
    usage_error(argv[0]);
    return 1;
  }

  // Split the UC_CODE parameter into protocol / code / bits / repeats
  char* parts[4];
  int partcount = 0;

  parts[partcount++] = argv[argv_offset];

  char* ptr = argv[argv_offset];
  while (*ptr && partcount < 4) {
      if (*ptr == ';') {
          *ptr = 0;
          parts[partcount++] = ptr + 1;
      }
      ptr++;
  }

  if (partcount != 4) {
    std::cerr << "Invalid UC_CODE specified" << std::endl;
    return 1;
  }
  argv_offset++;

  input_type = strToDecodeType(parts[0]);
  switch (input_type) {
    // Unsupported types
    case decode_type_t::UNUSED:
    case decode_type_t::UNKNOWN:
    case decode_type_t::GLOBALCACHE:
    case decode_type_t::PRONTO:
    case decode_type_t::RAW:
      std::cerr << "The protocol specified is not supported by this program."
                << std::endl;
      return 1;
    default:
      break;
  }

  uint16_t nbits = static_cast<uint16_t>(std::stoul(parts[2]));
  if (nbits == 0 && (nbits <= kStateSizeMax * 8)) {
    std::cerr << "Nr. of bits " << parts[2]
              << " is invalid." << std::endl;
    return 1;
  }
  uint16_t stateSize = nbits / 8;

  uint16_t repeats = static_cast<uint16_t>(std::stoul(parts[3]));
  if (repeats > 20) {
    std::cerr << "Repeat count is too large: " << repeats
              << ". Maximum is 20." << std::endl;
    return 1;
  }

  String hexstr = String(parts[1]);
  uint64_t strOffset = 0;
  if (hexstr.rfind("0x", 0) || hexstr.rfind("0X", 0)) {
    strOffset = 2;
  }

  // Calculate how many hexadecimal characters there are.
  uint64_t hexstrlength = hexstr.length() - strOffset;

  // Ptr to the least significant byte of the resulting state for this
  // protocol.
  uint8_t *statePtr = &state[stateSize - 1];

  // Convert the string into a state array of the correct length.
  for (uint16_t i = 0; i < hexstrlength; i++) {
    // Grab the next least sigificant hexadecimal digit from the string.
    uint8_t c = tolower(hexstr[hexstrlength + strOffset - i - 1]);
    if (isxdigit(c)) {
      if (isdigit(c))
        c -= '0';
      else
        c = c - 'a' + 10;
    } else {
      std::cerr << "Code " << parts[1]
                << " contains non-hexidecimal characters." << std::endl;
      return 3;
    }
    if (i % 2 == 1) {  // Odd: Upper half of the byte.
      *statePtr += (c << 4);
      statePtr--;  // Advance up to the next least significant byte of state.
    } else {  // Even: Lower half of the byte.
      *statePtr = c;
    }
  }
  if (!hasACState(input_type)) {
    code = std::stoull(parts[1], nullptr, 16);
  }

  IRsendTest irsend(kGpioUnused);
  IRrecv irrecv(kGpioUnused);
  irsend.begin();
  irsend.reset();

  bool result;
  if (hasACState(input_type)) {  // Is it larger than 64 bits?
    result = irsend.send(input_type, state, stateSize);
  } else {
    result = irsend.send(input_type, code, nbits, repeats);
  }

  if (!result) {
    std::cerr << "Failed to decode IR code!" << std::endl;
    return 4;
  }

  irsend.makeDecodeResult();
  irrecv.decode(&irsend.capture);

  std::cout << resultToRaw(&irsend.capture, repeats) << std::endl;

  return 0;
}

/// Return the frequency of a given protocol.
/// Values have been pulled out of the individual IR decoders.
uint32_t frequency(const decode_results * const results) {
  switch (results->decode_type) {
    case decode_type_t::DAIKIN2:
    case decode_type_t::PANASONIC:
      return kPanasonicFreq;
    case decode_type_t::DENON:
      return results->bits >= kPanasonicBits ? kPanasonicFreq : 38000;
    case decode_type_t::RC5:
    case decode_type_t::RC5X:
    case decode_type_t::RC6:
    case decode_type_t::RCMM:
    case decode_type_t::TROTEC:
      return 36000;
    case decode_type_t::PIONEER:
    case decode_type_t::SONY:
      return 40000;
    case decode_type_t::DISH:
      return 57600;
    case decode_type_t::LUTRON:
      return 40000;
    default:
      return 38000;
  }
}

/// Return the duty cyle of a given protocol.
/// Values have been pulled out of the individual IR decoders.
uint8_t duty_cycle(const decode_results * const results) {
  switch (results->decode_type) {
    case decode_type_t::RC5:
    case decode_type_t::RC5X:
    case decode_type_t::LASERTAG:
    case decode_type_t::MWM:
      return 25;
    case decode_type_t::JVC:
    case decode_type_t::RC6:
    case decode_type_t::RCMM:
      return 33;
    case decode_type_t::LUTRON:
      return 40;
    default:
      return 50;
  }
}

/// Return a String containing the key values of a decode_results structure
/// in a JSON style format.
/// @param[in] results A ptr to a decode_results structure.
/// @return A String containing the code-ified result.
String resultToRaw(const decode_results * const results,
                   const uint16_t repeat) {
  String output = "";
  const uint16_t length = getCorrectedRawLength(results);
  const bool hasState = hasACState(results->decode_type);
  // Reserve some space for the string to reduce heap fragmentation.
  // "uint16_t rawData[9999] = {};  // LONGEST_PROTOCOL\n" = ~55 chars.
  // "NNNN,  " = ~7 chars on average per raw entry
  // Protocols with a `state`:
  //   "uint8_t state[NN] = {};\n" = ~25 chars
  //   "0xNN, " = 6 chars per byte.
  // Protocols without a `state`:
  //   " DEADBEEFDEADBEEF\n"
  //   "uint32_t address = 0xDEADBEEF;\n"
  //   "uint32_t command = 0xDEADBEEF;\n"
  //   "uint64_t data = 0xDEADBEEFDEADBEEF;" = ~116 chars max.
  output.reserve(55 + (length * 7) + hasState ? 25 + (results->bits / 8) * 6
                                              : 116);
  // Start declaration
  output += F("{\n\"raw\": [");  // Start declaration

  // Dump data
  for (uint16_t i = 1; i < results->rawlen; i++) {
    uint32_t usecs = results->rawbuf[i] * kRawTick;
    output += uint64ToString(usecs, 10);
    if (i < results->rawlen - 1)
      output += kCommaSpaceStr;            // ',' not needed on the last one
  }

  output += F("],\n");

  output += F("\"protocol\": \"");
  output += typeToString(results->decode_type);
  output += F("\",\n\"bits\": ");
  output += uint64ToString(results->bits, 10);
  output += F(",\n\"frequency\": ");
  output += uint64ToString(frequency(results), 10);
  output += F(",\n\"duty_cycle\": ");
  output += uint64ToString(duty_cycle(results), 10);
  output += F(",\n\"repeat\": ");
  output += uint64ToString(repeat);
  output += F(",\n\"min_repeat\": ");
  output += uint64ToString(IRsend::minRepeats(results->decode_type));
  output += F(",\n");

  // Now dump "known" codes
  if (results->decode_type != UNKNOWN) {
    if (hasState) {
      // Untested, UCR2 doesn't support AC protocols
#if DECODE_AC
      uint16_t nbytes = ceil(static_cast<float>(results->bits) / 8.0);
      output += F("\"states\":  [");
      for (uint16_t i = 0; i < nbytes; i++) {
        output += F("\"0x");
        if (results->state[i] < 0x10) output += '0';
        output += uint64ToString(results->state[i], 16);
        output += F("\"");
        if (i < nbytes - 1) output += kCommaSpaceStr;
      }
      output += F("],\n");
#endif  // DECODE_AC
    } else {
      // Simple protocols
      // Some protocols have an address &/or command.
      // NOTE: It will ignore the atypical case when a message has been
      // decoded but the address & the command are both 0.
      if (results->address > 0 || results->command > 0) {
        output += F("\"address\": \"0x");
        output += uint64ToString(results->address, 16);
        output += F("\",\n");
        output += F("\"command\": \"0x");
        output += uint64ToString(results->command, 16);
        output += F("\",\n");
      }
      // Most protocols have data
      output += F("\"data\": \"0x");
      output += uint64ToString(results->value, 16);
      output += F("\"\n");
    }
  }

  output += F("}\n");

  return output;
}
