#ifndef TUBE_OVERLAY_H_
#define TUBE_OVERLAY_H_

// Per-tube overlay: each of the 6 tubes can get an LED colour/effect and a short text,
// set by one JSON message, with a time-to-live.
// Hardware-free on purpose (no Arduino.h): it is compiled for the ESP32 and for the host.
// Clocks are passed in as milliseconds, so the module can be tested off-target.

#include <stddef.h>
#include <stdint.h>

namespace tube_overlay
{
  constexpr uint8_t kTubes = 6;
  constexpr uint8_t kTextMax = 20;
  constexpr uint32_t kDefaultTtlS = 300;
  constexpr uint32_t kMaxTtlS = 3600;
  constexpr uint32_t kBlinkHalfPeriodMs = 500;

  enum class Effect : uint8_t
  {
    Constant,
    Pulse,
    Breath
  };

  enum class Result : uint8_t
  {
    Applied,
    LockChanged,
    IgnoredLocked,
    BadJson,
    BadShape,
    BadValue
  };

  struct Tube
  {
    bool active;    // false: no overlay on this tube
    bool has_color; // false: LED keeps the normal pattern
    uint8_t r, g, b;
    Effect effect;
    bool blink;
    char text[kTextMax + 1]; // "" = no text
  };

  class State
  {
  public:
    State();

    Result handleMessage(const char *json, size_t len, uint32_t now_ms);
    bool tick(uint32_t now_ms);          // true when the TTL just expired and the overlay was cleared
    bool locked() const;                 // true = switch OFF
    uint8_t activeCount() const;
    const Tube &tube(uint8_t pos) const; // pos 0 = leftmost tube
    bool textVisible(uint8_t pos, uint32_t now_ms) const;
    uint32_t revision() const;           // +1 on every change of tubes or lock

  private:
    void clearTubes();

    Tube tubes_[kTubes];
    bool locked_;
    uint32_t received_ms_;
    uint32_t ttl_ms_;
    uint32_t revision_;
  };

  // Raw effect level, 1..255, without intensity. Constant gives 255.
  uint8_t effectLevel(Effect e, uint32_t now_ms, uint8_t pulse_bpm, uint8_t breath_per_min);
  // Applies the effect level and the intensity (0..7) to one colour channel.
  uint8_t scaleChannel(uint8_t channel, Effect e, uint8_t level, uint8_t intensity);
}

#endif // TUBE_OVERLAY_H_
