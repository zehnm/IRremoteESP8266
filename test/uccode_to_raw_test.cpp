// Copyright 2024 Unfolded Circle ApS
// Unit tests for the uccode_to_raw tool (raw timing JSON conversion).

#include <string.h>
#include <string>
#include "IRrecv.h"
#include "IRsend.h"
#include "IRsend_test.h"
#include "../tools/uccode_common.h"
#include "../tools/uccode_to_raw.h"
#include "gtest/gtest.h"

// --- Helpers ---------------------------------------------------------------

// parseUcCode() mutates its argument, so copy the literal into a buffer first.
static int parse(const char* code, UcCode* uc, std::string* err) {
  char buf[128];
  strncpy(buf, code, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = 0;
  return parseUcCode(buf, uc, err);
}

// Full pipeline: parse -> structure -> capture -> raw JSON.
static std::string rawJson(const char* code) {
  UcCode uc;
  std::string err;
  EXPECT_EQ(0, parse(code, &uc, &err)) << err;
  IRsendTest irsend(kGpioUnused);
  IRrecv irrecv(kGpioUnused);
  UcCapture s = captureStructure(uc, &irsend, &irrecv);
  EXPECT_TRUE(s.ok);
  decode_results cap;
  EXPECT_TRUE(captureUcCode(uc, uc.repeats, &irsend, &irrecv, &cap));
  return resultToRaw(&cap, s.repeat_index, uc.repeats);
}

// --- parseUcCode ------------------------------------------------------------

TEST(UccodeParse, ValidSimpleCode) {
  UcCode uc;
  std::string err;
  ASSERT_EQ(0, parse("3;0x807F00FF;32;1", &uc, &err)) << err;
  EXPECT_EQ(decode_type_t::NEC, uc.type);
  EXPECT_EQ(0x807F00FFULL, uc.code);
  EXPECT_EQ(32, uc.nbits);
  EXPECT_EQ(1, uc.repeats);
}

TEST(UccodeParse, RejectsWrongFieldCount) {
  UcCode uc;
  std::string err;
  EXPECT_EQ(1, parse("3;0x807F00FF;32", &uc, &err));
  EXPECT_FALSE(err.empty());
}

TEST(UccodeParse, RejectsUnsupportedProtocols) {
  UcCode uc;
  std::string err;
  // PRONTO / RAW / GLOBALCACHE are not convertible by this tool.
  EXPECT_EQ(1, parse("PRONTO;0x10;32;1", &uc, &err));
  EXPECT_EQ(1, parse("RAW;0x10;32;1", &uc, &err));
  EXPECT_EQ(1, parse("GLOBALCACHE;0x10;32;1", &uc, &err));
}

TEST(UccodeParse, RejectsZeroBits) {
  UcCode uc;
  std::string err;
  EXPECT_EQ(1, parse("3;0x807F00FF;0;1", &uc, &err));
}

TEST(UccodeParse, RejectsTooManyRepeats) {
  UcCode uc;
  std::string err;
  EXPECT_EQ(1, parse("3;0x807F00FF;32;21", &uc, &err));
}

TEST(UccodeParse, RejectsNonHexCode) {
  UcCode uc;
  std::string err;
  EXPECT_EQ(3, parse("3;0xZZZZ;32;1", &uc, &err));
}

// Regression: a non-AC hex code longer than nbits/8 bytes must not overrun the
// state[] buffer (previously corrupted the repeats field).
TEST(UccodeParse, LongNonAcCodeDoesNotCorruptRepeats) {
  UcCode uc;
  std::string err;
  ASSERT_EQ(0, parse("10;0x4B4AE51;28;1", &uc, &err)) << err;  // LG, 7 nibbles
  EXPECT_EQ(1, uc.repeats);
  EXPECT_EQ(0x4B4AE51ULL, uc.code);
}

// --- captureStructure -------------------------------------------------------

static UcCapture structureOf(const char* code) {
  UcCode uc;
  std::string err;
  EXPECT_EQ(0, parse(code, &uc, &err)) << err;
  IRsendTest irsend(kGpioUnused);
  IRrecv irrecv(kGpioUnused);
  return captureStructure(uc, &irsend, &irrecv);
}

TEST(UccodeStructure, FullFrameRepeatUnitEqualsInitial) {
  // Panasonic repeats the whole frame; one repeat unit == the initial frame.
  UcCapture s = structureOf("5;0x4004BF40;48;1");
  ASSERT_TRUE(s.ok);
  EXPECT_GT(s.unit, 0);
  EXPECT_EQ(s.unit, s.repeat_index - 1);
}

TEST(UccodeStructure, SonyMinTwoIsolatesOneUnit) {
  // Sony has minRepeats == 2; the diff must still isolate a single frame.
  UcCapture s = structureOf("4;0x10;12;1");
  ASSERT_TRUE(s.ok);
  EXPECT_GT(s.unit, 0);
  EXPECT_EQ(s.unit, s.repeat_index - 1);  // Sony repeat frame == initial frame
}

TEST(UccodeStructure, NecRepeatUnitShorterThanInitial) {
  // NEC's repeat is a short (0-data) frame, distinct from the initial frame.
  UcCapture s = structureOf("3;0x807F00FF;32;1");
  ASSERT_TRUE(s.ok);
  EXPECT_GT(s.unit, 0);
  EXPECT_LT(s.unit, s.repeat_index - 1);
}

TEST(UccodeStructure, JvcRepeatDetected) {
  // JVC sends its repeat via a separate call; the diff approach still finds it.
  UcCapture s = structureOf("6;0x10;16;1");
  ASSERT_TRUE(s.ok);
  EXPECT_GT(s.unit, 0);
}

// --- resultToRaw ------------------------------------------------------------

TEST(UccodeRaw, ValidJsonNoTrailingComma) {
  std::string json = rawJson("3;0x807F00FF;32;1");
  EXPECT_EQ(0u, json.find("{\n\"raw\": ["));
  EXPECT_NE(std::string::npos, json.find("],\n\"protocol\": \"NEC\""));
  // The trailing-comma bug produced "..., ]"; make sure it is gone.
  EXPECT_EQ(std::string::npos, json.find(", ]"));
  EXPECT_NE(std::string::npos, json.find("\"repeat_index\": "));
  EXPECT_NE(std::string::npos, json.find("\"min_repeat\": "));
}

TEST(UccodeRaw, EchoesRequestedRepeatCount) {
  std::string json = rawJson("5;0x4004BF40;48;3");
  EXPECT_NE(std::string::npos, json.find("\"repeat\": 3,"));
}

TEST(UccodeRaw, NoRepeatRequestedStillValid) {
  // repeats == 0: still valid JSON, no trailing comma.
  std::string json = rawJson("5;0x4004BF40;48;0");
  EXPECT_EQ(std::string::npos, json.find(", ]"));
  EXPECT_NE(std::string::npos, json.find("\"repeat\": 0,"));
}

TEST(UccodeRaw, RawArrayEndsOnAMark) {
  // The final trailing gap is dropped, so the last raw value is a small mark,
  // not a multi-millisecond gap.
  std::string json = rawJson("3;0x807F00FF;32;1");
  size_t close = json.find("],");
  ASSERT_NE(std::string::npos, close);
  size_t last_comma = json.rfind(", ", close);
  ASSERT_NE(std::string::npos, last_comma);
  std::string last_val = json.substr(last_comma + 2, close - last_comma - 2);
  EXPECT_LT(std::stoul(last_val), 10000u);  // a mark, not an inter-frame gap
}
