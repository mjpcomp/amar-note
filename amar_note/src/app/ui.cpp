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

// ─── Menu action tile icons (drawn small, ~18px radius budget) ───────────────

// Gear / cog icon  (8-tooth, ~17px outer radius)
void iconGear(int cx, int cy, uint8_t col) {
  strokeCircle(cx, cy, 9, 3, col);
  // 8 rectangular teeth around the outside
  const int teeth = 8;
  for (int i = 0; i < teeth; i++) {
    float ang = i * 3.14159f * 2.0f / teeth;
    int tx1 = cx + (int)(12 * cosf(ang - 0.22f) + 0.5f);
    int ty1 = cy + (int)(12 * sinf(ang - 0.22f) + 0.5f);
    int tx2 = cx + (int)(12 * cosf(ang + 0.22f) + 0.5f);
    int ty2 = cy + (int)(12 * sinf(ang + 0.22f) + 0.5f);
    int tx3 = cx + (int)(17 * cosf(ang + 0.22f) + 0.5f);
    int ty3 = cy + (int)(17 * sinf(ang + 0.22f) + 0.5f);
    int tx4 = cx + (int)(17 * cosf(ang - 0.22f) + 0.5f);
    int ty4 = cy + (int)(17 * sinf(ang - 0.22f) + 0.5f);
    fillTriangle(tx1, ty1, tx2, ty2, tx3, ty3, col);
    fillTriangle(tx1, ty1, tx3, ty3, tx4, ty4, col);
  }
  fillCircle(cx, cy, 7, col);
  fillCircle(cx, cy, 4, col == BLACK ? WHITE : BLACK);
}

// USB trident plug symbol
void iconUsbPlug(int cx, int cy, uint8_t col) {
  // Stem
  fillRect(cx-2, cy-2, 4, 14, col);
  // Connector head (rectangle)
  fillRect(cx-8, cy-14, 16, 10, col);
  fillRect(cx-6, cy-12, 12, 6, col == BLACK ? WHITE : BLACK);
  // Left branch
  thickLine(cx-2, cy-2, cx-10, cy-10, 2, col);
  fillRect(cx-12, cy-13, 4, 4, col);
  // Right branch
  thickLine(cx+2, cy-2, cx+10, cy-10, 2, col);
  strokeRect(cx+8, cy-14, 4, 5, 1, col);
  // Base plug prongs
  fillRect(cx-10, cy+10, 6, 3, col);
  fillRect(cx+4,  cy+10, 6, 3, col);
  fillRect(cx-10, cy+10, 20, 2, col);
}

// Cute cat face icon
void iconCatFace(int cx, int cy, uint8_t col) {
  // Head circle
  strokeCircle(cx, cy+2, 14, 2, col);
  // Left ear (filled triangle)
  fillTriangle(cx-14, cy-10, cx-6, cy-10, cx-12, cy-20, col);
  // Right ear
  fillTriangle(cx+14, cy-10, cx+6, cy-10, cx+12, cy-20, col);
  // Eyes
  fillCircle(cx-5, cy-1, 2, col);
  fillCircle(cx+5, cy-1, 2, col);
  // Nose
  fillTriangle(cx-2, cy+4, cx+2, cy+4, cx, cy+2, col);
  // Whiskers left
  hline(cx-14, cy+3, 8, col);
  hline(cx-14, cy+6, 8, col);
  // Whiskers right
  hline(cx+6, cy+3, 8, col);
  hline(cx+6, cy+6, 8, col);
}

// Small sync arrows for tile (fits in ~16px radius)
void iconSyncSmall(int cx, int cy, uint8_t col) {
  strokeCircle(cx, cy, 13, 3, col);
  // Top-right arrow
  fillRect(cx+4, cy-17, 7, 7, col == BLACK ? WHITE : BLACK);
  thickLine(cx+4, cy-12, cx+11, cy-12, 2, col);
  thickLine(cx+11, cy-12, cx+7, cy-16, 2, col);
  thickLine(cx+11, cy-12, cx+7, cy-8,  2, col);
  // Bottom-left arrow
  fillRect(cx-11, cy+10, 7, 7, col == BLACK ? WHITE : BLACK);
  thickLine(cx-11, cy+12, cx-4, cy+12, 2, col);
  thickLine(cx-4,  cy+12, cx-8, cy+8,  2, col);
  thickLine(cx-4,  cy+12, cx-8, cy+16, 2, col);
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

// ─── Recording screen ─────────────────────────────────────────────────────────
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

// ─── Menu layout ──────────────────────────────────────────────────────────────
//
// Screen is 200×200 px.  Header: y=0..11 ("menu" label) + hline at y=22.
// We have ~177px below the hline.
//
// Row A  — Notes (left tile) + Tags (right tile)  [unchanged]
//          y=28, h=48
//          Left  tile: x=10, w=88
//          Right tile: x=102, w=88
//
// Row B  — 2×2 action-icon tile grid
//          tileW=88, tileH=48, gapX=4, gapY=4
//          Top-left of grid: x=10, y=82
//
//          [0] Sync     x=10,  y=82   [1] Settings  x=102, y=82
//          [2] USB      x=10,  y=134  [3] Pet        x=102, y=134
//
//          Bottom edge: 134+48 = 182 px  (safe, 18px breathing room)
//
// Cursor mapping: 0=Notes 1=Tags 2=Sync 3=Settings 4=USB 5=Pet
//
static const int ACT_X[4]  = { 10, 102,  10, 102 };
static const int ACT_Y[4]  = { 82,  82, 134, 134 };
static const int ACT_W     = 88;
static const int ACT_H     = 48;

void showMenu(int cursor) {
  clearWhite();
  drawStr(16, 10, "menu", 1, BLACK);
  hline(16, 22, W-32, BLACK);

  // ── Row A: Notes (0) + Tags (1) side by side ──
  const int tileY = 28, tileH = 48;
  const int tileL_x = 10,  tileL_w = 88;
  const int tileR_x = 102, tileR_w = 88;

  // Notes tile
  {
    bool active = (cursor == 0);
    if (active) fillRoundRect(tileL_x, tileY, tileL_w, tileH, 10, BLACK);
    else        strokeRoundRect(tileL_x, tileY, tileL_w, tileH, 10, 1, BLACK);
    uint8_t col = active ? WHITE : BLACK;
    drawMinimalDocIcon(tileL_x + tileL_w/2, tileY + 18, col);
    drawStrInBox(tileL_x + 2, tileY + tileH - 14, tileL_w - 4, 12, "Notes", 1, col);
  }

  // Tags tile
  {
    bool active = (cursor == 1);
    if (active) fillRoundRect(tileR_x, tileY, tileR_w, tileH, 10, BLACK);
    else        strokeRoundRect(tileR_x, tileY, tileR_w, tileH, 10, 1, BLACK);
    uint8_t col = active ? WHITE : BLACK;
    drawMinimalTagIcon(tileR_x + tileR_w/2, tileY + 18, col);
    drawStrInBox(tileR_x + 2, tileY + tileH - 14, tileR_w - 4, 12, "Tags", 1, col);
  }

  // ── Row B: 2×2 action icon tile grid (indices 2–5) ──
  // Labels and icon draw calls per tile:
  //   2 = Sync      iconSyncSmall
  //   3 = Settings  iconGear
  //   4 = USB       iconUsbPlug
  //   5 = Pet       iconCatFace
  const char* actLabels[4] = { "Sync", "Settings", "USB", "Pet" };

  for (int i = 0; i < 4; i++) {
    int menuIdx = 2 + i;
    bool active = (cursor == menuIdx);
    int x = ACT_X[i], y = ACT_Y[i];
    if (active) fillRoundRect(x, y, ACT_W, ACT_H, 10, BLACK);
    else        strokeRoundRect(x, y, ACT_W, ACT_H, 10, 1, BLACK);
    uint8_t col = active ? WHITE : BLACK;
    int icx = x + ACT_W / 2;
    int icy = y + 18;
    switch (i) {
      case 0: iconSyncSmall(icx, icy, col); break;
      case 1: iconGear(icx, icy, col);      break;
      case 2: iconUsbPlug(icx, icy, col);   break;
      case 3: iconCatFace(icx, icy, col);   break;
    }
    drawStrInBox(x + 2, y + ACT_H - 14, ACT_W - 4, 12, actLabels[i], 1, col);
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
      drawStr(28, y + 6, "idle rec", 1, col);
      drawStr(W - 70, y + 6, cfg::idleTouchRecord() ? "on" : "off", 1, col);
    } else {
      drawStr(28, y + 6, "reset", 1, col);
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
  drawStrC(100, 105, "reset done", 1, BLACK);
  drawStrC(100, 126, "restarting", 1, BLACK);
  refresh();
}

void showDeleteAllConfirm(int count, int cursor) {
  clearWhite();
  drawKicker("erase all", 18);
  char cb[24]; snprintf(cb, sizeof(cb), "%d notes", count);
  drawStrC(100, 50, cb, 1, BLACK);
  bool a0 = (cursor == 0), a1 = (cursor == 1);
  if (a0) fillRoundRect(20, 76, 160, 28, 8, BLACK);
  else    strokeRoundRect(20, 76, 160, 28, 8, 1, BLACK);
  drawStrInBox(20, 76, 160, 28, "SD only", 1, a0 ? WHITE : BLACK);
  if (a1) fillRoundRect(20, 110, 160, 28, 8, BLACK);
  else    strokeRoundRect(20, 110, 160, 28, 8, 1, BLACK);
  drawStrInBox(20, 110, 160, 28, "SD + vault", 1, a1 ? WHITE : BLACK);
  drawStrC(100, 158, "tap to select", 1, BLACK);
  drawStrC(100, 176, "rec = confirm", 1, BLACK);
  refresh();
}

void showDeleteAllDone(bool alsoVault) {
  clearWhite();
  drawCheckSmall(100, 70, BLACK);
  drawStrC(100, 105, "erased", 1, BLACK);
  if (alsoVault) drawStrC(100, 126, "+ vault", 1, BLACK);
  refresh();
}

// ─── Pet screen (stub — Tamagotchi feature coming soon) ───────────────────────
void showPet() {
  clearWhite();
  drawStr(16, 10, "pet", 1, BLACK);
  hline(16, 22, W-32, BLACK);
  iconCatFace(100, 88, BLACK);
  drawStrC(100, 122, "coming soon", 1, BLACK);
  drawStrC(100, 145, "pwr = back", 1, BLACK);
  refresh();
}
