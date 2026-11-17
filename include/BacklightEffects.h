#ifndef BACKLIGHT_EFFECTS_H_
#define BACKLIGHT_EFFECTS_H_

// Ambient LED effects: fireplace, fireworks, Christmas garland and three multicolor
// variants, one LED per tube. The engine only computes colours at full scale; the caller
// applies the global brightness and pushes the result to the LED strip.
// Hardware-free on purpose (no Arduino.h): it is compiled for the ESP32 and for the host.
// Clocks are passed in as milliseconds (wraparound-safe), so the module can be tested off-target.

#include <stddef.h>
#include <stdint.h>

namespace backlight_effects
{
  constexpr uint8_t kMaxLeds = 34;
  constexpr uint8_t kMaxBursts = 3;
  constexpr uint32_t kChaseStepMs = 1500; // time for one LED to fade from a palette colour to the next
  constexpr uint32_t kChristmasFigureMs = 20000;
  constexpr uint32_t kChristmasStepMs = 500;
  constexpr uint32_t kBurstFadeMs = 1000;

  constexpr uint32_t kBurstFlashMs = 150;       // white flash at the start of a burst
  constexpr uint32_t kBurstMinGapMs = 250;      // gap between two bursts: min + random(range)
  constexpr uint32_t kBurstGapRangeMs = 1500;
  constexpr uint32_t kFireMinHoldMs = 40;       // how long a flame LED keeps its target level
  constexpr uint32_t kFireHoldRangeMs = 160;
  constexpr uint32_t kFireEaseMs = 100;         // time to reach a new target level
  constexpr uint32_t kTwinkleSlotMs = 300;      // Christmas figure 2: one random LED per slot
  constexpr uint32_t kTwinkleLitMs = 150;

  enum class Effect : uint8_t
  {
    Fireplace,
    Fireworks,
    Christmas,
    Multicolor,
    MulticolorChase,
    MulticolorBreath
  };

  struct Rgb
  {
    uint8_t r, g, b;
  };

  class Engine
  {
  public:
    explicit Engine(uint32_t seed = 1);
    void reset(uint32_t seed); // clears bursts and flicker state

    // Fills out[0..count-1] at full scale (global brightness is applied by the caller).
    // count > kMaxLeds is clamped; count == 0 is a no-op.
    void render(Effect e, uint32_t now_ms, uint8_t breath_per_min, Rgb *out, uint8_t count);
    uint8_t activeBursts() const;
    uint8_t burstColor(uint8_t slot) const; // colour table index of an active burst slot, 255 if none

  private:
    struct Burst
    {
      bool active;
      uint8_t led;
      uint8_t color; // index in the burst colour table
      uint32_t start_ms;
    };

    uint32_t next();                // xorshift32
    void renderFireplace(uint32_t now_ms, Rgb *out, uint8_t count);
    void renderFireworks(uint32_t now_ms, Rgb *out, uint8_t count);
    void renderChristmas(uint32_t now_ms, Rgb *out, uint8_t count);

    uint32_t rng_;
    uint32_t seed_;

    bool fire_primed_;
    uint32_t fire_last_ms_;
    uint8_t level_[kMaxLeds];
    uint8_t target_[kMaxLeds];
    uint32_t hold_until_ms_[kMaxLeds];

    bool fw_primed_;
    uint32_t next_burst_ms_;
    Burst bursts_[kMaxBursts];
  };

  Rgb multicolorPalette(uint8_t index); // index % 6 : red, orange, yellow, green, blue, violet
}

#endif // BACKLIGHT_EFFECTS_H_
