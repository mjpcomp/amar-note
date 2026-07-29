#pragma once
#include "../../types.h"

// Raw single-button state machine — use pollButton() in loop().
ButtonEvent readButtonEvent(int pin);

// Convenience wrapper used by loop().
//   pin      — BTN_REC or BTN_PWR
//   isPwr    — true for BTN_PWR: EV_LONG also triggers power-latch cut
ButtonEvent pollButton(int pin, bool isPwr);
