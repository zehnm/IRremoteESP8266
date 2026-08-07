// Shared helpers for the Unfolded Circle uccode_to_raw / uccode_to_pronto CLIs.
//
// These host-side tools convert an Unfolded Circle IR "UC_CODE"
// (`<PROTOCOL>;<CODE>;<BITS>;<REPEAT>`) into either raw timing data or a PRONTO
// hex code. The logic lives in this header (as inline functions) so it can be
// exercised directly by the unit tests in test/uccode_*_test.cpp.
//
// Copyright 2021 David Conran (original code_to_raw)
// Copyright 2024 Unfolded Circle ApS

#ifndef TOOLS_UCCODE_COMMON_H_
#define TOOLS_UCCODE_COMMON_H_

#include <math.h>
#include <string>
#include "IRac.h"
#include "IRrecv.h"
#include "IRsend.h"
#include "IRsend_test.h"
#include "IRutils.h"

/// Parsed representation of a UC_CODE string.
struct UcCode {
  decode_type_t type = decode_type_t::UNKNOWN;
  uint64_t code = 0;
  uint16_t nbits = 0;
  uint16_t repeats = 0;
  uint8_t state[kStateSizeMax] = {0};
  uint16_t stateSize = 0;
};

/// Parse a UC_CODE string of the form `<PROTOCOL>;<CODE>;<BITS>;<REPEAT>`.
/// @param[in,out] arg Mutable UC_CODE buffer (split in place on ';').
/// @param[out] out The parsed result.
/// @param[out] err Human readable error message when parsing fails.
/// @return 0 on success, otherwise a non-zero exit code (1 = bad usage/value,
///   3 = non-hexadecimal code).
inline int parseUcCode(char* arg, UcCode* out, std::string* err) {
  // Split the UC_CODE parameter into protocol / code / bits / repeats.
  char* parts[4];
  int partcount = 0;
  parts[partcount++] = arg;
  for (char* ptr = arg; *ptr && partcount < 4; ptr++) {
    if (*ptr == ';') {
      *ptr = 0;
      parts[partcount++] = ptr + 1;
    }
  }
  if (partcount != 4) {
    *err = "Invalid UC_CODE specified";
    return 1;
  }

  out->type = strToDecodeType(parts[0]);
  switch (out->type) {
    case decode_type_t::UNUSED:
    case decode_type_t::UNKNOWN:
    case decode_type_t::GLOBALCACHE:
    case decode_type_t::PRONTO:
    case decode_type_t::RAW:
      *err = "The protocol specified is not supported by this program.";
      return 1;
    default:
      break;
  }

  out->nbits = static_cast<uint16_t>(std::stoul(parts[2]));
  if (out->nbits == 0 || out->nbits > kStateSizeMax * 8) {
    *err = std::string("Nr. of bits ") + parts[2] + " is invalid.";
    return 1;
  }
  out->stateSize = out->nbits / 8;

  out->repeats = static_cast<uint16_t>(std::stoul(parts[3]));
  if (out->repeats > 20) {
    *err = std::string("Repeat count is too large: ") + parts[3]
         + ". Maximum is 20.";
    return 1;
  }

  String hexstr = String(parts[1]);
  uint64_t strOffset = 0;
  if (hexstr.rfind("0x", 0) || hexstr.rfind("0X", 0)) strOffset = 2;
  uint64_t hexstrlength = hexstr.length() - strOffset;

  if (hasACState(out->type)) {
    // AC protocols use a byte-array state. Convert the hex string into it,
    // least-significant byte last (state[stateSize-1] holds the low byte).
    if (hexstrlength > static_cast<uint64_t>(out->stateSize) * 2) {
      *err = std::string("Code ") + parts[1] + " is too long for "
           + std::to_string(out->nbits) + " bits.";
      return 1;
    }
    uint8_t* statePtr = &out->state[out->stateSize - 1];
    for (uint16_t i = 0; i < hexstrlength; i++) {
      uint8_t c = tolower(hexstr[hexstrlength + strOffset - i - 1]);
      if (!isxdigit(c)) {
        *err = std::string("Code ") + parts[1]
             + " contains non-hexidecimal characters.";
        return 3;
      }
      c = isdigit(c) ? (c - '0') : (c - 'a' + 10);
      if (i % 2 == 1) {  // Odd: Upper half of the byte.
        *statePtr += (c << 4);
        statePtr--;
      } else {  // Even: Lower half of the byte.
        *statePtr = c;
      }
    }
  } else {
    // Simple protocols use a single integer value. Reject anything that is not
    // a well-formed hex number (stoull() would silently accept a partial one).
    if (hexstrlength == 0) {
      *err = std::string("Code ") + parts[1] + " is not a valid hex value.";
      return 1;
    }
    for (uint64_t i = 0; i < hexstrlength; i++) {
      if (!isxdigit(static_cast<unsigned char>(hexstr[strOffset + i]))) {
        *err = std::string("Code ") + parts[1]
             + " contains non-hexidecimal characters.";
        return 3;
      }
    }
    try {
      out->code = std::stoull(parts[1], nullptr, 16);
    } catch (const std::exception&) {
      *err = std::string("Code ") + parts[1] + " is not a valid hex value.";
      return 3;
    }
  }
  return 0;
}

/// Return the carrier frequency (Hz) of a given protocol.
/// Values have been pulled out of the individual IR decoders.
inline uint32_t ucCodeFrequency(const decode_results* const results) {
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

/// Return the duty cycle (%) of a given protocol.
/// Values have been pulled out of the individual IR decoders.
inline uint8_t ucCodeDutyCycle(const decode_results* const results) {
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

/// Send `sendRepeats` repeats of the parsed code and capture the raw timing.
/// @param[in] uc The parsed UC_CODE.
/// @param[in] sendRepeats Nr. of repeat frames to request (non-AC protocols).
///   The library enforces the protocol's minimum, so the captured frame count
///   may be higher.
/// @param[in,out] irsend Test sender used to generate the signal.
/// @param[in,out] irrecv Receiver used to normalise the capture.
/// @param[out] out The captured, decoded result with `repeat_index` preserved.
/// @return true on success, false if the protocol could not be sent.
inline bool captureUcCode(const UcCode& uc, uint16_t sendRepeats,
                          IRsendTest* irsend, IRrecv* irrecv,
                          decode_results* out) {
  irsend->begin();
  irsend->reset();

  bool ok;
  if (hasACState(uc.type)) {
    ok = irsend->send(uc.type, uc.state, uc.stateSize);
  } else {
    ok = irsend->send(uc.type, uc.code, uc.nbits, sendRepeats);
  }
  if (!ok) return false;

  irsend->makeDecodeResult();
  irrecv->decode(&irsend->capture);

  *out = irsend->capture;
  out->decode_type = uc.type;
  if (!hasACState(uc.type)) {
    out->value = uc.code;
    out->bits = uc.nbits;
  }
  return true;
}

/// Repeat structure of a code, derived without any library instrumentation.
struct UcCapture {
  // Capture holding the initial frame plus >=1 repeat frame (rawbuf usable).
  // Valid only when ok == true.
  decode_results result;
  uint16_t unit = 0;          ///< Length of one repeat frame (rawbuf entries).
  uint16_t repeat_index = 0;  ///< rawbuf index where the first repeat begins.
  bool ok = false;
};

/// Determine a code's repeat structure by capturing it at two repeat counts
/// that differ by exactly one repeat frame. The length delta is one repeat
/// unit; subtracting the protocol minimum yields the initial frame length. This
/// is protocol-agnostic (works for full-frame repeats, NEC/JVC-style short
/// repeats, and header-once protocols like DISH) and assumes, as every
/// supported protocol does, that repeat frames are identical to each other.
/// @param[in] uc The parsed UC_CODE.
/// @param[in,out] irsend Test sender.
/// @param[in,out] irrecv Receiver.
/// @return The structure. `result` holds the `minRep + 1` capture; its rawbuf
///   contains the initial frame followed by at least one repeat frame.
inline UcCapture captureStructure(const UcCode& uc, IRsendTest* irsend,
                                  IRrecv* irrecv) {
  UcCapture info;
  const uint16_t minRep =
      hasACState(uc.type) ? 0 : IRsend::minRepeats(uc.type);

  decode_results a;
  if (!captureUcCode(uc, minRep, irsend, irrecv, &a)) return info;
  const uint16_t lenA = a.rawlen;
  // captureUcCode() resets the sender, so snapshot lenA before the next call.
  if (!captureUcCode(uc, minRep + 1, irsend, irrecv, &info.result)) return info;
  const uint16_t lenB = info.result.rawlen;

  info.unit = (lenB > lenA) ? (lenB - lenA) : 0;
  // Initial-frame entries = (data entries in A) - (minRep repeat units).
  uint32_t initial = (lenA >= 1) ? (lenA - 1) : 0;
  const uint32_t repeats_total = static_cast<uint32_t>(minRep) * info.unit;
  if (initial > repeats_total) initial -= repeats_total;
  info.repeat_index = static_cast<uint16_t>(1 + initial);  // rawbuf coord
  info.ok = true;
  return info;
}

/// Append the shared metadata JSON fields (protocol .. repeat_index).
/// @param[in,out] output The JSON string being built.
/// @param[in] results The captured result.
/// @param[in] repeat The requested repeat count (echoed verbatim).
/// @param[in] repeat_index 0-based index into the emitted sequence where the
///   repeat starts (== sequence length when there is no repeat).
inline void appendMetadata(String* output, const decode_results* const results,
                           const uint16_t repeat, const uint16_t repeat_index) {
  *output += F("\"protocol\": \"");
  *output += typeToString(results->decode_type);
  *output += F("\",\n\"bits\": ");
  *output += uint64ToString(results->bits, 10);
  *output += F(",\n\"frequency\": ");
  *output += uint64ToString(ucCodeFrequency(results), 10);
  *output += F(",\n\"duty_cycle\": ");
  *output += uint64ToString(ucCodeDutyCycle(results), 10);
  *output += F(",\n\"repeat\": ");
  *output += uint64ToString(repeat);
  *output += F(",\n\"min_repeat\": ");
  *output += uint64ToString(IRsend::minRepeats(results->decode_type));
  *output += F(",\n\"repeat_index\": ");
  *output += uint64ToString(repeat_index);
  *output += F(",\n");
}

/// Append the "known codes" JSON fields (states / address / command / data).
/// @param[in,out] output The JSON string being built.
/// @param[in] results The captured result.
inline void appendKnownCodes(String* output,
                             const decode_results* const results) {
  if (results->decode_type == UNKNOWN) return;
  if (hasACState(results->decode_type)) {
#if DECODE_AC
    uint16_t nbytes = ceil(static_cast<float>(results->bits) / 8.0);
    *output += F("\"states\":  [");
    for (uint16_t i = 0; i < nbytes; i++) {
      *output += F("\"0x");
      if (results->state[i] < 0x10) *output += '0';
      *output += uint64ToString(results->state[i], 16);
      *output += F("\"");
      if (i < nbytes - 1) *output += kCommaSpaceStr;
    }
    *output += F("]\n");
#endif  // DECODE_AC
  } else {
    if (results->address > 0 || results->command > 0) {
      *output += F("\"address\": \"0x");
      *output += uint64ToString(results->address, 16);
      *output += F("\",\n");
      *output += F("\"command\": \"0x");
      *output += uint64ToString(results->command, 16);
      *output += F("\",\n");
    }
    *output += F("\"data\": \"0x");
    *output += uint64ToString(results->value, 16);
    *output += F("\"\n");
  }
}

#endif  // TOOLS_UCCODE_COMMON_H_
