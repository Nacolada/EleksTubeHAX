#include "TubeOverlay.h"

#include <ArduinoJson.h>
#include <math.h>
#include <string.h>

namespace tube_overlay
{
  namespace
  {
    const double kPi = 3.14159265358979323846;

    bool parseTube(JsonObjectConst obj, Tube &out)
    {
      memset(&out, 0, sizeof(out));
      out.active = true;
      out.effect = Effect::Constant;

      JsonVariantConst color = obj["color"];
      if (!color.isNull())
      {
        if (!color.is<JsonArrayConst>())
        {
          return false;
        }
        JsonArrayConst arr = color.as<JsonArrayConst>();
        if (arr.size() != 3)
        {
          return false;
        }
        int32_t c[3];
        for (uint8_t i = 0; i < 3; i++)
        {
          JsonVariantConst v = arr[i];
          if (!v.is<int32_t>())
          {
            return false;
          }
          c[i] = v.as<int32_t>();
          if (c[i] < 0 || c[i] > 255)
          {
            return false;
          }
        }
        out.has_color = true;
        out.r = (uint8_t)c[0];
        out.g = (uint8_t)c[1];
        out.b = (uint8_t)c[2];
      }

      JsonVariantConst effect = obj["effect"];
      if (!effect.isNull())
      {
        if (!effect.is<const char *>())
        {
          return false;
        }
        const char *name = effect.as<const char *>();
        if (strcmp(name, "constant") == 0)
        {
          out.effect = Effect::Constant;
        }
        else if (strcmp(name, "pulse") == 0)
        {
          out.effect = Effect::Pulse;
        }
        else if (strcmp(name, "breath") == 0)
        {
          out.effect = Effect::Breath;
        }
        else
        {
          return false;
        }
      }

      JsonVariantConst blink = obj["blink"];
      if (!blink.isNull())
      {
        if (!blink.is<bool>())
        {
          return false;
        }
        out.blink = blink.as<bool>();
      }

      JsonVariantConst text = obj["text"];
      if (!text.isNull())
      {
        if (!text.is<const char *>())
        {
          return false;
        }
        const char *src = text.as<const char *>();
        size_t n = strlen(src);
        if (n > kTextMax)
        {
          n = kTextMax;
        }
        for (size_t i = 0; i < n; i++)
        {
          uint8_t ch = (uint8_t)src[i];
          out.text[i] = (ch >= 0x20 && ch <= 0x7E) ? (char)ch : '?';
        }
        out.text[n] = '\0';
      }
      return true;
    }
  }

  State::State() : locked_(false), received_ms_(0), ttl_ms_(0), revision_(0)
  {
    clearTubes();
  }

  void State::clearTubes()
  {
    memset(tubes_, 0, sizeof(tubes_));
  }

  Result State::handleMessage(const char *json, size_t len, uint32_t now_ms)
  {
    JsonDocument doc;
    if (deserializeJson(doc, json, len))
    {
      return Result::BadJson;
    }
    if (!doc.is<JsonObject>())
    {
      return Result::BadShape;
    }
    JsonObjectConst root = doc.as<JsonObjectConst>();

    // Lock switch: {"state": "ON"|"OFF"}
    JsonVariantConst state = root["state"];
    if (!state.isNull())
    {
      if (!state.is<const char *>())
      {
        return Result::BadValue;
      }
      const char *s = state.as<const char *>();
      if (strcmp(s, "OFF") == 0)
      {
        locked_ = true;
        clearTubes();
      }
      else if (strcmp(s, "ON") == 0)
      {
        locked_ = false;
      }
      else
      {
        return Result::BadValue;
      }
      revision_++;
      return Result::LockChanged;
    }

    // Everything is validated into a scratch copy and installed only at the end.
    JsonVariantConst tubes = root["tubes"];
    if (!tubes.is<JsonArrayConst>())
    {
      return Result::BadShape;
    }
    JsonArrayConst arr = tubes.as<JsonArrayConst>();
    if (arr.size() != kTubes)
    {
      return Result::BadShape;
    }

    Tube next[kTubes];
    memset(next, 0, sizeof(next));
    bool value_error = false;
    for (uint8_t i = 0; i < kTubes; i++)
    {
      JsonVariantConst entry = arr[i];
      if (entry.isNull())
      {
        continue;
      }
      if (!entry.is<JsonObjectConst>())
      {
        return Result::BadShape;
      }
      if (!parseTube(entry.as<JsonObjectConst>(), next[i]))
      {
        value_error = true;
      }
    }

    uint32_t ttl_s = kDefaultTtlS;
    JsonVariantConst ttl = root["ttl"];
    if (!ttl.isNull())
    {
      if (!ttl.is<int64_t>() || ttl.as<int64_t>() <= 0)
      {
        value_error = true;
      }
      else
      {
        int64_t t = ttl.as<int64_t>();
        ttl_s = (t > (int64_t)kMaxTtlS) ? kMaxTtlS : (uint32_t)t;
      }
    }
    if (value_error)
    {
      return Result::BadValue;
    }

    if (locked_)
    {
      return Result::IgnoredLocked;
    }

    memcpy(tubes_, next, sizeof(tubes_));
    received_ms_ = now_ms;
    ttl_ms_ = ttl_s * 1000UL;
    revision_++;
    return Result::Applied;
  }

  bool State::tick(uint32_t now_ms)
  {
    if (activeCount() == 0)
    {
      return false;
    }
    if ((uint32_t)(now_ms - received_ms_) >= ttl_ms_)
    {
      clearTubes();
      revision_++;
      return true;
    }
    return false;
  }

  bool State::locked() const
  {
    return locked_;
  }

  uint8_t State::activeCount() const
  {
    uint8_t n = 0;
    for (uint8_t i = 0; i < kTubes; i++)
    {
      if (tubes_[i].active)
      {
        n++;
      }
    }
    return n;
  }

  const Tube &State::tube(uint8_t pos) const
  {
    return tubes_[pos < kTubes ? pos : kTubes - 1];
  }

  bool State::textVisible(uint8_t pos, uint32_t now_ms) const
  {
    if (pos >= kTubes)
    {
      return false;
    }
    const Tube &t = tubes_[pos];
    if (!t.active || t.text[0] == '\0')
    {
      return false;
    }
    if (!t.blink)
    {
      return true;
    }
    return (((uint32_t)(now_ms - received_ms_)) / kBlinkHalfPeriodMs) % 2 == 0;
  }

  uint32_t State::revision() const
  {
    return revision_;
  }

  uint8_t effectLevel(Effect e, uint32_t now_ms, uint8_t pulse_bpm, uint8_t breath_per_min)
  {
    // Same formulas as Backlights::pulsePattern() and Backlights::breathPattern().
    if (e == Effect::Pulse)
    {
      double len = (60.0 * 1000) / (pulse_bpm ? pulse_bpm : 1);
      double val = 1 + fabs(sin(2 * kPi * now_ms / len)) * 254;
      if (val < 1)
      {
        val = 1;
      }
      if (val > 255)
      {
        val = 255;
      }
      return (uint8_t)val;
    }
    if (e == Effect::Breath)
    {
      double len = (60.0 * 1000) / (breath_per_min ? breath_per_min : 1);
      double val = (exp(sin(2 * kPi * now_ms / len)) - 0.36787944) * 108.0;
      if (val < 1)
      {
        val = 1;
      }
      if (val > 255)
      {
        val = 255;
      }
      return (uint8_t)val;
    }
    return 255;
  }

  uint8_t scaleChannel(uint8_t channel, Effect e, uint8_t level, uint8_t intensity)
  {
    if (intensity > 7)
    {
      intensity = 7;
    }
    if (e == Effect::Constant)
    {
      return (uint8_t)((uint32_t)channel * (0xFF >> (7 - intensity)) / 255);
    }
    return (uint8_t)((uint32_t)channel * level * intensity / 7 / 255);
  }
}
