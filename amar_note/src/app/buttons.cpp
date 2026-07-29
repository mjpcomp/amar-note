#include "Arduino.h"
#include "../../config.h"
#include "../../globals.h"
#include "../../types.h"
#include "buttons.h"

extern void resetActivity();

bool isDown(int pin) { return digitalRead(pin) == LOW; }

// Non-blocking per-button state machine, sampled once per loop (no busy-wait).
// Emits:
//   EV_SINGLE  on release, if held < BTN_LONG_MS  (fires the instant you let go)
//   EV_LONG    the moment the hold crosses BTN_LONG_MS (fires once, no wait-for-release)
namespace {
  enum Phase { PH_IDLE, PH_DEBOUNCE, PH_DOWN, PH_LONGFIRED };
  struct BtnState { Phase phase; uint32_t tDown; };
  BtnState st[2] = {{PH_IDLE, 0}, {PH_IDLE, 0}};
  inline int idx(int pin) { return pin == BTN_REC ? 0 : 1; }
}

ButtonEvent readButtonEvent(int pin) {
  BtnState& b = st[idx(pin)];
  bool down = isDown(pin);
  uint32_t now = millis();

  switch (b.phase) {
    case PH_IDLE:
      if (down) { b.phase = PH_DEBOUNCE; b.tDown = now; }
      return EV_NONE;

    case PH_DEBOUNCE:
      if (!down) { b.phase = PH_IDLE; return EV_NONE; }      // bounce / too brief
      if (now - b.tDown >= BTN_DEBOUNCE_MS) b.phase = PH_DOWN;
      return EV_NONE;

    case PH_DOWN:
      if (now - b.tDown >= BTN_LONG_MS) {                    // crossed the long threshold
        b.phase = PH_LONGFIRED;
        resetActivity();
        return EV_LONG;
      }
      if (!down) {                                           // released as a tap
        b.phase = PH_IDLE;
        resetActivity();
        return EV_SINGLE;
      }
      return EV_NONE;

    case PH_LONGFIRED:                                       // long already fired; await release
      if (!down) b.phase = PH_IDLE;
      return EV_NONE;
  }
  return EV_NONE;
}

// pollButton() — called once per loop() tick.
//
// For BTN_PWR (isPwr=true) an EV_LONG means the user held the power button;
// we immediately cut the power-hold latch so the board shuts off, exactly as
// the old inline loop() did before the refactor.
ButtonEvent pollButton(int pin, bool isPwr) {
  ButtonEvent ev = readButtonEvent(pin);
  if (isPwr && ev == EV_LONG) {
    // Hard power-off: release the battery-hold latch and let the board die.
    pinMode(PWR_HOLD_PIN, OUTPUT);
    digitalWrite(PWR_HOLD_PIN, LOW);
  }
  return ev;
}
