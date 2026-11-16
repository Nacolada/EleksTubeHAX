#ifndef TUBE_OVERLAY_GLUE_H_
#define TUBE_OVERLAY_GLUE_H_

#ifdef TUBE_OVERLAY

#include <stdint.h>
#include "GLOBAL_DEFINES.h"
#include "TubeOverlay.h"

extern tube_overlay::State tubeOverlay; // defined in src/main.cpp

// Screen/LED index of the tube at visible position pos (0 = leftmost).
static inline uint8_t tubeOverlayDigit(uint8_t pos) { return HOURS_TENS == 0 ? pos : NUM_DIGITS - 1 - pos; }

#endif // TUBE_OVERLAY

#endif // TUBE_OVERLAY_GLUE_H_
