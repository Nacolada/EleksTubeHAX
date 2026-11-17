#include "BacklightColor.h"

namespace backlight_color
{
  void encode(const FreeColor &c, uint8_t out[kBlobSize])
  {
    out[0] = kMagic;
    out[1] = c.active ? 1 : 0;
    out[2] = c.r;
    out[3] = c.g;
    out[4] = c.b;
  }

  bool decode(const uint8_t *in, size_t len, FreeColor &c)
  {
    if (in == nullptr || len != kBlobSize || in[0] != kMagic)
    {
      return false;
    }
    c.active = (in[1] & 1) != 0;
    c.r = in[2];
    c.g = in[3];
    c.b = in[4];
    return true;
  }
}
