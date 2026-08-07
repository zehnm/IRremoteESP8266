// uccode_to_pronto tool for Unfolded Circle Remote Two.
// Convert an IR hex code to a PRONTO hex code plus IR metadata as JSON.
//
// PRONTO encodes a separate initial (seq1) and repeat (seq2) burst, so the
// output carries exactly one repeat unit; the `min_repeat` field tells the
// client how many times seq2 should be played.
//
// Copyright 2024 Unfolded Circle ApS

#include <iostream>
#include <string>
#include "IRrecv.h"
#include "IRsend.h"
#include "IRsend_test.h"
#include "uccode_common.h"
#include "uccode_to_pronto.h"

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

  // Derive the initial-frame / repeat-frame split, then emit seq1 + one repeat
  // unit. PRONTO repeats seq2 natively, so a single unit is all that's needed.
  UcCapture s = captureStructure(uc, &irsend, &irrecv);
  if (!s.ok) {
    std::cerr << "Failed to send IR code!" << std::endl;
    return 4;
  }

  std::cout << resultToPronto(&s.result, s.unit, s.repeat_index, uc.repeats)
            << std::endl;
  return 0;
}
