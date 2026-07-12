// Copyright 2024 Unfolded Circle ApS
// Unit tests for the uccode_to_pronto tool (PRONTO hex code conversion).

#include <string.h>
#include <sstream>
#include <string>
#include <vector>
#include "IRrecv.h"
#include "IRsend.h"
#include "IRsend_test.h"
#include "../tools/uccode_common.h"
#include "../tools/uccode_to_pronto.h"
#include "gtest/gtest.h"

// --- Helpers ---------------------------------------------------------------

static int parse(const char* code, UcCode* uc, std::string* err) {
  char buf[128];
  strncpy(buf, code, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = 0;
  return parseUcCode(buf, uc, err);
}

// Full pipeline: parse -> structure -> pronto JSON.
static std::string prontoJson(const char* code) {
  UcCode uc;
  std::string err;
  EXPECT_EQ(0, parse(code, &uc, &err)) << err;
  IRsendTest irsend(kGpioUnused);
  IRrecv irrecv(kGpioUnused);
  UcCapture s = captureStructure(uc, &irsend, &irrecv);
  EXPECT_TRUE(s.ok);
  return resultToPronto(&s.result, s.unit, s.repeat_index, uc.repeats);
}

// Extract the space-separated tokens of the "pronto" field value.
static std::vector<std::string> prontoTokens(const std::string& json) {
  size_t key = json.find("\"pronto\": \"");
  EXPECT_NE(std::string::npos, key);
  size_t start = key + strlen("\"pronto\": \"");
  size_t end = json.find('"', start);
  std::istringstream ss(json.substr(start, end - start));
  std::vector<std::string> out;
  for (std::string tok; ss >> tok;) out.push_back(tok);
  return out;
}

static uint32_t hexOf(const std::string& s) {
  return std::stoul(s, nullptr, 16);
}

// --- Low-level conversions --------------------------------------------------

TEST(ProntoConvert, FrequencyCode) {
  EXPECT_EQ(0x006D, ucProntoFreqCode(38000));  // 109
  EXPECT_EQ(0x0068, ucProntoFreqCode(40000));  // 104
  EXPECT_EQ(0x0073, ucProntoFreqCode(36000));  // 115
}

TEST(ProntoConvert, UsecsToProntoUnitsAndClamp) {
  const float period = 1000000.0 / 38000.0;  // ~26.3 us
  EXPECT_EQ(0u, usecsToPronto(0, period));
  EXPECT_EQ(38u, usecsToPronto(1000, period));  // round(1000/26.3)
  // Durations beyond the 16-bit PRONTO range are clamped.
  EXPECT_EQ(0xFFFFu, usecsToPronto(0xFFFFFFFF, period));
}

// --- resultToPronto ---------------------------------------------------------

TEST(ProntoResult, HeaderFieldsAndValidJson) {
  std::string json = prontoJson("3;0x807F00FF;32;1");  // NEC, 38kHz
  EXPECT_NE(std::string::npos, json.find("\"protocol\": \"NEC\""));
  EXPECT_NE(std::string::npos, json.find("\"min_repeat\": "));
  std::vector<std::string> t = prontoTokens(json);
  ASSERT_GE(t.size(), 4u);
  EXPECT_EQ("0000", t[0]);            // raw PRONTO type
  EXPECT_EQ(0x006Du, hexOf(t[1]));    // 38kHz frequency code
}

TEST(ProntoResult, PairCountsMatchHeader) {
  std::string json = prontoJson("5;0x4004BF40;48;1");  // Panasonic
  std::vector<std::string> t = prontoTokens(json);
  ASSERT_GE(t.size(), 4u);
  uint32_t seq1 = hexOf(t[2]);
  uint32_t seq2 = hexOf(t[3]);
  EXPECT_EQ(2 * (seq1 + seq2), t.size() - 4);  // 2 values per pair
}

TEST(ProntoResult, FullFrameRepeatIsIdentical) {
  // Panasonic repeats the whole frame, so seq1 and seq2 are identical.
  std::string json = prontoJson("5;0x4004BF40;48;1");
  std::vector<std::string> t = prontoTokens(json);
  uint32_t seq1 = hexOf(t[2]);
  uint32_t seq2 = hexOf(t[3]);
  ASSERT_EQ(seq1, seq2);
  ASSERT_GT(seq1, 0u);
  for (uint32_t i = 0; i < 2 * seq1; i++)
    EXPECT_EQ(t[4 + i], t[4 + 2 * seq1 + i]) << "mismatch at pair value " << i;
}

TEST(ProntoResult, SpecialRepeatSeq2ShorterThanSeq1) {
  // NEC's repeat frame is shorter than the initial frame.
  std::string json = prontoJson("3;0x807F00FF;32;1");
  std::vector<std::string> t = prontoTokens(json);
  uint32_t seq1 = hexOf(t[2]);
  uint32_t seq2 = hexOf(t[3]);
  EXPECT_GT(seq1, 0u);
  EXPECT_GT(seq2, 0u);
  EXPECT_LT(seq2, seq1);
}

TEST(ProntoResult, SonyMinRepeatYieldsSingleUnit) {
  // Sony (minRepeats == 2) must still emit exactly one repeat unit == seq1.
  std::string json = prontoJson("4;0x10;12;1");
  std::vector<std::string> t = prontoTokens(json);
  uint32_t seq1 = hexOf(t[2]);
  uint32_t seq2 = hexOf(t[3]);
  EXPECT_EQ(seq1, seq2);
  EXPECT_EQ(0x0068u, hexOf(t[1]));  // Sony is 40kHz
}
