#include "Arduino.h"
#include "SD_MMC.h"
#include <time.h>

#include "../../config.h"
#include "../../globals.h"
#include "../../types.h"
#include "../../sounds.h"

#include "pet.h"
#include "draw.h"
#include "ui.h"
#include "buttons.h"
#include "rtc.h"

#include "cat_sprites/cat_sprites.h"

// ─── State (kept local to this module; globals.h stays untouched) ────────────
static int    petHunger = PET_START_HUNGER;
static int    petHappy  = PET_START_HAPPY;
static int    petEnergy = PET_START_ENERGY;
static time_t petBorn      = 0;
static time_t petLastSeen  = 0;

enum PetView { PV_MAIN, PV_STATS, PV_ACTION };
static PetView  petView        = PV_MAIN;
static int      petSel         = 0;   // action selector: 0 Feed, 1 Play, 2 Pet, 3 Stats
static uint32_t petActionUntil = 0;   // millis() when the action pose ends

static const char* ACTS[4] = { "Feed", "Play", "Pet", "Stats" };

// ─── Helpers ─────────────────────────────────────────────────────────────────
static inline int clampStat(int v) { return v < 0 ? 0 : (v > 100 ? 100 : v); }

// Current UTC epoch from the RTC chip, or 0 if the clock is not set.
static time_t petNow() {
  struct tm t;
  if (rtcReadUtcTm(&t)) return utcTmToEpoch(t);
  return 0;
}

// ─── Persistence: one CSV line on SD  "v1,born,lastSeen,hunger,happy,energy" ──
static void petSave() {
  File f = SD_MMC.open(PET_FILE, FILE_WRITE);
  if (!f) return;
  f.printf("v1,%ld,%ld,%d,%d,%d\n",
           (long)petBorn, (long)petLastSeen, petHunger, petHappy, petEnergy);
  f.close();
}

static bool petLoad() {
  File f = SD_MMC.open(PET_FILE);
  if (!f) return false;
  String line = f.readStringUntil('\n');
  f.close();

  long born = 0, seen = 0;
  int h = 0, ha = 0, e = 0;
  if (sscanf(line.c_str(), "v1,%ld,%ld,%d,%d,%d", &born, &seen, &h, &ha, &e) != 5)
    return false;

  petBorn     = (time_t)born;
  petLastSeen = (time_t)seen;
  petHunger   = clampStat(h);
  petHappy    = clampStat(ha);
  petEnergy   = clampStat(e);
  return true;
}

static void petInitNew(time_t now) {
  petHunger   = PET_START_HUNGER;
  petHappy    = PET_START_HAPPY;
  petEnergy   = PET_START_ENERGY;
  petBorn     = now;   // 0 if clock unknown; set later once time is available
  petLastSeen = now;
}

// Apply real-time decay/recovery once, on entering the screen.
static void petApplyDecay(time_t now) {
  if (petBorn == 0 && now > 0) petBorn = now;     // adopt birth time once known

  if (now == 0 || petLastSeen == 0) {             // clock not set yet
    petLastSeen = now;
    return;
  }

  long elapsed = (long)(now - petLastSeen);
  if (elapsed <= 0) { petLastSeen = now; return; } // clock moved backwards

  long capSec = (long)PET_ELAPSED_CAP_H * 3600L;
  if (elapsed > capSec) elapsed = capSec;

  float hours = elapsed / 3600.0f;
  petHunger = clampStat(petHunger - (int)(PET_DECAY_HUNGER_PH   * hours));
  petHappy  = clampStat(petHappy  - (int)(PET_DECAY_HAPPY_PH    * hours));
  petEnergy = clampStat(petEnergy + (int)(PET_RECOVER_ENERGY_PH * hours));
  petLastSeen = now;
}

// Mood → sprite (priority order per the feature spec).
static const uint8_t* petMoodSprite() {
  if (petEnergy < PET_TH_SLEEP_ENERGY)                      return cat_sleep;
  if (petHappy  < PET_TH_SAD && petHunger < PET_TH_SAD)     return cat_sad;
  if (petHunger < PET_TH_HUNGRY)                            return cat_hungry;
  if (petHunger > PET_TH_HAPPY && petHappy > PET_TH_HAPPY)  return cat_happy;
  return cat_content;
}

static void petAgeStr(char* buf, size_t n) {
  time_t now = petNow();
  if (petBorn == 0 || now == 0 || now < petBorn) { snprintf(buf, n, "--"); return; }
  long days = (long)((now - petBorn) / 86400L);
  snprintf(buf, n, "%ldd", days);
}

// ─── Rendering ───────────────────────────────────────────────────────────────
static void petDrawActionBar() {
  int fh = uiFontHeight(1);
  int h  = fh + 6;
  int y  = PET_BAR_Y;
  const int pad = 6, gap = 3;

  int ws[4], total = 0;
  for (int i = 0; i < 4; i++) { ws[i] = textW(ACTS[i], 1) + pad * 2; total += ws[i]; }
  total += gap * 3;

  int x = (W - total) / 2;
  if (x < 2) x = 2;
  for (int i = 0; i < 4; i++) {
    drawModernPill(x, y, ws[i], h, ACTS[i], i == petSel);
    x += ws[i] + gap;
  }
}

static void petDrawMain() {
  clearWhite();
  drawStr(12, 8, "cat", 1, BLACK);
  char age[12]; petAgeStr(age, sizeof(age));
  drawStr(W - 12 - textW(age, 1), 8, age, 1, BLACK);
  hline(12, 22, W - 24, BLACK);

  drawBitmap1BPP(PET_SPRITE_X, PET_SPRITE_Y, petMoodSprite(), CAT_W, CAT_H, BLACK);
  petDrawActionBar();
  refresh();
}

static void petDrawBar(int y, const char* label, int val) {
  drawStr(16, y, label, 1, BLACK);
  char v[8]; snprintf(v, sizeof(v), "%d", val);
  drawStr(W - 16 - textW(v, 1), y, v, 1, BLACK);

  int bx = 16, by = y + 15, bw = W - 32, bh = 14;
  strokeRoundRect(bx, by, bw, bh, 4, 1, BLACK);
  int fillw = (bw - 4) * val / 100;
  if (fillw > 0) fillRoundRect(bx + 2, by + 2, fillw, bh - 4, 3, BLACK);
}

static void petDrawStats() {
  clearWhite();
  drawStr(16, 14, "stats", 1, BLACK);
  hline(16, 32, W - 32, BLACK);

  petDrawBar(46,  "hunger", petHunger);
  petDrawBar(94,  "mood",   petHappy);
  petDrawBar(142, "energy", petEnergy);

  char age[12]; petAgeStr(age, sizeof(age));
  char line[24]; snprintf(line, sizeof(line), "age %s", age);
  drawStr(16, 182, line, 1, BLACK);
  drawStrC(W - 46, 182, "back", 1, BLACK);
  refresh();
}

static void petStartAction(const uint8_t* sprite) {
  petView = PV_ACTION;
  petActionUntil = millis() + PET_ACTION_MS;

  clearWhite();
  drawBitmap1BPP(PET_SPRITE_X, PET_SPRITE_Y, sprite, CAT_W, CAT_H, BLACK);
  refresh();
}

// ─── Actions ─────────────────────────────────────────────────────────────────
static void petDoAction(int sel) {
  switch (sel) {
    case 0:  // Feed
      petHunger = clampStat(petHunger + PET_FEED_HUNGER);
      soundSaved();
      petSave();
      petStartAction(cat_eat);
      break;
    case 1:  // Play
      petHappy  = clampStat(petHappy  + PET_PLAY_HAPPY);
      petEnergy = clampStat(petEnergy - PET_PLAY_ENERGY);
      soundSuccess();
      petSave();
      petStartAction(cat_play);
      break;
    case 2:  // Pet
      petHappy = clampStat(petHappy + PET_PET_HAPPY);
      soundSelect();
      petSave();
      petStartAction(cat_purr);
      break;
    case 3:  // Stats
      soundSelect();
      petView = PV_STATS;
      petDrawStats();
      break;
  }
}

// ─── Public entry points ─────────────────────────────────────────────────────
void petEnter() {
  time_t now = petNow();
  if (!petLoad()) petInitNew(now);
  petApplyDecay(now);
  petSave();

  petSel  = 0;
  petView = PV_MAIN;
  petDrawMain();
}

void petRedraw() {
  if (petView == PV_STATS) petDrawStats();
  else                     petDrawMain();
}

// ─── Touch entry points (called from handleTouch in amar_note.ino) ───────────
// Tap one of the 4 action pills by index (0=Feed, 1=Play, 2=Pet, 3=Stats).
void petTouchAction(int sel) {
  if (petView == PV_ACTION) return;   // ignore taps during pose animation
  if (sel < 0 || sel > 3)  return;
  if (petView == PV_STATS) {
    // Any pill tap from Stats returns to main view.
    soundBack();
    petView = PV_MAIN;
    petDrawMain();
    return;
  }
  petSel = sel;
  petDoAction(sel);
}

// Tap the back strip (top ~22 px) — exits to menu, or stats → main.
void petTouchBack() {
  if (petView == PV_ACTION) return;   // ignore during pose animation
  if (petView == PV_STATS) {
    soundBack();
    petView = PV_MAIN;
    petDrawMain();
    return;
  }
  // PV_MAIN — back to menu
  soundBack();
  petSave();
  menuCursor = 5;              // Tamagotchi is index 5 in our 6-item menu
  state = STATE_TAMAGOTCHI;    // will be overwritten below, kept for symmetry
  state = STATE_MENU;
  showMenu(menuCursor);
}

void petLoop() {
  // Hold an action pose, then return to the main view with the new mood.
  if (petView == PV_ACTION) {
    if ((int32_t)(millis() - petActionUntil) >= 0) {
      petView = PV_MAIN;
      petDrawMain();
    }
    return;
  }

  // NOTE: pollButton() is our API (not readButtonEvent).
  // isPwr=false for REC so it doesn't trigger the power latch on long-press.
  ButtonEvent rec = pollButton(BTN_REC, false);
  ButtonEvent pwr = pollButton(BTN_PWR, true);

  // Back to menu: REC long-press (EV_DOUBLE is not in our ButtonEvent enum).
  if (rec == EV_LONG) {
    soundBack();
    petSave();
    menuCursor = 5;   // Tamagotchi is index 5 in our 6-item menu
    state = STATE_MENU;
    showMenu(menuCursor);
    return;
  }

  if (petView == PV_STATS) {
    if (rec == EV_SINGLE || pwr == EV_SINGLE) {
      soundBack();
      petView = PV_MAIN;
      petDrawMain();
    }
    return;
  }

  // PV_MAIN — PWR cycles selection, REC confirms.
  if (pwr == EV_SINGLE) {
    soundNext();
    petSel = (petSel + 1) % 4;
    petDrawMain();
  } else if (rec == EV_SINGLE) {
    petDoAction(petSel);
  }
}
