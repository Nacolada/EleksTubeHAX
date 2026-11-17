#include "BacklightEffects.h"

#include <math.h>
#include <string.h>

namespace backlight_effects
{
  namespace
  {
    const double kPi = 3.14159265358979323846;

    const Rgb kMulticolor[6] = {
      {255, 0, 0},   // red
      {255, 128, 0}, // orange
      {255, 255, 0}, // yellow
      {0, 255, 0},   // green
      {0, 0, 255},   // blue
      {148, 0, 211}  // violet
    };

    const Rgb kGarland[4] = {
      {255, 0, 0},   // red
      {0, 255, 0},   // green
      {255, 170, 0}, // gold
      {255, 255, 255} // white
    };

    // Vivid burst colours: always at least one channel at 0, so never mistaken for the white flash.
    const Rgb kBurstColors[12] = {
      {255, 0, 0}, {255, 128, 0}, {255, 255, 0}, {128, 255, 0},
      {0, 255, 0}, {0, 255, 128}, {0, 255, 255}, {0, 128, 255},
      {0, 0, 255}, {128, 0, 255}, {255, 0, 255}, {255, 0, 128}};
    constexpr uint8_t kBurstColorCount = 12;

    inline uint8_t scale(uint8_t channel, uint8_t k)
    {
      return (uint8_t)((uint32_t)channel * k / 255);
    }

    inline uint8_t addSat(uint8_t a, uint8_t b)
    {
      uint16_t s = (uint16_t)a + b;
      return s > 255 ? 255 : (uint8_t)s;
    }

    void addTo(Rgb &px, const Rgb &c, uint8_t k)
    {
      px.r = addSat(px.r, scale(c.r, k));
      px.g = addSat(px.g, scale(c.g, k));
      px.b = addSat(px.b, scale(c.b, k));
    }

    // Stateless hash, used where the pattern must depend on the clock only.
    uint32_t mix(uint32_t x)
    {
      x ^= x << 13;
      x ^= x >> 17;
      x ^= x << 5;
      x *= 2654435761u;
      x ^= x >> 15;
      return x;
    }

    // True when now has reached deadline, wraparound-safe.
    inline bool reached(uint32_t now_ms, uint32_t deadline_ms)
    {
      return (int32_t)(now_ms - deadline_ms) >= 0;
    }
  }

  Rgb multicolorPalette(uint8_t index)
  {
    return kMulticolor[index % 6];
  }

  Engine::Engine(uint32_t seed)
  {
    reset(seed);
  }

  void Engine::reset(uint32_t seed)
  {
    seed_ = seed;
    // Raw small seeds give weak first xorshift outputs: spread them, never leave 0.
    rng_ = seed * 2654435761u;
    if (rng_ == 0)
    {
      rng_ = 1;
    }
    for (uint8_t i = 0; i < 4; i++)
    {
      next();
    }
    fire_primed_ = false;
    fire_last_ms_ = 0;
    memset(level_, 128, sizeof(level_));
    memset(target_, 128, sizeof(target_));
    memset(hold_until_ms_, 0, sizeof(hold_until_ms_));
    fw_primed_ = false;
    next_burst_ms_ = 0;
    memset(bursts_, 0, sizeof(bursts_));
  }

  uint32_t Engine::next()
  {
    rng_ ^= rng_ << 13;
    rng_ ^= rng_ >> 17;
    rng_ ^= rng_ << 5;
    return rng_;
  }

  uint8_t Engine::activeBursts() const
  {
    uint8_t n = 0;
    for (uint8_t i = 0; i < kMaxBursts; i++)
    {
      if (bursts_[i].active)
      {
        n++;
      }
    }
    return n;
  }

  uint8_t Engine::burstColor(uint8_t slot) const
  {
    return (slot < kMaxBursts && bursts_[slot].active) ? bursts_[slot].color : 255;
  }

  void Engine::render(Effect e, uint32_t now_ms, uint8_t breath_per_min, Rgb *out, uint8_t count)
  {
    if (count == 0)
    {
      return;
    }
    if (count > kMaxLeds)
    {
      count = kMaxLeds;
    }
    switch (e)
    {
    case Effect::Fireplace:
      renderFireplace(now_ms, out, count);
      break;
    case Effect::Fireworks:
      renderFireworks(now_ms, out, count);
      break;
    case Effect::Christmas:
      renderChristmas(now_ms, out, count);
      break;
    case Effect::Multicolor:
      for (uint8_t i = 0; i < count; i++)
      {
        out[i] = multicolorPalette(i);
      }
      break;
    case Effect::MulticolorChase:
    {
      // The palette advances one LED per step, towards higher indexes.
      uint8_t shift = (uint8_t)((now_ms / kChaseStepMs) % 6);
      for (uint8_t i = 0; i < count; i++)
      {
        out[i] = multicolorPalette((uint8_t)(i + 6 - shift));
      }
      break;
    }
    case Effect::MulticolorBreath:
    {
      // Same formula as the Breath overlay level: 1..254 over one period.
      uint32_t len = 60000UL / (breath_per_min ? breath_per_min : 1);
      double val = (exp(sin(2 * kPi * (now_ms % len) / len)) - 0.36787944) * 108.0;
      if (val < 1)
      {
        val = 1;
      }
      if (val > 255)
      {
        val = 255;
      }
      uint8_t level = (uint8_t)val;
      for (uint8_t i = 0; i < count; i++)
      {
        Rgb c = multicolorPalette(i);
        // A lit channel never drops to 0, so the hue stays recognisable at the bottom of the breath.
        out[i].r = c.r ? (scale(c.r, level) ? scale(c.r, level) : 1) : 0;
        out[i].g = c.g ? (scale(c.g, level) ? scale(c.g, level) : 1) : 0;
        out[i].b = c.b ? (scale(c.b, level) ? scale(c.b, level) : 1) : 0;
      }
      break;
    }
    }
  }

  void Engine::renderFireplace(uint32_t now_ms, Rgb *out, uint8_t count)
  {
    if (!fire_primed_)
    {
      for (uint8_t i = 0; i < kMaxLeds; i++)
      {
        hold_until_ms_[i] = now_ms;
      }
      fire_last_ms_ = now_ms;
      fire_primed_ = true;
    }
    uint32_t dt = now_ms - fire_last_ms_;
    fire_last_ms_ = now_ms;
    if (dt > kFireEaseMs)
    {
      dt = kFireEaseMs;
    }

    // Every LED has its own target level and hold time: flames flicker independently.
    for (uint8_t i = 0; i < kMaxLeds; i++)
    {
      if (reached(now_ms, hold_until_ms_[i]))
      {
        uint32_t r = next();
        uint32_t t = r & 0xFF;
        // One pick in four is a dip towards dark red, the others sit in orange/yellow.
        target_[i] = ((r >> 8) & 3) == 0 ? (uint8_t)(t / 3) : (uint8_t)(90 + t * 165 / 255);
        hold_until_ms_[i] = now_ms + kFireMinHoldMs + (r >> 12) % kFireHoldRangeMs;
      }
      int32_t diff = (int32_t)target_[i] - level_[i];
      level_[i] = (uint8_t)(level_[i] + diff * (int32_t)dt / (int32_t)kFireEaseMs);
    }

    for (uint8_t i = 0; i < count; i++)
    {
      uint32_t v = level_[i];
      // Dark red (v = 0) through orange to yellow (v = 255); blue stays 0 and g <= r always.
      out[i].r = (uint8_t)(40 + v * 215 / 255);
      out[i].g = (uint8_t)(v * v / 255 * 200 / 255);
      out[i].b = 0;
    }
  }

  void Engine::renderFireworks(uint32_t now_ms, Rgb *out, uint8_t count)
  {
    if (!fw_primed_)
    {
      next_burst_ms_ = now_ms + kBurstMinGapMs + next() % kBurstGapRangeMs;
      fw_primed_ = true;
    }

    for (uint8_t i = 0; i < kMaxBursts; i++)
    {
      if (bursts_[i].active && (uint32_t)(now_ms - bursts_[i].start_ms) >= kBurstFadeMs)
      {
        bursts_[i].active = false;
      }
    }

    if (reached(now_ms, next_burst_ms_))
    {
      for (uint8_t i = 0; i < kMaxBursts; i++)
      {
        if (bursts_[i].active)
        {
          continue;
        }
        // Different colour from the bursts still on: at most 3 of 12, so one is always free.
        uint8_t color = (uint8_t)(next() % kBurstColorCount);
        for (uint8_t tries = 0; tries < kBurstColorCount; tries++)
        {
          bool used = false;
          for (uint8_t j = 0; j < kMaxBursts; j++)
          {
            used = used || (bursts_[j].active && bursts_[j].color == color);
          }
          if (!used)
          {
            break;
          }
          color = (uint8_t)((color + 1) % kBurstColorCount);
        }
        bursts_[i].active = true;
        bursts_[i].led = (uint8_t)(next() % count);
        bursts_[i].color = color;
        bursts_[i].start_ms = now_ms;
        next_burst_ms_ = now_ms + kBurstMinGapMs + next() % kBurstGapRangeMs;
        break;
      }
    }

    memset(out, 0, count * sizeof(Rgb));
    for (uint8_t i = 0; i < kMaxBursts; i++)
    {
      const Burst &b = bursts_[i];
      if (!b.active || b.led >= count)
      {
        continue;
      }
      uint32_t age = now_ms - b.start_ms;
      if (age < kBurstFlashMs)
      {
        addTo(out[b.led], kGarland[3], 255); // white flash
        continue;
      }
      // Then the colour spills onto the neighbours and everything fades out.
      uint32_t span = kBurstFadeMs - kBurstFlashMs;
      uint8_t k = (uint8_t)(255 * (span - (age - kBurstFlashMs)) / span);
      const Rgb &c = kBurstColors[b.color];
      addTo(out[b.led], c, k);
      if (b.led > 0)
      {
        addTo(out[b.led - 1], c, k / 2);
      }
      if (b.led + 1 < count)
      {
        addTo(out[b.led + 1], c, k / 2);
      }
    }
  }

  void Engine::renderChristmas(uint32_t now_ms, Rgb *out, uint8_t count)
  {
    uint32_t figure = (now_ms / kChristmasFigureMs) % 3;
    uint32_t step = now_ms / kChristmasStepMs;

    if (figure == 0)
    {
      // Swap in place: neighbours exchange their colours on odd steps.
      for (uint8_t i = 0; i < count; i++)
      {
        uint8_t src = i;
        if ((step & 1) && (i ^ 1) < count)
        {
          src = i ^ 1;
        }
        out[i] = kGarland[src % 4];
      }
    }
    else if (figure == 1)
    {
      // Scrolling: the garland advances one LED per step, towards higher indexes.
      uint8_t shift = (uint8_t)(step % 4);
      for (uint8_t i = 0; i < count; i++)
      {
        out[i] = kGarland[(i + 4 - shift) % 4];
      }
    }
    else
    {
      // Twinkle: static garland, one random LED briefly white.
      for (uint8_t i = 0; i < count; i++)
      {
        out[i] = kGarland[i % 4];
      }
      uint32_t slot = now_ms / kTwinkleSlotMs;
      if (now_ms % kTwinkleSlotMs < kTwinkleLitMs)
      {
        out[mix(slot ^ seed_) % count] = kGarland[3];
      }
    }
  }
}
