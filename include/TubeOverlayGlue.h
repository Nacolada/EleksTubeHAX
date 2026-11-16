#ifndef TUBE_OVERLAY_GLUE_H_
#define TUBE_OVERLAY_GLUE_H_

#ifdef TUBE_OVERLAY

#include <stdint.h>
#include "GLOBAL_DEFINES.h"
#include "TubeOverlay.h"

extern tube_overlay::State tubeOverlay; // defined in src/main.cpp

// Screen index of the tube at visible position pos (0 = leftmost).
static inline uint8_t tubeOverlayDigit(uint8_t pos) { return HOURS_TENS == 0 ? pos : NUM_DIGITS - 1 - pos; }

// LED pixel index of the tube at visible position pos (0 = leftmost).
// On IPSTube boards LED 0 is the leftmost tube (verified on an IPSTubeA_III),
// whereas digit index 0 is the rightmost screen there. On every other hardware
// the pixel index is assumed to match the digit index, as upstream documents
// (unverified on those boards).
static inline uint8_t tubeOverlayLed(uint8_t pos)
{
#ifdef HARDWARE_IPSTUBE_CLOCK
  return pos;
#else
  return tubeOverlayDigit(pos);
#endif
}

#endif // TUBE_OVERLAY

#endif // TUBE_OVERLAY_GLUE_H_
