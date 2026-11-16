#include <unity.h>
#include <string.h>

#include "TubeOverlay.h"

using namespace tube_overlay;

static const char kValid[] =
  "{\"ttl\":300,\"tubes\":["
  "{\"color\":[255,0,0],\"effect\":\"pulse\",\"text\":\"nacolada-ops\",\"blink\":true},"
  "{\"color\":[0,0,255],\"effect\":\"breath\"},"
  "null,null,null,null]}";

static Result send(State &s, const char *json, uint32_t now_ms = 0)
{
  return s.handleMessage(json, strlen(json), now_ms);
}

void setUp(void) {}
void tearDown(void) {}

void test_valid_message(void)
{
  State s;
  TEST_ASSERT_EQUAL(Result::Applied, send(s, kValid));
  TEST_ASSERT_EQUAL_UINT8(2, s.activeCount());
  TEST_ASSERT_TRUE(s.tube(0).active);
  TEST_ASSERT_TRUE(s.tube(0).has_color);
  TEST_ASSERT_EQUAL_UINT8(255, s.tube(0).r);
  TEST_ASSERT_EQUAL_UINT8(0, s.tube(0).g);
  TEST_ASSERT_EQUAL_UINT8(0, s.tube(0).b);
  TEST_ASSERT_EQUAL(Effect::Pulse, s.tube(0).effect);
  TEST_ASSERT_TRUE(s.tube(0).blink);
  TEST_ASSERT_EQUAL_STRING("nacolada-ops", s.tube(0).text);
  TEST_ASSERT_EQUAL(Effect::Breath, s.tube(1).effect);
  TEST_ASSERT_EQUAL_UINT8(255, s.tube(1).b);
  TEST_ASSERT_EQUAL_STRING("", s.tube(1).text);
  TEST_ASSERT_FALSE(s.tube(2).active);
}

void test_defaults(void)
{
  State s;
  TEST_ASSERT_EQUAL(Result::Applied, send(s, "{\"tubes\":[{\"color\":[1,2,3]},null,null,null,null,null]}", 1000));
  TEST_ASSERT_EQUAL(Effect::Constant, s.tube(0).effect);
  TEST_ASSERT_FALSE(s.tube(0).blink);
  TEST_ASSERT_FALSE(s.tick(1000 + 299999));
  TEST_ASSERT_EQUAL_UINT8(1, s.activeCount());
  TEST_ASSERT_TRUE(s.tick(1000 + 300000));
  TEST_ASSERT_EQUAL_UINT8(0, s.activeCount());
}

void test_no_color(void)
{
  State s;
  TEST_ASSERT_EQUAL(Result::Applied, send(s, "{\"tubes\":[{\"text\":\"x\"},null,null,null,null,null]}"));
  TEST_ASSERT_TRUE(s.tube(0).active);
  TEST_ASSERT_FALSE(s.tube(0).has_color);
  TEST_ASSERT_EQUAL_STRING("x", s.tube(0).text);
}

void test_clear(void)
{
  State s;
  TEST_ASSERT_EQUAL(Result::Applied, send(s, kValid));
  TEST_ASSERT_EQUAL(Result::Applied, send(s, "{\"tubes\":[null,null,null,null,null,null]}"));
  TEST_ASSERT_EQUAL_UINT8(0, s.activeCount());
}

void test_bad_json(void)
{
  State s;
  TEST_ASSERT_EQUAL(Result::Applied, send(s, kValid));
  TEST_ASSERT_EQUAL(Result::BadJson, send(s, "{\"tubes\":["));
  TEST_ASSERT_EQUAL_UINT8(2, s.activeCount());
  TEST_ASSERT_EQUAL_STRING("nacolada-ops", s.tube(0).text);
}

void test_bad_shape(void)
{
  State s;
  TEST_ASSERT_EQUAL(Result::Applied, send(s, kValid));
  TEST_ASSERT_EQUAL(Result::BadShape, send(s, "{\"tubes\":[null,null,null,null,null]}"));
  TEST_ASSERT_EQUAL(Result::BadShape, send(s, "{\"tubes\":[null,null,null,null,null,null,null]}"));
  TEST_ASSERT_EQUAL(Result::BadShape, send(s, "{\"tubes\":3}"));
  TEST_ASSERT_EQUAL(Result::BadShape, send(s, "{\"ttl\":5}"));
  TEST_ASSERT_EQUAL(Result::BadShape, send(s, "{\"tubes\":[\"x\",null,null,null,null,null]}"));
  TEST_ASSERT_EQUAL_UINT8(2, s.activeCount());
  TEST_ASSERT_EQUAL_STRING("nacolada-ops", s.tube(0).text);
}

void test_bad_value(void)
{
  State s;
  TEST_ASSERT_EQUAL(Result::Applied, send(s, kValid));
  TEST_ASSERT_EQUAL(Result::BadValue, send(s, "{\"tubes\":[{\"color\":[256,0,0]},null,null,null,null,null]}"));
  TEST_ASSERT_EQUAL(Result::BadValue, send(s, "{\"tubes\":[{\"color\":[1,2]},null,null,null,null,null]}"));
  TEST_ASSERT_EQUAL(Result::BadValue, send(s, "{\"tubes\":[{\"effect\":\"rainbow\"},null,null,null,null,null]}"));
  TEST_ASSERT_EQUAL(Result::BadValue, send(s, "{\"ttl\":0,\"tubes\":[null,null,null,null,null,null]}"));
  TEST_ASSERT_EQUAL(Result::BadValue, send(s, "{\"ttl\":1.5,\"tubes\":[null,null,null,null,null,null]}"));
  TEST_ASSERT_EQUAL_UINT8(2, s.activeCount());
  TEST_ASSERT_EQUAL_STRING("nacolada-ops", s.tube(0).text);
}

void test_ttl_cap(void)
{
  State s;
  TEST_ASSERT_EQUAL(Result::Applied, send(s, "{\"ttl\":99999,\"tubes\":[{\"color\":[1,2,3]},null,null,null,null,null]}", 0));
  TEST_ASSERT_FALSE(s.tick(3599999));
  TEST_ASSERT_EQUAL_UINT8(1, s.activeCount());
  TEST_ASSERT_TRUE(s.tick(3600000));
}

void test_ttl_wraparound(void)
{
  State s;
  const uint32_t t0 = 0xFFFFF000u;
  TEST_ASSERT_EQUAL(Result::Applied, send(s, "{\"ttl\":20,\"tubes\":[{\"color\":[1,2,3]},null,null,null,null,null]}", t0));
  TEST_ASSERT_FALSE(s.tick(0x00000100u));
  TEST_ASSERT_EQUAL_UINT8(1, s.activeCount());
  TEST_ASSERT_TRUE(s.tick((uint32_t)(t0 + 20000u)));
}

void test_text_sanitised(void)
{
  State s;
  TEST_ASSERT_EQUAL(Result::Applied, send(s, "{\"tubes\":[{\"text\":\"abcdefghijklmnopqrstuvwxyz0123\"},{\"text\":\"caf\\u00e9\"},null,null,null,null]}"));
  TEST_ASSERT_EQUAL_STRING("abcdefghijklmnopqrst", s.tube(0).text);
  TEST_ASSERT_EQUAL_UINT32(20, strlen(s.tube(0).text));
  TEST_ASSERT_EQUAL_STRING("caf??", s.tube(1).text);
}

void test_blink_phase(void)
{
  State s;
  TEST_ASSERT_EQUAL(Result::Applied, send(s, "{\"tubes\":[{\"text\":\"x\",\"blink\":true},{\"text\":\"y\"},{\"blink\":true},null,null,null]}", 1000));
  TEST_ASSERT_TRUE(s.textVisible(0, 1000));
  TEST_ASSERT_TRUE(s.textVisible(0, 1499));
  TEST_ASSERT_FALSE(s.textVisible(0, 1500));
  TEST_ASSERT_FALSE(s.textVisible(0, 1999));
  TEST_ASSERT_TRUE(s.textVisible(0, 2000));
  // steady text is always visible, no text is never visible
  TEST_ASSERT_TRUE(s.textVisible(1, 1500));
  TEST_ASSERT_FALSE(s.textVisible(2, 1000));
  TEST_ASSERT_FALSE(s.textVisible(3, 1000));
}

void test_lock(void)
{
  State s;
  TEST_ASSERT_EQUAL(Result::Applied, send(s, kValid));
  TEST_ASSERT_EQUAL(Result::LockChanged, send(s, "{\"state\":\"OFF\"}"));
  TEST_ASSERT_TRUE(s.locked());
  TEST_ASSERT_EQUAL_UINT8(0, s.activeCount());
  TEST_ASSERT_EQUAL(Result::IgnoredLocked, send(s, kValid));
  TEST_ASSERT_EQUAL_UINT8(0, s.activeCount());
  TEST_ASSERT_EQUAL(Result::LockChanged, send(s, "{\"state\":\"ON\"}"));
  TEST_ASSERT_FALSE(s.locked());
  TEST_ASSERT_EQUAL(Result::Applied, send(s, kValid));
  TEST_ASSERT_EQUAL_UINT8(2, s.activeCount());
}

void test_revision(void)
{
  State s;
  uint32_t r = s.revision();
  TEST_ASSERT_EQUAL(Result::Applied, send(s, kValid, 0));
  TEST_ASSERT_TRUE(s.revision() > r);
  r = s.revision();
  TEST_ASSERT_EQUAL(Result::BadJson, send(s, "{"));
  TEST_ASSERT_EQUAL(Result::BadShape, send(s, "{\"tubes\":3}"));
  TEST_ASSERT_EQUAL(Result::BadValue, send(s, "{\"ttl\":0,\"tubes\":[null,null,null,null,null,null]}"));
  TEST_ASSERT_FALSE(s.tick(1));
  TEST_ASSERT_EQUAL_UINT32(r, s.revision());
  TEST_ASSERT_TRUE(s.tick(300000));
  TEST_ASSERT_TRUE(s.revision() > r);
  r = s.revision();
  TEST_ASSERT_EQUAL(Result::LockChanged, send(s, "{\"state\":\"OFF\"}"));
  TEST_ASSERT_TRUE(s.revision() > r);
  r = s.revision();
  TEST_ASSERT_EQUAL(Result::IgnoredLocked, send(s, kValid));
  TEST_ASSERT_EQUAL_UINT32(r, s.revision());
}

void test_effect_level(void)
{
  TEST_ASSERT_EQUAL_UINT8(255, effectLevel(Effect::Constant, 12345, 60, 12));
  TEST_ASSERT_EQUAL_UINT8(1, effectLevel(Effect::Pulse, 0, 60, 12));
  TEST_ASSERT_EQUAL_UINT8(255, effectLevel(Effect::Pulse, 250, 60, 12));
  for (uint32_t t = 0; t < 1000; t++)
  {
    TEST_ASSERT_TRUE(effectLevel(Effect::Pulse, t * 37, 60, 12) >= 1);
    TEST_ASSERT_TRUE(effectLevel(Effect::Breath, t * 37, 60, 12) >= 1);
  }
}

void test_scale_channel(void)
{
  TEST_ASSERT_EQUAL_UINT8(255, scaleChannel(255, Effect::Constant, 255, 7));
  TEST_ASSERT_EQUAL_UINT8(1, scaleChannel(255, Effect::Constant, 255, 0));
  TEST_ASSERT_EQUAL_UINT8(128, scaleChannel(255, Effect::Pulse, 128, 7));
  TEST_ASSERT_EQUAL_UINT8(0, scaleChannel(0, Effect::Constant, 255, 7));
  TEST_ASSERT_EQUAL_UINT8(0, scaleChannel(0, Effect::Pulse, 128, 7));
  TEST_ASSERT_EQUAL_UINT8(0, scaleChannel(0, Effect::Breath, 255, 3));
}

int main(int, char **)
{
  UNITY_BEGIN();
  RUN_TEST(test_valid_message);
  RUN_TEST(test_defaults);
  RUN_TEST(test_no_color);
  RUN_TEST(test_clear);
  RUN_TEST(test_bad_json);
  RUN_TEST(test_bad_shape);
  RUN_TEST(test_bad_value);
  RUN_TEST(test_ttl_cap);
  RUN_TEST(test_ttl_wraparound);
  RUN_TEST(test_text_sanitised);
  RUN_TEST(test_blink_phase);
  RUN_TEST(test_lock);
  RUN_TEST(test_revision);
  RUN_TEST(test_effect_level);
  RUN_TEST(test_scale_channel);
  return UNITY_END();
}
