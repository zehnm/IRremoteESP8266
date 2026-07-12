// PRONTO hex-code formatter for the Unfolded Circle uccode_to_pronto tool.
//
// PRONTO natively encodes a separate initial (seq1) and repeat (seq2) burst;
// the player repeats only seq2. We therefore emit exactly ONE repeat unit as
// seq2, regardless of how many repeat frames the library minimum produced.
//
// Copyright 2024 Unfolded Circle ApS

#ifndef TOOLS_UCCODE_TO_PRONTO_H_
#define TOOLS_UCCODE_TO_PRONTO_H_

#include <math.h>
#include <stdio.h>
#include "IRrecv.h"
#include "IRsend.h"
#include "IRutils.h"
#include "uccode_common.h"

/// PRONTO carrier frequency code: round(1000000 / (hertz * 0.241246)).
inline uint16_t ucProntoFreqCode(const uint32_t hertz) {
  return static_cast<uint16_t>(round(1000000.0 / (hertz * 0.241246)));
}

/// Convert a duration in microseconds to PRONTO carrier-cycle units, clamped to
/// the 16-bit range PRONTO can represent.
inline uint16_t usecsToPronto(const uint32_t usecs, const float period) {
  const double value = round(usecs / period);
  return (value > 65535.0) ? 0xFFFF : static_cast<uint16_t>(value);
}

/// Format an initial (seq1) and repeat (seq2) burst as a PRONTO hex string.
/// @param[in] rawbuf The capture buffer (rawbuf[0] is the lead-in gap).
/// @param[in] seq1_start First rawbuf index of the initial burst.
/// @param[in] seq1_count Nr. of rawbuf entries in the initial burst.
/// @param[in] seq2_start First rawbuf index of the repeat burst.
/// @param[in] seq2_count Nr. of rawbuf entries in the repeat burst.
/// @param[in] hertz Carrier frequency in Hz.
/// @return The space-separated, uppercase PRONTO hex code.
inline String rawToPronto(const volatile uint16_t* rawbuf,
                          const uint16_t seq1_start, const uint16_t seq1_count,
                          const uint16_t seq2_start, const uint16_t seq2_count,
                          const uint32_t hertz) {
  const float period = 1000000.0 / static_cast<float>(hertz);
  char buf[16];
  String result = "";

  // Header: 0000 <freq> <seq1 pairs> <seq2 pairs>
  snprintf(buf, sizeof(buf), "0000 %04X", ucProntoFreqCode(hertz));
  result += buf;
  snprintf(buf, sizeof(buf), " %04X",
           static_cast<uint16_t>(seq1_count / 2));
  result += buf;
  snprintf(buf, sizeof(buf), " %04X",
           static_cast<uint16_t>(seq2_count / 2));
  result += buf;

  for (uint16_t i = 0; i < seq1_count; i++) {
    snprintf(buf, sizeof(buf), " %04X",
             usecsToPronto(rawbuf[seq1_start + i] * kRawTick, period));
    result += buf;
  }
  for (uint16_t i = 0; i < seq2_count; i++) {
    snprintf(buf, sizeof(buf), " %04X",
             usecsToPronto(rawbuf[seq2_start + i] * kRawTick, period));
    result += buf;
  }
  return result;
}

/// Return a JSON string with the PRONTO code for a captured result.
/// seq1 is the initial frame; seq2 is a single repeat unit. The `min_repeat`
/// field tells the client how often to play seq2.
/// @param[in] results The `minRep + 1` capture (rawbuf holds the initial frame
///   plus at least one repeat frame).
/// @param[in] unit Length of one repeat frame in rawbuf entries (0 if none).
/// @param[in] repeat_index_rawbuf rawbuf index where the first repeat begins.
/// @param[in] repeat The requested repeat count (echoed into the output).
/// @return The JSON formatted string.
inline String resultToPronto(const decode_results* const results,
                            const uint16_t unit,
                            const uint16_t repeat_index_rawbuf,
                            const uint16_t repeat) {
  String output = "";
  const uint32_t hertz = ucCodeFrequency(results);
  const uint16_t rawlen = results->rawlen;
  const uint16_t ri = repeat_index_rawbuf;

  uint16_t seq1_start = 1, seq1_count = 0;
  uint16_t seq2_start = 0, seq2_count = 0;
  if (unit > 0 && ri > 1 && ri + unit <= rawlen) {
    seq1_count = ri - 1;          // initial frame: rawbuf[1 .. ri)
    seq2_start = ri;
    seq2_count = unit;            // one repeat frame: rawbuf[ri .. ri + unit)
  } else {
    // No distinct repeat captured: emit the frame as seq1 only, dropping the
    // final trailing gap so seq1 is complete mark/space pairs.
    seq1_count = (rawlen > 2) ? rawlen - 2 : 0;
  }

  // 0-based repeat offset into the (seq1+seq2) sequence.
  const uint16_t repeat_index = seq1_count;

  output += F("{\n");
  appendMetadata(&output, results, repeat, repeat_index);
  output += F("\"pronto\": \"");
  output += rawToPronto(results->rawbuf, seq1_start, seq1_count,
                        seq2_start, seq2_count, hertz);
  output += F("\",\n");
  appendKnownCodes(&output, results);
  output += F("}\n");
  return output;
}

#endif  // TOOLS_UCCODE_TO_PRONTO_H_
