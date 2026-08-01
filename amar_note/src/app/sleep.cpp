#include "Arduino.h"
#include "../../config.h"
#include "../../globals.h"
#include "../../types.h"
#include "sleep.h"
#include "ui.h"
#include "battery.h"
#include "network.h"
#include "../../sounds.h"
#include "WiFi.h"

extern "C" {
#include "../../src/audio/audio_bsp.h"
}

void resetActivity() {
  lastActivityMs = millis();
}

void enterUltraSleep() {
  // Re-initialise the panel in full-waveform mode before drawing the splash.
  // If we were in partial-refresh mode (any screen other than idle), the
  // full-waveform LUT is not active and forceFullRefresh() inside
  // showUltraSleepScreen() will not render the image correctly, leaving the
  // previous screen frozen on the display.  EPD_Init() reloads the full LUT
  // and makes the subsequent refresh reliable regardless of prior state.
  if (display) display->EPD_Init();

  showUltraSleepScreen();   // final image; refresh completes (read_busy) before we continue
  delay(120);

  stopTransferMode();

  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  audio_playback_set_vol(0);
  amarSoundSetEnabled(false);

  // Deep-sleep the panel controller (image is retained on e-paper), then cut the
  // display and audio power rails so they draw nothing during MCU deep sleep.
  if (display) display->EPD_Sleep();
  board.POWEER_Audio_OFF();
  board.POWEER_EPD_OFF();

  // Keep the battery power latch engaged so the RTC domain stays alive to wake
  // us on a button press.
  board.VBAT_POWER_ON();

  uint64_t wakeMask = (1ULL << BTN_REC) | (1ULL << BTN_PWR);
  esp_sleep_enable_ext1_wakeup(wakeMask, ESP_EXT1_WAKEUP_ANY_LOW);

  delay(50);
  esp_deep_sleep_start();
}

// ─── checkBatteryWarning ────────────────────────────────────────────────────
//
// Sampled every BAT_CHECK_INTERVAL_MS.  Shows a low-battery overlay for
// 4 seconds when the level first drops below BAT_LOW_THRESHOLD (%), then
// stays silent until it recovers above BAT_RECOVER_THRESHOLD (hysteresis).
//
// Must only be called from STATE_IDLE (overlay draw assumes idle screen is
// already showing behind it).
//
#define BAT_WARN_DURATION_MS  4000UL

void checkBatteryWarning() {
  uint32_t now = millis();

  // Dismiss an active overlay once its display window expires.
  if (batWarnActive && now >= batWarnShowUntilMs) {
    batWarnActive = false;
    showIdle();   // redraw clean idle screen
  }

  // Throttle ADC reads.
  if (now - lastBatCheckMs < BAT_CHECK_INTERVAL_MS) return;
  lastBatCheckMs = now;

  int pct = readBatteryPercent();
  if (pct < 0) return;   // ADC not ready / no battery

  if (!batLowWarned && pct <= BAT_LOW_THRESHOLD) {
    batLowWarned          = true;
    batWarnActive         = true;
    batWarnShowUntilMs    = now + BAT_WARN_DURATION_MS;
    showBatteryLow(pct);
  } else if (batLowWarned && pct >= BAT_RECOVER_THRESHOLD) {
    // Recovered — allow the warning to fire again next time it drops.
    batLowWarned = false;
  }
}

// ─── checkAutoSleep ─────────────────────────────────────────────────────────────
//
// Puts the device into ultra-sleep after ULTRA_SLEEP_MS of inactivity.
// Guards: active transfer server or active recording suppress sleep.
//
void checkAutoSleep() {
  if (transferServerActive) return;
  if (state == STATE_RECORDING) return;
  if (millis() - lastActivityMs >= ULTRA_SLEEP_MS) {
    enterUltraSleep();
  }
}
