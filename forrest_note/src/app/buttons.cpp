#include "Arduino.h"
#include "../../config.h"
#include "../../globals.h"
#include "buttons.h"
#include "ui.h"
#include "record.h"

// ============================================================
// Buttons — Amar Note two-button state machine
//
// BTN_REC (GPIO0/BOOT):
//   tap          → start/stop recording from anywhere (instant, no hold)
//   long-press   → select / open item
//
// BTN_PWR (GPIO18):
//   tap          → scroll / navigate
//   long-press   → back / up
//   very-long    → sleep
// ============================================================

static uint32_t recDownMs  = 0;
static uint32_t pwrDownMs  = 0;
static bool     recWasDown = false;
static bool     pwrWasDown = false;
static bool     recLongFired = false;
static bool     pwrLongFired = false;

void buttonsInit() {
    pinMode(BTN_REC, INPUT_PULLUP);
    pinMode(BTN_PWR, INPUT_PULLUP);
}

void buttonsService() {
    const uint32_t now = millis();
    const bool recDown = (digitalRead(BTN_REC) == LOW);
    const bool pwrDown = (digitalRead(BTN_PWR) == LOW);

    // --- BTN_REC ---
    if (recDown && !recWasDown) {
        recDownMs    = now;
        recLongFired = false;
    }
    if (recDown && !recLongFired && (now - recDownMs) >= BTN_LONG_MS) {
        recLongFired = true;
        uiOnRecLong();
    }
    if (!recDown && recWasDown) {
        if (!recLongFired) {
            uiOnRecTap();   // instant tap — start or stop recording
        }
    }
    recWasDown = recDown;

    // --- BTN_PWR ---
    if (pwrDown && !pwrWasDown) {
        pwrDownMs    = now;
        pwrLongFired = false;
    }
    if (pwrDown && !pwrLongFired && (now - pwrDownMs) >= BTN_LONG_MS) {
        pwrLongFired = true;
        uiOnPwrLong();
    }
    if (pwrDown && !pwrLongFired && (now - pwrDownMs) >= BTN_VLONG_MS) {
        // very-long already consumed by long handler; sleep handled inside uiOnPwrLong
    }
    if (!pwrDown && pwrWasDown) {
        if (!pwrLongFired) {
            uiOnPwrTap();   // scroll / navigate
        }
    }
    pwrWasDown = pwrDown;
}
