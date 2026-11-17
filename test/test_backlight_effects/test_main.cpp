#include <unity.h>
#include <string.h>

#include "BacklightEffects.h"

using namespace backlight_effects;

static const Rgb kRed = {255, 0, 0};
static const Rgb kGreen = {0, 255, 0};
static const Rgb kGold = {255, 170, 0};
static const Rgb kWhite = {255, 255, 255};
static const Rgb kGarland[4] = {kRed, kGreen, kGold, kWhite};

static const Effect kAllEffects[6] = {
  Effect::Fireplace, Effect::Fireworks, Effect::Christmas,
  Effect::Multicolor, Effect::MulticolorChase, Effect::MulticolorBreath};

static bool same(const Rgb &a, const Rgb &b)
{
  return a.r == b.r && a.g == b.g && a.b == b.b;
}

static bool inGarland(const Rgb &p)
{
  for (uint8_t i = 0; i < 4; i++)
  {
    if (same(p, kGarland[i]))
    {
      return true;
    }
  }
  return false;
}

static uint8_t countTrue(const bool *seen, size_t n)
{
  uint8_t c = 0;
  for (size_t i = 0; i < n; i++)
  {
    c += seen[i] ? 1 : 0;
  }
  return c;
}

void setUp(void) {}
void tearDown(void) {}

void test_multicolor_fixed(void)
{
  Engine e(1);
  Rgb a[kMaxLeds], b[kMaxLeds];
  e.render(Effect::Multicolor, 0, 20, a, 6);
  e.render(Effect::Multicolor, 123456, 20, b, 6);
  TEST_ASSERT_EQUAL_MEMORY(a, b, 6 * sizeof(Rgb));
  TEST_ASSERT_TRUE(same(a[0], kRed));
  for (uint8_t i = 0; i < 6; i++)
  {
    TEST_ASSERT_TRUE(same(a[i], multicolorPalette(i)));
    for (uint8_t j = i + 1; j < 6; j++)
    {
      TEST_ASSERT_FALSE(same(a[i], a[j]));
    }
  }
}

// Chase colour of LED i at the very start of a step: palette index (i - step) mod 6.
static Rgb chasePure(uint8_t i, uint32_t step)
{
  return multicolorPalette((uint8_t)(i % 6 + 6 - step % 6));
}

static bool within1(uint8_t a, uint8_t b)
{
  return (a > b ? a - b : b - a) <= 1;
}

static void check_chase(uint8_t count)
{
  Engine e(1);
  Rgb f[kMaxLeds];
  // Steps 0..7 cover the wrap of the palette; a large step checks there is no overflow.
  const uint32_t steps[] = {0, 1, 2, 5, 6, 7, UINT32_MAX / kChaseStepMs - 1};
  for (uint32_t step : steps)
  {
    uint32_t t = step * kChaseStepMs;
    e.render(Effect::MulticolorChase, t, 20, f, count);
    for (uint8_t i = 0; i < count; i++)
    {
      Rgb a = chasePure(i, step);
      Rgb b = chasePure(i, step + 1); // the colour the LED glides to
      TEST_ASSERT_TRUE(same(f[i], a));
      e.render(Effect::MulticolorChase, t + kChaseStepMs / 2, 20, f, count);
      TEST_ASSERT_TRUE(within1(f[i].r, (a.r + b.r) / 2));
      TEST_ASSERT_TRUE(within1(f[i].g, (a.g + b.g) / 2));
      TEST_ASSERT_TRUE(within1(f[i].b, (a.b + b.b) / 2));
      e.render(Effect::MulticolorChase, t + kChaseStepMs, 20, f, count);
      TEST_ASSERT_TRUE(same(f[i], b));
      e.render(Effect::MulticolorChase, t, 20, f, count);
    }
  }
}

void test_chase_cross_fades_6(void)
{
  check_chase(6);
}

void test_chase_cross_fades_34(void)
{
  check_chase(34);
}

void test_chase_never_pauses(void)
{
  Engine e(1);
  Rgb a[kMaxLeds], b[kMaxLeds];
  for (uint32_t t = 1000; t < 1000 + 2 * kChaseStepMs; t += 50)
  {
    e.render(Effect::MulticolorChase, t, 20, a, 6);
    e.render(Effect::MulticolorChase, t + 50, 20, b, 6);
    TEST_ASSERT_FALSE(same(a[0], b[0]));
  }
}

void test_breath_keeps_hue_and_varies_level(void)
{
  Engine e(1);
  Rgb f[kMaxLeds];
  bool seen[256] = {false};
  for (uint32_t t = 0; t < 60000 / 20; t += 50)
  {
    e.render(Effect::MulticolorBreath, t, 20, f, 6);
    TEST_ASSERT_EQUAL_UINT8(0, f[0].g);
    TEST_ASSERT_EQUAL_UINT8(0, f[0].b);
    TEST_ASSERT_TRUE(f[0].r >= 1);
    seen[f[0].r] = true;
  }
  TEST_ASSERT_TRUE(countTrue(seen, 256) >= 20);
}

void test_breath_zero_bpm_is_safe(void)
{
  Engine e(1);
  Rgb f[kMaxLeds];
  for (uint32_t t = 0; t < 5000; t += 250)
  {
    e.render(Effect::MulticolorBreath, t, 0, f, 6);
    TEST_ASSERT_TRUE(f[0].r >= 1);
  }
}

void test_fireplace_is_warm_and_alive(void)
{
  Engine e(1);
  Rgb f[kMaxLeds];
  bool seen[256] = {false};
  bool differ = false;
  for (uint32_t i = 0; i < 2000; i++)
  {
    e.render(Effect::Fireplace, i * 20, 20, f, 6);
    for (uint8_t k = 0; k < 6; k++)
    {
      TEST_ASSERT_TRUE(f[k].r >= f[k].g);
      TEST_ASSERT_EQUAL_UINT8(0, f[k].b);
      TEST_ASSERT_TRUE(f[k].r >= 40);
    }
    seen[f[0].r] = true;
    differ = differ || !same(f[0], f[1]);
  }
  TEST_ASSERT_TRUE(countTrue(seen, 256) >= 10);
  TEST_ASSERT_TRUE(differ);
}

void test_fireworks_bursts(void)
{
  Engine e(1);
  e.reset(7);
  Rgb f[kMaxLeds];
  bool saw_black = false, saw_white = false, saw_color = false;
  bool hues[216] = {false}; // burst colours, keyed by quantised channel ratios
  for (uint32_t i = 0; i < 3000; i++)
  {
    e.render(Effect::Fireworks, i * 20, 20, f, 6);
    TEST_ASSERT_TRUE(e.activeBursts() <= kMaxBursts);
    bool black = true;
    for (uint8_t k = 0; k < 6; k++)
    {
      const Rgb &p = f[k];
      if (p.r || p.g || p.b)
      {
        black = false;
      }
      if (p.r == p.g && p.g == p.b)
      {
        saw_white = saw_white || p.r >= 200;
        continue;
      }
      saw_color = true;
      uint8_t m = p.r > p.g ? (p.r > p.b ? p.r : p.b) : (p.g > p.b ? p.g : p.b);
      if (m >= 100)
      {
        hues[(p.r * 255 / m / 50) * 36 + (p.g * 255 / m / 50) * 6 + (p.b * 255 / m / 50)] = true;
      }
    }
    saw_black = saw_black || black;
  }
  TEST_ASSERT_TRUE(saw_black);
  TEST_ASSERT_TRUE(saw_white);
  TEST_ASSERT_TRUE(saw_color);
  TEST_ASSERT_TRUE(countTrue(hues, 216) >= 2);
}

void test_fireworks_three_simultaneous_distinct_colors(void)
{
  Engine e(1);
  e.reset(7);
  Rgb f[kMaxLeds];
  bool saw_three = false;
  for (uint32_t i = 0; i < 3000; i++)
  {
    e.render(Effect::Fireworks, i * 20, 20, f, 6);
    saw_three = saw_three || e.activeBursts() == 3;
    for (uint8_t a = 0; a < kMaxBursts; a++)
    {
      for (uint8_t b = a + 1; b < kMaxBursts; b++)
      {
        if (e.burstColor(a) != 255 && e.burstColor(b) != 255)
        {
          TEST_ASSERT_TRUE(e.burstColor(a) != e.burstColor(b));
        }
      }
    }
  }
  TEST_ASSERT_TRUE(saw_three);
}

void test_christmas_figures(void)
{
  Engine e(1);
  Rgb f0[kMaxLeds], f1[kMaxLeds];

  // Figure 0: swap in place.
  e.render(Effect::Christmas, 0, 20, f0, 6);
  e.render(Effect::Christmas, kChristmasStepMs, 20, f1, 6);
  bool shift_up = true, shift_down = true, changed = false;
  for (uint8_t i = 0; i < 6; i++)
  {
    TEST_ASSERT_TRUE(inGarland(f0[i]));
    TEST_ASSERT_TRUE(inGarland(f1[i]));
    changed = changed || !same(f0[i], f1[i]);
    if (i < 5)
    {
      TEST_ASSERT_FALSE(same(f0[i], f0[i + 1]));
      TEST_ASSERT_FALSE(same(f1[i], f1[i + 1]));
      shift_down = shift_down && same(f1[i], f0[i + 1]);
    }
    if (i > 0)
    {
      shift_up = shift_up && same(f1[i], f0[i - 1]);
    }
  }
  TEST_ASSERT_TRUE(changed);
  TEST_ASSERT_FALSE(shift_up);
  TEST_ASSERT_FALSE(shift_down);

  // Figure 1: scrolling, one LED per step.
  e.render(Effect::Christmas, kChristmasFigureMs, 20, f0, 6);
  e.render(Effect::Christmas, kChristmasFigureMs + kChristmasStepMs, 20, f1, 6);
  for (uint8_t i = 0; i < 6; i++)
  {
    TEST_ASSERT_TRUE(inGarland(f0[i]));
    TEST_ASSERT_TRUE(inGarland(f1[i]));
    if (i < 5)
    {
      TEST_ASSERT_FALSE(same(f0[i], f0[i + 1]));
      TEST_ASSERT_FALSE(same(f1[i], f1[i + 1]));
    }
    if (i > 0)
    {
      TEST_ASSERT_TRUE(same(f1[i], f0[i - 1]));
    }
  }

  // Figure 2: static garland, at most one white twinkle.
  bool twinkled = false;
  for (uint32_t t = 2 * kChristmasFigureMs; t < 3 * kChristmasFigureMs; t += 50)
  {
    e.render(Effect::Christmas, t, 20, f0, 6);
    uint8_t diff = 0;
    for (uint8_t i = 0; i < 6; i++)
    {
      if (!same(f0[i], kGarland[i % 4]))
      {
        diff++;
        TEST_ASSERT_TRUE(same(f0[i], kWhite));
      }
    }
    TEST_ASSERT_TRUE(diff <= 1);
    twinkled = twinkled || diff == 1;
  }
  TEST_ASSERT_TRUE(twinkled);

  // The figures loop.
  Rgb a[kMaxLeds], b[kMaxLeds];
  e.render(Effect::Christmas, 0, 20, a, 6);
  e.render(Effect::Christmas, 3 * kChristmasFigureMs, 20, b, 6);
  TEST_ASSERT_EQUAL_MEMORY(a, b, 6 * sizeof(Rgb));
}

void test_same_seed_same_sequence(void)
{
  Engine a(3), b(3);
  Rgb fa[kMaxLeds], fb[kMaxLeds];
  for (uint32_t i = 0; i < 500; i++)
  {
    a.render(Effect::Fireworks, i * 20, 20, fa, 6);
    b.render(Effect::Fireworks, i * 20, 20, fb, 6);
    TEST_ASSERT_EQUAL_MEMORY(fa, fb, 6 * sizeof(Rgb));
  }
}

void test_led_counts(void)
{
  const uint8_t counts[4] = {0, 1, 34, 200};
  for (uint8_t ei = 0; ei < 6; ei++)
  {
    for (uint8_t ci = 0; ci < 4; ci++)
    {
      Engine e(1);
      Rgb buf[kMaxLeds + 2];
      for (uint8_t i = 0; i < kMaxLeds + 2; i++)
      {
        buf[i] = Rgb{1, 2, 3};
      }
      uint8_t n = counts[ci] > kMaxLeds ? kMaxLeds : counts[ci];
      e.render(kAllEffects[ei], 12345, 20, buf + 1, counts[ci]);
      TEST_ASSERT_TRUE(same(buf[0], Rgb{1, 2, 3}));
      for (uint8_t i = n + 1; i < kMaxLeds + 2; i++)
      {
        TEST_ASSERT_TRUE(same(buf[i], Rgb{1, 2, 3}));
      }
    }
  }
}

void test_millis_wraparound(void)
{
  for (uint8_t ei = 0; ei < 6; ei++)
  {
    Engine e(1);
    Rgb f[kMaxLeds];
    for (uint32_t i = 0; i < 32; i++)
    {
      uint32_t t = 0xFFFFFF00u + i * 16; // wraps through 0
      e.render(kAllEffects[ei], t, 20, f, 6);
      if (kAllEffects[ei] == Effect::Fireplace)
      {
        for (uint8_t k = 0; k < 6; k++)
        {
          TEST_ASSERT_TRUE(f[k].r >= 40);
        }
      }
    }
  }
}

int main(int, char **)
{
  UNITY_BEGIN();
  RUN_TEST(test_multicolor_fixed);
  RUN_TEST(test_chase_cross_fades_6);
  RUN_TEST(test_chase_cross_fades_34);
  RUN_TEST(test_chase_never_pauses);
  RUN_TEST(test_breath_keeps_hue_and_varies_level);
  RUN_TEST(test_breath_zero_bpm_is_safe);
  RUN_TEST(test_fireplace_is_warm_and_alive);
  RUN_TEST(test_fireworks_bursts);
  RUN_TEST(test_fireworks_three_simultaneous_distinct_colors);
  RUN_TEST(test_christmas_figures);
  RUN_TEST(test_same_seed_same_sequence);
  RUN_TEST(test_led_counts);
  RUN_TEST(test_millis_wraparound);
  return UNITY_END();
}
