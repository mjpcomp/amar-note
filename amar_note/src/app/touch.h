#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Call once after i2c_master_Init().
void touchInit(void);

// Poll the FT6336.  Returns true and fills *sx/*sy (screen coords 0-199)
// if a finger is currently down.  Returns false with *sx/*sy unchanged
// when no touch is detected.
//
// Named touchPoll (not touchRead) to avoid collision with the Arduino
// touchRead(pin) GPIO capacitive-touch API.
bool touchPoll(uint16_t *sx, uint16_t *sy);

// Convenience AABB hit-test: returns true if (sx,sy) falls inside the
// rectangle defined by (rx, ry, rw, rh).
bool touchHitTest(uint16_t sx, uint16_t sy,
                  int rx, int ry, int rw, int rh);

#ifdef __cplusplus
}
#endif
