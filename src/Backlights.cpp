#include "Backlights.h"
#include <Preferences.h>
#include <string.h>
#ifdef TUBE_OVERLAY
#include "TubeOverlayGlue.h"
#endif

void Backlights::begin(StoredConfig::Config::Backlights *config_)
{
  Adafruit_NeoPixel::begin(); // Initialize RMT and pin
  config = config_;

  if (config->is_valid != StoredConfig::valid)
  {
    // Config is invalid, probably a new device never had its config written.
    // Load some reasonable defaults.
    Serial.println("Loaded Backlights config is invalid, using default.  This is normal on first boot.");
    setPattern(rainbow);
    setColorPhase(0);
    setIntensity(max_intensity - 1);
    setPulseRate(60);
    setBreathRate(20);
    setRainbowDuration(DEFAULT_BL_RAINBOW_DURATION_SEC);
    config->is_valid = StoredConfig::valid;
  }
  off = false;
  loadFreeColor();
}

#define FREE_COLOR_NAMESPACE "blcolor"
#define FREE_COLOR_KEY "c"

void Backlights::loadFreeColor()
{
  free_color = {false, 0, 0, 0};
  Preferences p;
  if (p.begin(FREE_COLOR_NAMESPACE, false))
  {
    uint8_t blob[backlight_color::kBlobSize];
    size_t len = p.getBytesLength(FREE_COLOR_KEY);
    if (len == backlight_color::kBlobSize && p.getBytes(FREE_COLOR_KEY, blob, len) == len)
    {
      backlight_color::FreeColor loaded;
      if (backlight_color::decode(blob, len, loaded) && loaded.active)
      {
        free_color = loaded;
      }
    }
    p.end();
  }
  else
  {
    Serial.println("Backlights: could not open free colour storage, free colour inactive.");
  }
  backlight_color::encode(free_color, saved_color_blob);
  pattern_needs_init = true;
}

// Writes only when the value differs from what was last loaded or written (flash wear).
void Backlights::saveFreeColor()
{
  uint8_t blob[backlight_color::kBlobSize];
  backlight_color::encode(free_color, blob);
  if (memcmp(blob, saved_color_blob, sizeof(blob)) == 0)
  {
    return;
  }
  Preferences p;
  if (!p.begin(FREE_COLOR_NAMESPACE, false))
  {
    Serial.println("Backlights: could not open free colour storage, free colour not saved.");
    return;
  }
  if (p.putBytes(FREE_COLOR_KEY, blob, sizeof(blob)) == sizeof(blob))
  {
    memcpy(saved_color_blob, blob, sizeof(blob));
  }
  p.end();
}

// These feel like they should be generalizable into a helper function.
// https://stackoverflow.com/questions/11720656/modulo-operation-with-negative-numbers
void Backlights::setNextPattern(int8_t i)
{
  int8_t next_pattern = (config->pattern + i) % num_patterns;
  while (next_pattern < 0)
  {
    next_pattern += num_patterns;
  }
  setPattern(patterns(next_pattern));
}

void Backlights::adjustColorPhase(int16_t adj)
{
  int16_t new_phase = (int16_t(config->color_phase % max_phase) + adj) % max_phase;
  while (new_phase < 0)
  {
    new_phase += max_phase;
  }
  setColorPhase(new_phase);
}

void Backlights::adjustIntensity(int16_t adj)
{
  int16_t new_intensity = (int16_t(config->intensity) + adj) % max_intensity;
  while (new_intensity < 0)
  {
    new_intensity += max_intensity;
  }
  setIntensity(new_intensity);
}

void Backlights::setIntensity(uint8_t intensity)
{
  config->intensity = intensity;
  setBrightness(0xFF >> max_intensity - config->intensity - 1);
  pattern_needs_init = true;
}

uint8_t Backlights::getEffectiveIntensity()
{
  return dimming ? dimmed_intensity : config->intensity;
}

#ifdef TUBE_OVERLAY
void Backlights::show()
{
  applyTubeOverlay();
  Adafruit_NeoPixel::show();
}

// Writes straight into the pixel buffer, so the overlay is not scaled by the
// global brightness of the running pattern. Only the 6 tube pixels are touched.
void Backlights::applyTubeOverlay()
{
  if (config == NULL || tubeOverlay.activeCount() == 0)
  {
    return;
  }
  uint8_t *pixels = getPixels();
  uint32_t now = millis();
  uint8_t intensity = getEffectiveIntensity();
  for (uint8_t pos = 0; pos < tube_overlay::kTubes; pos++)
  {
    const tube_overlay::Tube &t = tubeOverlay.tube(pos);
    uint8_t digit = tubeOverlayLed(pos);
    if (!t.active || !t.has_color || digit >= numPixels())
    {
      continue;
    }
    uint8_t level = tube_overlay::effectLevel(t.effect, now, config->pulse_bpm, config->breath_per_min);
    uint8_t *p = &pixels[digit * (wOffset == rOffset ? 3 : 4)];
    p[rOffset] = tube_overlay::scaleChannel(t.r, t.effect, level, intensity);
    p[gOffset] = tube_overlay::scaleChannel(t.g, t.effect, level, intensity);
    p[bOffset] = tube_overlay::scaleChannel(t.b, t.effect, level, intensity);
  }
}
#endif

void Backlights::loop()
{
#ifdef TUBE_OVERLAY
  if (tubeOverlay.revision() != overlay_revision)
  {
    overlay_revision = tubeOverlay.revision();
    pattern_needs_init = true; // otherwise constant/dark keep the overlay colour after it ends
  }
#endif
  //   enum patterns { dark, test, constant, rainbow, pulse, breath, num_patterns };
  if (off || config->pattern == dark)
  {
    if (pattern_needs_init)
    {
      clear();
      show();
    }
#ifdef TUBE_OVERLAY
    else if (tubeOverlay.activeCount() > 0)
    {
      clear();
      show(); // the overlay stays visible with the lighting off
    }
#endif
  }
  else if (config->pattern == test)
  {
    testPattern();
  }
  else if (config->pattern == constant)
  {
    if (pattern_needs_init)
    {
      fill(getColor());
    }
    if (dimming)
    {
      setBrightness(0xFF >> max_intensity - dimmed_intensity - 1);
    }
    else
    {
      setBrightness(0xFF >> max_intensity - config->intensity - 1);
    }
    show();
  }
  else if (config->pattern == rainbow)
  {
    rainbowPattern();
  }
  else if (config->pattern == pulse)
  {
    pulsePattern();
  }
  else if (config->pattern == breath)
  {
    breathPattern();
  }

  pattern_needs_init = false;
}

void Backlights::pulsePattern()
{
  fill(getColor());

  float pulse_length_millis = (60.0f * 1000) / config->pulse_bpm;
  float val = 1 + abs(sin(2 * M_PI * millis() / pulse_length_millis)) * 254;
  if (dimming)
  {
    val = val * dimmed_intensity / 7;
  }
  else
  {
    val = val * config->intensity / 7;
  }
  setBrightness((uint8_t)val);

  show();
}

void Backlights::breathPattern()
{
  fill(getColor());

  // https://sean.voisen.org/blog/2011/10/breathing-led-with-arduino/
  // Avoid a 0 value as it shuts off the LEDs and we have to re-initialize.
  float pulse_length_millis = (60.0f * 1000) / config->breath_per_min;
  float val = (exp(sin(2 * M_PI * millis() / pulse_length_millis)) - 0.36787944f) * 108.0f;

  if (dimming)
  {
    val = val * dimmed_intensity / 7;
  }
  else
  {
    val = val * config->intensity / 7;
  }

  uint8_t brightness = (uint8_t)val;
  if (brightness < 1)
  {
    brightness = 1;
  }
  setBrightness(brightness);

  show();
}

void Backlights::testPattern()
{
  const uint8_t num_colors = 4; // or 3 if you don't want black
  uint8_t num_states = numPixels() * num_colors;
  uint8_t state = (millis() / test_ms_delay) % num_states;

  uint8_t digit = state / num_colors;
  uint32_t color = 0xFF0000 >> (state % num_colors) * 8;

  clear();
  setPixelColor(digit, color);

  if (dimming)
  {
    setBrightness(0xFF >> max_intensity - dimmed_intensity - 1);
  }
  else
  {
    setBrightness(0xFF >> max_intensity - config->intensity - 1);
  }

  show();
}

uint8_t Backlights::phaseToIntensity(uint16_t phase)
{
  uint16_t color = 0;
  if (phase <= 255)
  {
    // Ramping up
    color = phase;
  }
  else if (phase <= 511)
  {
    // Ramping down
    color = 511 - phase;
  }
  else
  {
    // Off
    color = 0;
  }
  if (color > 255)
  {
    // TODO: Trigger ERROR STATE, bug in code.
  }
  return uint8_t(color % 256);
}

uint32_t Backlights::phaseToColor(uint16_t phase)
{
  uint8_t red = phaseToIntensity(phase);
  uint8_t green = phaseToIntensity((phase + 256) % max_phase);
  uint8_t blue = phaseToIntensity((phase + 512) % max_phase);
  return (uint32_t(red) << 16 | uint32_t(green) << 8 | uint32_t(blue));
}

uint32_t Backlights::hueToPhase(float hue)
{
  hue = hue - 120.f;
  if (hue < 0)
  {
    hue = hue + 360.f;
  }
  uint32_t phase = uint32_t(round(768.f * (1.f - hue / 360.f)));
  phase = phase % max_phase;
  return (phase);
}

float Backlights::phaseToHue(uint32_t phase)
{
  float hue = 120.f + ((768.f - float(phase)) / 768.f) * 360.f;
  // h = 120 + (1 - p/768)*360
  if (hue >= 360.f)
  {
    hue = hue - 360.f;
  }
  return (round(hue));
}

void Backlights::rainbowPattern()
{
  // Divide by 3 to spread it out some, so the whole rainbow isn't displayed at once.
  // TODO Make this /3 a parameter
  const uint16_t phase_per_digit = (max_phase / numPixels()) / 3;

  // Rainbow roatation speed now configurable
  uint16_t duration = uint16_t(round(getRainbowDuration() * 1000));
  uint16_t phase = uint16_t(round(float(millis() % duration) / duration * max_phase));

  for (uint8_t digit = 0; digit < numPixels(); digit++)
  {
    // Shift the phase for this LED.
    uint16_t my_phase = (phase + digit * phase_per_digit) % max_phase;
    setPixelColor(digit, phaseToColor(my_phase));
  }
  if (dimming)
  {
    if (dimmed_intensity > 0)
    {
      setBrightness(0xFF >> max_intensity - dimmed_intensity - 1);
    }
    else
    { // turn off backlight if intensity is 0
      setBrightness(0);
    }
  }
  else
  {
    setBrightness(0xFF >> max_intensity - config->intensity - 1);
  }
  show();
}

const String Backlights::patterns_str[Backlights::num_patterns] =
    {"Dark", "Test", "Constant", "Rainbow", "Pulse", "Breath"};
