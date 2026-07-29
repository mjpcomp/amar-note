#include "Arduino.h"
#include "../../config.h"
#include "../../globals.h"
#include "../../types.h"
#include "ui.h"
#include "draw.h"
#include "notes.h"
#include "battery.h"
#include "rtc.h"
#include "../../logo_bitmap.h"
#include "../../sounds.h"
#include "config_store.h"
#include "SD_MMC.h"

#define W   200
#define H   200

// ─── Icons ────────────────────────────────────────────────────────────────────────────────────
void iconMicWhite(int cx, int cy) {
  fillRect(cx-13, cy-36, 26, 44, WHITE);
  fillCircle(cx, cy-36, 13, WHITE);
  fillCircle(cx, cy+8,  13, WHITE);
  strokeCircle(cx, cy-4, 40, 5, WHITE);
  fillRect(cx-50, cy-50, 100, 50, BLACK);
  fillRect(cx-3,  cy+38, 6,  18, WHITE);
  fillRect(cx-24, cy+54, 48,  5, WHITE);
}

void iconRecordBig(int cx, int cy) {
  fillCircle(cx, cy, 36, WHITE);
  strokeCircle(cx, cy, 52, 5, WHITE);
  strokeCircle(cx, cy, 68, 2, WHITE);
}

void iconCheck(int cx, int cy, bool filled) {
  if (filled) {
    fillCircle(cx, cy, 44, BLACK);
    for (int t=-3;t<=3;t++) {
      line(cx-22, cy-2+t, cx-6, cy+17+t, WHITE);
      line(cx-6,  cy+17+t, cx+30, cy-22+t, WHITE);
    }
  } else {
    strokeCircle(cx, cy, 44, 3, BLACK);
    for (int t=-2;t<=2;t++) {
      line(cx-22, cy-2+t, cx-6, cy+17+t, BLACK);
      line(cx-6,  cy+17+t, cx+30, cy-22+t, BLACK);
    }
  }
}

void iconError(int cx, int cy) {
  strokeCircle(cx, cy, 44, 3, BLACK);
  for (int t=-3;t<=3;t++) {
    line(cx-22, cy-22+t, cx+22, cy+22+t, BLACK);
    line(cx+22, cy-22+t, cx-22, cy+22+t, BLACK);
  }
}

void iconThinking(int cx, int cy) {
  fillCircle(cx-28, cy, 8, BLACK);
  fillCircle(cx,    cy, 8, BLACK);
  fillCircle(cx+28, cy, 8, BLACK);
}

void iconTag(int cx, int cy) {
  const int pts[5][2] = {
    {cx-26, cy+4}, {cx-4, cy-18}, {cx+32, cy-18},
    {cx+32, cy+12}, {cx+4,  cy+36}
  };
  for(int i=0;i<4;i++) thickLine(pts[i][0],pts[i][1],pts[i+1][0],pts[i+1][1],4,BLACK);
  thickLine(pts[4][0],pts[4][1],pts[0][0],pts[0][1],4,BLACK);
  fillCircle(cx-2, cy-4, 5, BLACK);
}

void iconSync(int cx, int cy) {
  strokeCircle(cx, cy, 40, 4, BLACK);
  fillRect(cx+16, cy-46, 20, 20, WHITE);
  thickLine(cx+16, cy-36, cx+36, cy-36, 3, BLACK);
  thickLine(cx+36, cy-36, cx+26, cy-46, 3, BLACK);
  thickLine(cx+36, cy-36, cx+26, cy-26, 3, BLACK);
  fillRect(cx-36, cy+26, 20, 20, WHITE);
  thickLine(cx-36, cy+36, cx-16, cy+36, 3, BLACK);
  thickLine(cx-16, cy+36, cx-26, cy+26, 3, BLACK);
  thickLine(cx-16, cy+36, cx-26, cy+46, 3, BLACK);
}

void iconWifi(int cx, int cy) {
  int base = cy + 26;
  strokeCircle(cx, base, 50, 5, BLACK);
  strokeCircle(cx, base, 32, 5, BLACK);
  strokeCircle(cx, base, 14, 5, BLACK);
  fillRect(0, base, W, H - base, WHITE);
  fillCircle(cx, base, 5, BLACK);
}

void iconNoteLines(int cx, int cy) {
  fillRect(cx-32, cy-12, 64, 6, BLACK);
  fillRect(cx-32, cy+2,  64, 6, BLACK);
  fillRect(cx-32, cy+16, 44, 6, BLACK);
}

void iconUsbDrive(int cx, int cy) {
  strokeRoundRect(cx-32, cy-18, 56, 36, 5, 3, BLACK);
  fillRect(cx+24, cy-8, 16, 16, BLACK);
  fillRect(cx+27, cy-5, 4, 5, WHITE);
  fillRect(cx+27, cy+2, 4, 5, WHITE);
  fillCircle(cx-12, cy, 4, BLACK);
  hline(cx-24, cy-6, 30, BLACK);
  hline(cx-24, cy+2, 22, BLACK);
}

// ── Menu tile icon: circular sync arrows (small, centred in tile) ────────────
static void iconSyncSmall(int cx, int cy, uint8_t col) {
  strokeCircle(cx, cy, 12, 2, col);
  // Top-right arrow head pointing clockwise
  fillRect(cx+7, cy-14, 7, 7, col == WHITE ? BLACK : WHITE);  // erase arc segment
  thickLine(cx+7,  cy-12, cx+13, cy-12, 2, col);
  thickLine(cx+13, cy-12, cx+9,  cy-17, 2, col);
  thickLine(cx+13, cy-12, cx+9,  cy-7,  2, col);
  // Bottom-left arrow head pointing clockwise
  fillRect(cx-14, cy+7, 7, 7, col == WHITE ? BLACK : WHITE);
  thickLine(cx-13, cy+12, cx-7,  cy+12, 2, col);
  thickLine(cx-7,  cy+12, cx-11, cy+17, 2, col);
  thickLine(cx-7,  cy+12, cx-11, cy+7,  2, col);
}

// ── Menu tile icon: gear / cog (Settings) ────────────────────────────────────
static void iconGear(int cx, int cy, uint8_t col) {
  // Outer ring with 6 teeth
  strokeCircle(cx, cy, 12, 2, col);
  fillCircle(cx, cy, 5, col);              // hub
  strokeCircle(cx, cy, 5, 2, col == WHITE ? BLACK : WHITE);  // hub hole
  // 6 teeth at 0, 60, 120, 180, 240, 300 degrees
  const int toothLen = 5;
  const float angles[6] = {0.0f, 1.047f, 2.094f, 3.141f, 4.189f, 5.236f};
  for (int i = 0; i < 6; i++) {
    float a = angles[i];
    int x0 = cx + (int)(12 * cosf(a));
    int y0 = cy + (int)(12 * sinf(a));
    int x1 = cx + (int)((12 + toothLen) * cosf(a));
    int y1 = cy + (int)((12 + toothLen) * sinf(a));
    thickLine(x0, y0, x1, y1, 3, col);
  }
}

// ── Menu tile icon: USB trident / plug ───────────────────────────────────────
static void iconUsbPlug(int cx, int cy, uint8_t col) {
  // Connector body
  strokeRoundRect(cx-9, cy-4, 18, 12, 3, 2, col);
  // Cable stem
  fillRect(cx-2, cy+8, 4, 8, col);
  // Trident: vertical stem + 3 prongs
  fillRect(cx-1, cy-14, 3, 10, col);  // stem
  // Left prong
  thickLine(cx-6, cy-14, cx-6, cy-9, 2, col);
  thickLine(cx-6, cy-14, cx-1, cy-14, 2, col);
  // Right prong
  thickLine(cx+6, cy-14, cx+6, cy-9, 2, col);
  thickLine(cx+6, cy-14, cx+1, cy-14, 2, col);
  // Top dot
  fillCircle(cx, cy-17, 2, col);
}

// ── Menu tile icon: cute cat face (Tamagotchi) ───────────────────────────────
//
// Head circle + two pointy ears + two dot eyes + small W-mouth.
static void iconCatFace(int cx, int cy, uint8_t col) {
  // Head
  strokeCircle(cx, cy+2, 13, 2, col);
  // Left ear (triangle: filled)
  fillTriangle(cx-13, cy-8,  cx-6,  cy-8,  cx-10, cy-18, col);
  // Right ear
  fillTriangle(cx+13, cy-8,  cx+6,  cy-8,  cx+10, cy-18, col);
  // Eyes
  fillCircle(cx-5, cy,   2, col);
  fillCircle(cx+5, cy,   2, col);
  // Nose dot
  fillCircle(cx,   cy+4, 1, col);
  // W-mouth
  thickLine(cx-5, cy+6, cx-2, cy+9,  2, col);
  thickLine(cx-2, cy+9, cx,   cy+7,  2, col);
  thickLine(cx,   cy+7, cx+2, cy+9,  2, col);
  thickLine(cx+2, cy+9, cx+5, cy+6,  2, col);
}

// ─── Layout helpers ────────────────────────────────────────────────────────────────────────────
void drawHeader(const char* title, const char* rightInfo) {
  fillRect(0, 0, W, 28, BLACK);
  drawStrC(W/2, 10, title, 1, WHITE);
  if (rightInfo) {
    int rw = textW(rightInfo, 1);
    drawStr(W - 8 - rw, 10, rightInfo, 1, WHITE);
  }
}

void drawHints(const char* recLabel, const char* pwrLabel) {
  hline(0, 179, W, BLACK);
  fillRect(0, 180, W, 20, WHITE);
  drawStr(8, 186, recLabel, 1, BLACK);
  int rw = textW(pwrLabel, 1);
  drawStr(W - 8 - rw, 186, pwrLabel, 1, BLACK);
}

void drawBadge(int cx, int cy, const char* text, bool filled) {
  char up[32]; uppercaseCopy(up, text, sizeof(up));
  int tw = textW(up, 1);
  int bw = tw + 20, bh = 20;
  int bx = cx - bw/2, by = cy - bh/2;
  if (filled) {
    fillRoundRect(bx, by, bw, bh, 9, BLACK);
    drawStrC(cx, by + 6, up, 1, WHITE);
  } else {
    strokeRoundRect(bx, by, bw, bh, 9, 2, BLACK);
    drawStrC(cx, by + 6, up, 1, BLACK);
  }
}

void drawPageDots(int cur, int total) {
  if (total <= 1) return;
  int n = min(total, 7);
  int gap = 16;
  int startX = W/2 - ((n-1)*gap)/2;
  for (int i = 0; i < n; i++) {
    int x = startX + i*gap, y = 168;
    if (i == cur % n) fillCircle(x, y, 5, BLACK);
    else              strokeCircle(x, y, 4, 1, BLACK);
  }
}

void drawChevronRight(int x, int cy, uint8_t c) {
  thickLine(x,   cy-8, x+8, cy,   2, c);
  thickLine(x+8, cy,   x,   cy+8, 2, c);
}

void drawTinyHint(const char* left, const char* right) {
  (void)left; (void)right;
}

void drawKicker(const char* txt, int y) {
  char up[40]; uppercaseCopy(up, txt, sizeof(up));
  drawStrC(W/2, y, up, 1, BLACK);
}

void drawSoftFrame() {
  strokeRoundRect(12, 12, W-24, H-24, 10, 1, BLACK);
}

void drawProductWordmark(int cx, int y, uint8_t color) {
  drawStr(cx - textW("amar", 2) / 2, y,      "amar", 2, color);
  drawStr(cx - textW("note", 2) / 2, y + 22, "note", 2, color);
}

void drawModernPill(int x, int y, int w, int h, const char* label, bool active) {
  if (active) {
    fillRoundRect(x, y, w, h, h/2, BLACK);
    drawStrInBox(x, y, w, h, label, 1, WHITE);
  } else {
    strokeRoundRect(x, y, w, h, h/2, 1, BLACK);
    drawStrInBox(x, y, w, h, label, 1, BLACK);
  }
}

void drawDotSelector(int cur, int total, int y) {
  int gap = 17, startX = W/2 - ((total-1)*gap)/2;
  for (int i=0; i<total; i++) {
    int x = startX + i*gap;
    if (i == cur) fillCircle(x, y, 4, BLACK);
    else          strokeCircle(x, y, 4, 1, BLACK);
  }
}

void drawCheckSmall(int cx, int cy, uint8_t color) {
  strokeCircle(cx, cy, 13, 1, color);
  thickLine(cx-6, cy, cx-1, cy+5, 2, color);
  thickLine(cx-1, cy+5, cx+8, cy-6, 2, color);
}

void drawMinimalDocIcon(int cx, int cy, uint8_t color) {
  strokeRoundRect(cx-13, cy-16, 26, 32, 3, 2, color);
  hline(cx-7, cy-5, 14, color);
  hline(cx-7, cy+4, 14, color);
  hline(cx-7, cy+13, 9, color);
}

void drawMinimalTagIcon(int cx, int cy, uint8_t color) {
  thickLine(cx-13, cy, cx-2, cy-13, 2, color);
  thickLine(cx-2, cy-13, cx+14, cy-13, 2, color);
  thickLine(cx+14, cy-13, cx+14, cy+2, 2, color);
  thickLine(cx+14, cy+2, cx+2, cy+15, 2, color);
  thickLine(cx+2, cy+15, cx-13, cy, 2, color);
  fillCircle(cx+4, cy-5, 3, color);
}

void drawMinimalCloudIcon(int cx, int cy, uint8_t color) {
  strokeCircle(cx-8, cy+2, 10, 2, color);
  strokeCircle(cx+4, cy-4, 13, 2, color);
  strokeCircle(cx+15, cy+4, 9, 2, color);
  fillRect(cx-22, cy+4, 47, 16, WHITE);
  hline(cx-21, cy+10, 44, color);
}

void drawMenuTile(int x, int y, int w, int h, const char* label, int icon, bool active) {
  if (active) fillRoundRect(x, y, w, h, 12, BLACK);
  else        strokeRoundRect(x, y, w, h, 12, 1, BLACK);
  uint8_t col = active ? WHITE : BLACK;
  int cx = x + w/2;
  fillCircle(cx, y + 17, 4, col);
  drawStrInBox(x + 4, y + 29, w - 8, 18, label, 1, col);
}

void drawNoteCard(int y, int idx, bool active) {
  const int x = 16, w = 168, h = 39;
  if (active) fillRoundRect(x, y, w, h, 8, BLACK);
  else        strokeRoundRect(x, y, w, h, 8, 1, BLACK);
  uint8_t col = active ? WHITE : BLACK;

  char n[8]; snprintf(n, sizeof(n), "#%03d", noteIndex[idx].num);
  String tagLabel = normalizeForDisplay(String(noteIndex[idx].tag));
  drawStr(x + 10, y + 5, n, 1, col);
  drawStrFit(x + 66, y + 5, 88, tagLabel.c_str(), 1, col);
  String ticker = noteTickerText(idx);
  drawTickerText(x + 10, y + 22, 145, ticker, active, col);
}

void drawListMenuCard(int y, const char* title, const char* meta, bool active) {
  const int x = 16, w = 168, h = 32;
  if (active) fillRoundRect(x, y, w, h, 8, BLACK);
  else        strokeRoundRect(x, y, w, h, 8, 1, BLACK);
  uint8_t col = active ? WHITE : BLACK;
  drawStrFit(x + 10, y + 8, meta ? 92 : 140, title, 1, col);
  if (meta && strlen(meta) > 0) {
    int mw = min(textW(meta, 1), 56);
    drawStrFit(x + w - 10 - mw, y + 8, 56, meta, 1, col);
  }
}

// ─── Screens ──────────────────────────────────────────────────────────────────────────────────
static void drawBolt(int x, int y) {
  fillTriangle(x+7, y,    x+1, y+9,  x+6, y+9,  BLACK);
  fillTriangle(x+5, y+8,  x+10, y+8, x+3, y+18, BLACK);
}

static void drawBatteryIcon(int x, int y, int bw, int bh, int pct, uint8_t color) {
  strokeRect(x, y, bw, bh, 1, color);
  int nubH = max(2, bh / 3);
  int nubY = y + (bh - nubH) / 2;
  fillRect(x + bw, nubY, 2, nubH, color);
  if (pct > 0) {
    int fillW = ((bw - 4) * constrain(pct, 0, 100)) / 100;
    if (fillW > 0) fillRect(x + 2, y + 2, fillW, bh - 4, color);
  }
}

void showIdle() {
  clearWhite();
  int  batt     = readBatteryPercent();
  bool charging = isBatteryCharging();
  drawBatteryRing(batt);
  drawProductWordmark(100, 58, BLACK);

  char b[8];
  if (batt < 0) snprintf(b, sizeof(b), "--");
  else          snprintf(b, sizeof(b), "%d%%", batt);

  const int iconW = 18;
  const int iconH = 10;
  const int iconTotalW = iconW + 2;
  const int gap   = 4;
  int tw      = textW(b, 1);
  int boltW   = charging ? 14 : 0;
  int totalW  = boltW + iconTotalW + gap + tw;
  int startX  = 100 - totalW / 2;

  int cx = startX;
  if (charging) { drawBolt(cx, 131); cx += boltW; }
  drawBatteryIcon(cx, 135, iconW, iconH, batt, BLACK);
  cx += iconTotalW + gap;
  drawStr(cx, 144, b, 1, BLACK);

  refresh();
}

void showBatteryLow(int pct) {
  fillRect(0, 0, W, H, BLACK);
  fillRect(95, 48, 10, 50, WHITE);
  fillRect(95, 108, 10, 10, WHITE);
  char buf[8]; snprintf(buf, sizeof(buf), "%d%%", pct);
  drawStrC(100, 132, buf,       2, WHITE);
  drawStrC(100, 160, "battery", 1, WHITE);
  drawStrC(100, 176, "low",     1, WHITE);
  refresh();
}

// ─── Recording screen: animated microphone with sound arcs ────────────────────────────────────
static float recCircleR = 24.0f;

static void drawMicBody(int cx, int micCy) {
  fillRoundRect(cx - 11, micCy - 18, 22, 36, 10, WHITE);
  fillCircle(cx, micCy - 18, 11, WHITE);
  strokeCircle(cx, micCy, 22, 3, WHITE);
  fillRect(cx - 26, micCy - 26, 52, 28, BLACK);
  fillRect(cx - 1, micCy + 18, 3, 14, WHITE);
  fillRect(cx - 15, micCy + 31, 30, 3, WHITE);
}

static void drawSoundArcs(int cx, int micCy, int arcR) {
  if (arcR < 26) return;
  strokeCircle(cx, micCy, arcR,      2, WHITE);
  int inner = arcR - 12;
  if (inner > 24) strokeCircle(cx, micCy, inner, 2, WHITE);
  int maskH = arcR / 2 + 2;
  fillRect(cx - arcR - 4, micCy - arcR - 4, 2*(arcR+4), maskH + 4, BLACK);
  fillRect(cx - arcR - 4, micCy + arcR - maskH + 2, 2*(arcR+4), maskH + 6, BLACK);
}

static void drawRecordingScreen(uint32_t elapsedMs, int level) {
  (void)elapsedMs;
  fillRect(0, 0, W, H, BLACK);
  float target = 26.0f + (float)level * 42.0f / 152.0f;
  if (target < 26.0f) target = 26.0f;
  if (target > 68.0f) target = 68.0f;
  float a = (target > recCircleR) ? 0.55f : 0.18f;
  recCircleR += (target - recCircleR) * a;
  int cx     = W / 2;
  int micCy  = 90;
  int arcR   = (int)(recCircleR + 0.5f);
  drawSoundArcs(cx, micCy, arcR);
  drawMicBody(cx, micCy);
}

void showRecording() {
  recCircleR = 24.0f;
  drawRecordingScreen(0, 0);
  refresh();
}

void showRecordingLive(uint32_t elapsedMs, int level) {
  if (displayBusy()) return;
  drawRecordingScreen(elapsedMs, level);
  display->EPD_DisplayPartTrigger();
}

void showSaved(int num) {
  clearWhite();
  drawCheckSmall(100, 46, BLACK);
  drawStrC(100, 76, "saved", 1, BLACK);
  char b[8]; snprintf(b, sizeof(b), "#%03d", num);
  drawStrC(100, 105, b, 2, BLACK);
  refresh();
}

void showTagSelect(int cursor) {
  clearWhite();
  if (tagCount <= 0) {
    drawKicker("no tags", 34);
    drawStrC(100, 100, "open portal", 1, BLACK);
    refresh();
    return;
  }
  drawKicker("choose tag", 17);
  const int x = 36, w = 128, h = 21, gap = 7;
  int y0 = 40;
  cursor = constrain(cursor, 0, max(tagCount - 1, 0));
  for (int i=0; i<tagCount; i++) {
    int y = y0 + i*(h+gap);
    drawModernPill(x, y, w, h, tags[i], i == cursor);
  }
  refresh();
}

// ─── Menu layout ──────────────────────────────────────────────────────────────────────────────
//
// Screen is 200×200 px.  Available below header (y=0..27): 172 px.
//
// Row A  — Notes (left) + Tags (right) — tall portrait tiles
//          y=30, h=50
//          Left:  x=8,  w=90
//          Right: x=102, w=90
//
// Row B  — 2×2 icon tile grid (Sync | Settings | USB | Tamagotchi)
//          Tile size: w=88, h=48, gap=4
//          Left column:  x=8
//          Right column: x=104
//          Top row y=86,  bottom row y=138
//          Bottom edge:  138+48 = 186 px  (4 px margin to 190; safe)
//
// Touch hit-zones mirror these coordinates exactly (see handleTouch in .ino).
//   cursor 0 = Notes   tile  (8,  30, 90, 50)
//   cursor 1 = Tags    tile  (102,30, 90, 50)
//   cursor 2 = Sync    tile  (8,  86, 88, 48)
//   cursor 3 = Settings tile (104,86, 88, 48)
//   cursor 4 = USB     tile  (8,  138,88, 48)
//   cursor 5 = Tamagotchi    (104,138,88, 48)

void showMenu(int cursor) {
  clearWhite();
  drawStr(16, 14, "menu", 1, BLACK);
  hline(16, 26, W-32, BLACK);

  // ── Row A: Notes (0) + Tags (1) ──────────────────────────────────────
  const int tileAy = 30, tileAh = 50;
  const int tileL_x = 8,   tileL_w = 90;
  const int tileR_x = 102, tileR_w = 90;

  // Notes tile
  {
    bool active = (cursor == 0);
    if (active) fillRoundRect(tileL_x, tileAy, tileL_w, tileAh, 10, BLACK);
    else        strokeRoundRect(tileL_x, tileAy, tileL_w, tileAh, 10, 1, BLACK);
    uint8_t col = active ? WHITE : BLACK;
    drawMinimalDocIcon(tileL_x + tileL_w/2, tileAy + 18, col);
    drawStrInBox(tileL_x + 2, tileAy + tileAh - 16, tileL_w - 4, 14, "Notes", 1, col);
  }

  // Tags tile
  {
    bool active = (cursor == 1);
    if (active) fillRoundRect(tileR_x, tileAy, tileR_w, tileAh, 10, BLACK);
    else        strokeRoundRect(tileR_x, tileAy, tileR_w, tileAh, 10, 1, BLACK);
    uint8_t col = active ? WHITE : BLACK;
    drawMinimalTagIcon(tileR_x + tileR_w/2, tileAy + 18, col);
    drawStrInBox(tileR_x + 2, tileAy + tileAh - 16, tileR_w - 4, 14, "Tags", 1, col);
  }

  // ── Row B: 2×2 icon tile grid ─────────────────────────────────────────
  //   [2] Sync      [3] Settings
  //   [4] USB       [5] Tamagotchi
  const int gridTileW = 88, gridTileH = 48, gridR = 8;
  const int gridLx = 8, gridRx = 104;
  const int gridTy = 86, gridBy = 138;        // top-row y, bottom-row y
  const int iconOffY = 16;                    // icon centre relative to tile top
  const int lblH    = 13;                     // label box height at tile bottom

  struct { int x; int y; int idx; const char* label; } tiles[4] = {
    { gridLx, gridTy,  2, "Sync"       },
    { gridRx, gridTy,  3, "Settings"   },
    { gridLx, gridBy,  4, "USB"        },
    { gridRx, gridBy,  5, "Tamagotchi" },
  };

  for (int t = 0; t < 4; t++) {
    int x = tiles[t].x, y = tiles[t].y, mi = tiles[t].idx;
    bool active = (cursor == mi);
    if (active) fillRoundRect(x, y, gridTileW, gridTileH, gridR, BLACK);
    else        strokeRoundRect(x, y, gridTileW, gridTileH, gridR, 1, BLACK);
    uint8_t col = active ? WHITE : BLACK;
    int icx = x + gridTileW / 2;
    int icy = y + iconOffY;
    switch (mi) {
      case 2: iconSyncSmall(icx, icy, col); break;
      case 3: iconGear(icx, icy, col);      break;
      case 4: iconUsbPlug(icx, icy, col);   break;
      case 5: iconCatFace(icx, icy, col);   break;
    }
    drawStrInBox(x + 2, y + gridTileH - lblH - 2, gridTileW - 4, lblH, tiles[t].label, 1, col);
  }

  refresh();
}

void showTagBrowser(int cursor) {
  clearWhite();
  if (tagCount <= 0) {
    drawKicker("tags", 16);
    drawStrC(100, 100, "no tags", 1, BLACK);
    refresh();
    return;
  }
  drawKicker("tags", 16);
  fillRoundRect(28, 56, 144, 54, 17, BLACK);
  cursor = constrain(cursor, 0, max(tagCount - 1, 0));
  drawStrInBox(28, 56, 144, 54, tags[cursor], 2, WHITE);
  int cnt = 0;
  for (int i=0; i<(int)noteIndex.size(); i++)
    if (strcmp(noteIndex[i].tag, tags[cursor])==0) cnt++;
  char cb[20]; snprintf(cb, sizeof(cb), "%d notes", cnt);
  drawStrC(100, 130, cb, 1, BLACK);
  refresh();
}

void showNoteList(int cursor) {
  if (tickerCursor != cursor) {
    tickerCursor = cursor;
    tickerOffset = 0;
    tickerLastMs = millis();
  }
  clearWhite();
  int count = filteredCount();
  char cb[16]; snprintf(cb, sizeof(cb), "%d notes", count);
  drawStr(16, 14, "notes", 1, BLACK);
  int cw = textW(cb, 1);
  drawStr(W-16-cw, 14, cb, 1, BLACK);
  if (count <= 0) {
    drawMinimalDocIcon(100, 76, BLACK);
    drawStrC(100, 116, "no notes yet", 1, BLACK);
    refresh();
    return;
  }
  const int pageSize = 3;
  int pageStart = (cursor / pageSize) * pageSize;
  int activeRow = cursor - pageStart;
  const int y0 = 43, step = 47;
  int shown = min(pageSize, count - pageStart);
  for (int row=0; row<shown; row++) {
    int vis = pageStart + row;
    int idx = noteAtFilteredIndex(vis);
    if (idx >= 0) drawNoteCard(y0 + row*step, idx, row == activeRow);
  }
  refresh();
}

void showNoteDetail(int cursor) {
  clearWhite();
  int idx = noteAtFilteredIndex(cursor);
  if (idx < 0) {
    drawStrC(100, 96, "not found", 1, BLACK);
    refresh();
    return;
  }
  char n[8]; snprintf(n, sizeof(n), "#%03d", noteIndex[idx].num);
  drawStr(16, 14, n, 1, BLACK);
  String tagLabel = normalizeForDisplay(String(noteIndex[idx].tag));
  int tw = textW(tagLabel.c_str(), 1);
  drawStrFit(W-16-min(tw, 82), 14, 82, tagLabel.c_str(), 1, BLACK);
  hline(16, 32, W-32, BLACK);

  if (noteIndex[idx].hasText) {
    char txtPath[64];
    snprintf(txtPath, sizeof(txtPath), "%s/note_%03d.txt", NOTES_DIR, noteIndex[idx].num);
    File f = SD_MMC.open(txtPath);
    char text[2048] = {0};
    if (f) { f.read((uint8_t*)text, 2047); f.close(); }
    String bodyText = normalizeForDisplay(String(text));
    const int linesPerPage = 7;
    int skip = detailScrollPage * linesPerPage;
    detailTotalLines = drawWrappedText(18, 48, 164, 18, linesPerPage, bodyText, BLACK, skip);
    int totalPages = (detailTotalLines + linesPerPage - 1) / linesPerPage;
    if (totalPages > 1) {
      char pageLabel[12];
      snprintf(pageLabel, sizeof(pageLabel), "%d/%d", detailScrollPage + 1, totalPages);
      int lw = textW(pageLabel, 1);
      drawStr(W - 8 - lw, 186, pageLabel, 1, BLACK);
      hline(0, 179, W, BLACK);
    }
  } else {
    iconThinking(100, 82);
    drawStrC(100, 122, "not synced", 1, BLACK);
  }
  refresh();
}

void showDeleteConfirm(int noteNum) {
  clearWhite();
  fillRect(0, 0, W, 28, BLACK);
  drawStrC(W/2, 10, "DELETE", 1, WHITE);
  char label[16]; snprintf(label, sizeof(label), "#%03d", noteNum);
  drawStrC(W/2, 52, label, 2, BLACK);
  drawStrC(W/2, 88, "Delete this note?", 1, BLACK);
  drawStrC(W/2, 108, "WAV + TXT + meta", 1, BLACK);
  hline(0, 179, W, BLACK);
  fillRect(0, 180, W, 20, WHITE);
  drawStr(8, 186, "confirm", 1, BLACK);
  int rw = textW("cancel", 1);
  drawStr(W - 8 - rw, 186, "cancel", 1, BLACK);
  refresh();
}

void showObsidianSync(int done, int total) {
  clearWhite();
  drawKicker("vault", 20);
  iconSync(100, 76);
  int barW = 144, barH = 10, barX = 28, barY = 116;
  strokeRoundRect(barX, barY, barW, barH, 5, 1, BLACK);
  if (total > 0) {
    int fill = (done * (barW - 4)) / max(total, 1);
    if (fill > 0) fillRoundRect(barX+2, barY+2, fill, barH-4, 3, BLACK);
    char b[20]; snprintf(b, sizeof(b), "%d / %d", done, total);
    drawStrC(100, 142, b, 1, BLACK);
  } else {
    drawStrC(100, 142, "please wait", 1, BLACK);
  }
  refresh();
}

void showTranscribing(int done, int total) {
  clearWhite();
  drawKicker("syncing", 20);
  iconThinking(100, 76);
  int barW = 144, barH = 10, barX = 28, barY = 116;
  strokeRoundRect(barX, barY, barW, barH, 5, 1, BLACK);
  if (total > 0) {
    int fill = (done * (barW - 4)) / max(total, 1);
    if (fill > 0) fillRoundRect(barX+2, barY+2, fill, barH-4, 3, BLACK);
    char b[20]; snprintf(b, sizeof(b), "%d / %d", done, total);
    drawStrC(100, 142, b, 1, BLACK);
  } else {
    drawStrC(100, 142, "please wait", 1, BLACK);
  }
  refresh();
}

void showWifiConnecting(int attempt, int maxA) {
  clearWhite();
  drawKicker("wifi", 20);
  iconWifi(100, 84);
  int barW = 130, barH = 10, barX = 35, barY = 140;
  strokeRoundRect(barX, barY, barW, barH, 5, 1, BLACK);
  int fill = (attempt * (barW - 4)) / max(maxA, 1);
  if (fill > 0) fillRoundRect(barX+2, barY+2, fill, barH-4, 3, BLACK);
  char b[20]; snprintf(b, sizeof(b), "%d / %d", attempt, maxA);
  drawStrC(100, 164, b, 1, BLACK);
  refresh();
}

void showDone() {
  clearWhite();
  drawCheckSmall(100, 70, BLACK);
  drawStrC(100, 105, "all done", 1, BLACK);
  refresh();
}

void showError(const char* msg) {
  clearWhite();
  iconError(100, 70);
  if (msg && strlen(msg) > 0) drawStrC(100, 118, msg, 1, BLACK);
  else drawStrC(100, 118, "error", 1, BLACK);
  refresh();
}

void showUltraSleepScreen() {
  clearWhite();
  #ifdef LOGO_WIDTH
    drawBitmap1BPP((W - LOGO_WIDTH) / 2, (H - LOGO_HEIGHT) / 2,
                   logo_bitmap, LOGO_WIDTH, LOGO_HEIGHT, BLACK);
  #else
    drawProductWordmark(100, 70, BLACK);
  #endif
  forceFullRefresh();
}

void showPlaybackOverlay() {
  fillRoundRect(75, 145, 50, 34, 11, BLACK);
  fillTriangle(95, 154, 95, 170, 110, 162, WHITE);
  refresh();
}

void showTransferConnecting() {
  clearWhite();
  drawKicker("transfer", 18);
  iconWifi(100, 82);
  drawStrC(100, 138, "connecting", 1, BLACK);
  refresh();
}

void showTransferMode(const char* ip) {
  clearWhite();
  drawKicker("transfer", 16);
  fillRoundRect(26, 48, 148, 58, 16, BLACK);
  drawStrInBox(26, 48, 148, 24, "amar note portal", 1, WHITE);
  drawStrInBox(26, 74, 148, 24, "active", 1, WHITE);
  drawStrC(100, 124, "open browser", 1, BLACK);
  drawStrC(100, 146, ip, 1, BLACK);
  drawStrC(100, 169, "hold rec to exit", 1, BLACK);
  refresh();
}

// ─── Settings screen ──────────────────────────────────────────────────────────
void showSettings(int cursor) {
  clearWhite();
  drawStr(16, 14, "settings", 1, BLACK);
  hline(16, 32, W-32, BLACK);
  const int y0 = 38, step = 26, boxH = 22;
  for (int row = 0; row < SETTINGS_COUNT; row++) {
    bool active = row == cursor;
    int y = y0 + row * step;
    if (active) fillRoundRect(16, y, 168, boxH, 6, BLACK);
    else        strokeRoundRect(16, y, 168, boxH, 6, 1, BLACK);
    uint8_t col = active ? WHITE : BLACK;
    if (row == 0) {
      drawStr(28, y + 6, "sounds", 1, col);
      drawStr(W - 70, y + 6, amarSoundIsEnabled() ? "on" : "off", 1, col);
    } else if (row == 1) {
      drawStr(28, y + 6, "transfer", 1, col);
    } else if (row == 2) {
      drawStr(28, y + 6, "device", 1, col);
    } else if (row == 3) {
      drawStr(28, y + 6, "erase all", 1, col);
    } else if (row == 4) {
      drawStr(28, y + 6, "reset", 1, col);
    } else {
      drawStr(28, y + 6, "idle rec", 1, col);
      drawStr(W - 70, y + 6, cfg::idleTouchRecord() ? "on" : "off", 1, col);
    }
  }
  refresh();
}

void showDeviceInfo() {
  clearWhite();
  drawStr(16, 14, "device", 1, BLACK);
  hline(16, 32, W-32, BLACK);
  drawStr(18, 50, "firmware", 1, BLACK);
  drawStrFit(18, 68, 160, FIRMWARE_VERSION, 1, BLACK);
  drawStr(18, 94, "board", 1, BLACK);
  drawStrFit(18, 112, 160, "ESP32-S3 ePaper 1.54", 1, BLACK);
  char b[24]; snprintf(b, sizeof(b), "%d notes", (int)noteIndex.size());
  drawStr(18, 138, b, 1, BLACK);
  drawStr(18, 160, amarSoundIsEnabled() ? "sounds on" : "sounds off", 1, BLACK);
  drawStr(18, 178, rtcUtcIso().length() ? "rtc set" : "rtc not set", 1, BLACK);
  refresh();
}

void showResetConfirm() {
  clearWhite();
  drawKicker("factory reset", 18);
  drawStrC(100, 64,  "erase wifi & key?", 1, BLACK);
  drawStrC(100, 86,  "notes are kept", 1, BLACK);
  hline(20, 110, W - 40, BLACK);
  drawStrC(100, 134, "rec = erase", 1, BLACK);
  drawStrC(100, 156, "pwr = cancel", 1, BLACK);
  refresh();
}

void showResetDone() {
  clearWhite();
  drawCheckSmall(100, 70, BLACK);
  drawStrC(100, 110, "reset done", 1, BLACK);
  drawStrC(100, 132, "restarting", 1, BLACK);
  forceFullRefresh();
}

void showDeleteAllConfirm(int count, int cursor) {
  clearWhite();
  fillRect(0, 0, W, 26, BLACK);
  drawStrC(W/2, 9, "ERASE ALL", 1, WHITE);
  char label[20]; snprintf(label, sizeof(label), "%d notes", count);
  drawStrC(W/2, 40, label, 2, BLACK);

  const char* opts[2] = { "Device only", "Device + GitHub" };
  const int y0 = 76, step = 34, boxH = 28;
  for (int i = 0; i < 2; i++) {
    bool active = (i == cursor);
    int y = y0 + i * step;
    if (active) fillRoundRect(20, y, 160, boxH, 8, BLACK);
    else        strokeRoundRect(20, y, 160, boxH, 8, 1, BLACK);
    drawStrC(W/2, y + 8, opts[i], 1, active ? WHITE : BLACK);
  }

  hline(0, 179, W, BLACK);
  fillRect(0, 180, W, 20, WHITE);
  drawStr(8, 186, "select", 1, BLACK);
  const char* r = "hold=cancel";
  drawStr(W - 8 - textW(r, 1), 186, r, 1, BLACK);
  refresh();
}

void showDeleteAllDone(bool alsoVault) {
  clearWhite();
  drawCheckSmall(100, 70, BLACK);
  drawStrC(100, 110, "all erased", 1, BLACK);
  if (alsoVault) drawStrC(100, 132, "github on sync", 1, BLACK);
  forceFullRefresh();
}

void showUsbMsc() {
  clearWhite();
  drawHeader("USB Storage", nullptr);
  iconUsbDrive(100, 82);
  drawStrC(100, 126, "Connected", 1, BLACK);
  drawStrC(100, 146, "safely eject before", 1, BLACK);
  drawStrC(100, 162, "exiting", 1, BLACK);
  hline(0, 179, W, BLACK);
  fillRect(0, 180, W, 20, WHITE);
  drawStr(8, 186, "hold rec to exit", 1, BLACK);
  refresh();
}

// ─── Tamagotchi placeholder screen ────────────────────────────────────────────
//
// showTamagotchi() is a stub — the pet logic lives in its own module.
// This screen is shown when the menu tile is tapped and the Tamagotchi
// module has not yet been initialised for this session.
void showTamagotchi() {
  clearWhite();
  drawStr(16, 14, "tamagotchi", 1, BLACK);
  hline(16, 26, W-32, BLACK);
  iconCatFace(100, 88, BLACK);
  drawStrC(100, 118, "coming soon", 1, BLACK);
  refresh();
}

void redrawCurrentScreen() {
  switch (state) {
    case STATE_MENU:        showMenu(menuCursor);         break;
    case STATE_SETTINGS:    showSettings(settingsCursor); break;
    case STATE_NOTE_LIST:   showNoteList(listCursor);     break;
    case STATE_TAG_BROWSER: showTagBrowser(tagCursor);    break;
    case STATE_TAG_SELECT:  showTagSelect(tagCursor);     break;
    case STATE_NOTE_DETAIL: showNoteDetail(listCursor);   break;
    case STATE_USB_MSC:     showUsbMsc();                 break;
    case STATE_TAMAGOTCHI:  showTamagotchi();             break;
    default: break;
  }
}

void serviceDisplay() {
  if (!displayDirty()) return;
  if (displayBusy()) return;
  clearDisplayDirty();
  beginBufferDraw();
  redrawCurrentScreen();
  endBufferDraw();
  refreshAsyncFromBuffer();
}
