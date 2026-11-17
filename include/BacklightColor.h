#ifndef BACKLIGHT_COLOR_H_
#define BACKLIGHT_COLOR_H_

// Free RGB colour of the back light, kept outside StoredConfig::Config: that struct is saved
// as one binary blob (backlights, clock, wifi), so adding a field would shift the stored
// Wi-Fi credentials. The colour has its own tiny blob in a separate Preferences namespace.
// Hardware-free on purpose (no Arduino.h): it is compiled for the ESP32 and for the host.

#include <stddef.h>
#include <stdint.h>

namespace backlight_color
{
  constexpr size_t kBlobSize = 5; // magic, flags, r, g, b
  constexpr uint8_t kMagic = 0xC1;

  struct FreeColor
  {
    bool active;
    uint8_t r, g, b;
  };

  void encode(const FreeColor &c, uint8_t out[kBlobSize]);
  // false (and c untouched) when len != kBlobSize or the magic is wrong
  bool decode(const uint8_t *in, size_t len, FreeColor &c);
}

#endif // BACKLIGHT_COLOR_H_
