// Custom version of the code_to_raw tool for Unfolded Circle Remote Two.
// Convert an IR hex code to raw timing and additional IR information as JSON.
// The raw array contains the initial frame plus all requested repeat frames.
//
// Original file header of code_to_raw:
// Quick and dirty tool to convert a protocol's (hex) codes to raw timings.
// Copyright 2021 David Conran

#include <iostream>
#include <string>
#include "IRrecv.h"
#include "IRsend.h"
#include "IRsend_test.h"
#include "uccode_common.h"
#include "uccode_to_raw.h"

void usage_error(char *name) {
  std::cerr << "Usage: " << name << " UC_CODE" << std::endl;
  std::cerr << std::endl;
  std::cerr << "  UC_CODE: <PROTOCOL>;<CODE>;<BITS>;<REPEAT>" << std::endl;
  std::cerr << std::endl;
  std::cerr << "  Example: " << name << " \"12;0xE242;16;2\"" << std::endl;
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    usage_error(argv[0]);
    return 1;
  }

  UcCode uc;
  std::string err;
  int rc = parseUcCode(argv[1], &uc, &err);
  if (rc != 0) {
    std::cerr << err << std::endl;
    return rc;
  }

  IRsendTest irsend(kGpioUnused);
  IRrecv irrecv(kGpioUnused);

  // Derive the repeat boundary (initial-frame length) independently of how many
  // repeats the client asked for.
  UcCapture s = captureStructure(uc, &irsend, &irrecv);
  if (!s.ok) {
    std::cerr << "Failed to send IR code!" << std::endl;
    return 4;
  }

  // Capture the frame with the requested repeat count. The library bumps this
  // up to the protocol's minimum, so the raw array holds every frame the client
  // needs; captureStructure's boundary still applies (same initial frame).
  decode_results capture;
  if (!captureUcCode(uc, uc.repeats, &irsend, &irrecv, &capture)) {
    std::cerr << "Failed to send IR code!" << std::endl;
    return 4;
  }

  std::cout << resultToRaw(&capture, s.repeat_index, uc.repeats) << std::endl;
  return 0;
}
