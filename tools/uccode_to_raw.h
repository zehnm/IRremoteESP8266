// Raw-timing JSON formatter for the Unfolded Circle uccode_to_raw tool.
//
// Copyright 2021 David Conran (original code_to_raw)
// Copyright 2024 Unfolded Circle ApS

#ifndef TOOLS_UCCODE_TO_RAW_H_
#define TOOLS_UCCODE_TO_RAW_H_

#include "IRrecv.h"
#include "uccode_common.h"

/// Return a JSON string with the raw timing data for a captured code.
/// The `raw` array contains the initial frame followed by all requested repeat
/// frames (the library enforces each protocol's minimum). `repeat_index` is the
/// 0-based offset into that array where the repeat portion begins, or the array
/// length when the protocol has no distinct repeat.
/// @param[in] results The captured result.
/// @param[in] repeat_index_rawbuf rawbuf index where the first repeat begins.
/// @param[in] repeat The requested repeat count (echoed into the output).
/// @return The JSON formatted string.
inline String resultToRaw(const decode_results* const results,
                          const uint16_t repeat_index_rawbuf,
                          const uint16_t repeat) {
  String output = "";
  const uint16_t rawlen = results->rawlen;
  // Emit rawbuf[1 .. rawlen-1); the final entry is the trailing inter-message
  // gap, which is intentionally dropped so the array ends on the last mark
  // (the usual raw-capture convention). Note: unlike code generation, the raw
  // dump prints microseconds directly, so getCorrectedRawLength()'s over-large
  // expansion must NOT be used as the bound (it would read past the buffer).
  const uint16_t emit_end = (rawlen > 1) ? rawlen - 1 : 1;
  const uint16_t raw_count = emit_end - 1;

  // Convert the rawbuf-based repeat_index (1-based, rawbuf[0] is the lead-in
  // gap) into a 0-based index into the emitted `raw` array.
  const uint16_t ri = repeat_index_rawbuf;
  const uint16_t repeat_index =
      (ri > 1 && ri < rawlen) ? (ri - 1) : raw_count;

  output += F("{\n\"raw\": [");
  for (uint16_t i = 1; i < emit_end; i++) {
    if (i > 1) output += kCommaSpaceStr;
    output += uint64ToString(results->rawbuf[i] * kRawTick, 10);
  }
  output += F("],\n");

  appendMetadata(&output, results, repeat, repeat_index);
  appendKnownCodes(&output, results);
  output += F("}\n");
  return output;
}

#endif  // TOOLS_UCCODE_TO_RAW_H_
