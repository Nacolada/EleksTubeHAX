#include <unity.h>
#include <string.h>

#include "BacklightColor.h"

using namespace backlight_color;

void setUp() {}
void tearDown() {}

static void assertRoundTrip(const FreeColor &in)
{
  uint8_t blob[kBlobSize];
  encode(in, blob);
  FreeColor out = {!in.active, 1, 2, 3};
  TEST_ASSERT_TRUE(decode(blob, kBlobSize, out));
  TEST_ASSERT_EQUAL(in.active, out.active);
  TEST_ASSERT_EQUAL_UINT8(in.r, out.r);
  TEST_ASSERT_EQUAL_UINT8(in.g, out.g);
  TEST_ASSERT_EQUAL_UINT8(in.b, out.b);
}

void test_round_trip_active_warm_white()
{
  assertRoundTrip({true, 255, 147, 41});
}

void test_round_trip_inactive()
{
  assertRoundTrip({false, 0, 0, 0});
}

void test_encode_starts_with_magic()
{
  uint8_t blob[kBlobSize];
  encode({true, 1, 2, 3}, blob);
  TEST_ASSERT_EQUAL_UINT8(kMagic, blob[0]);
}

static void assertRejected(const uint8_t *blob, size_t len)
{
  FreeColor c = {true, 10, 20, 30};
  TEST_ASSERT_FALSE(decode(blob, len, c));
  TEST_ASSERT_TRUE(c.active);
  TEST_ASSERT_EQUAL_UINT8(10, c.r);
  TEST_ASSERT_EQUAL_UINT8(20, c.g);
  TEST_ASSERT_EQUAL_UINT8(30, c.b);
}

void test_decode_rejects_wrong_length()
{
  uint8_t blob[kBlobSize + 1];
  encode({true, 255, 147, 41}, blob);
  blob[kBlobSize] = 0;
  assertRejected(blob, 0);
  assertRejected(blob, kBlobSize - 1);
  assertRejected(blob, kBlobSize + 1);
}

void test_decode_rejects_wrong_magic()
{
  uint8_t blob[kBlobSize];
  encode({true, 255, 147, 41}, blob);
  blob[0] = kMagic ^ 0xFF;
  assertRejected(blob, kBlobSize);
  blob[0] = 0;
  assertRejected(blob, kBlobSize);
}

int main(int, char **)
{
  UNITY_BEGIN();
  RUN_TEST(test_round_trip_active_warm_white);
  RUN_TEST(test_round_trip_inactive);
  RUN_TEST(test_encode_starts_with_magic);
  RUN_TEST(test_decode_rejects_wrong_length);
  RUN_TEST(test_decode_rejects_wrong_magic);
  return UNITY_END();
}
