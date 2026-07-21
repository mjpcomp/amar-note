#include "Arduino.h"
#include "../../config.h"
#include "../../globals.h"
#include "ui.h"
#include "record.h"
#include "network.h"
#include "config_store.h"
#include "../../logo_bitmap.h"

// ============================================================
// ui.cpp — Amar Note display and state machine
//
// States:
//   SLEEP    — e-paper sleep screen (Amar logo)
//   IDLE     — home screen, ready to record
//   RECORD   — recording in progress + VU meter
//   SAVING   — brief "Saving…" splash
//   NOTES    — scrollable note list
//   DETAIL   — single note view
//   SETTINGS — settings menu
// ============================================================

#include "epd1in54_V2.h"   // Waveshare 1.54" e-paper driver
#include "epdpaint.h"

static Epd epd;
static Paint paint;
static uint8_t framebuf[200 * 200 / 8];

enum UiState {
    UI_SLEEP, UI_IDLE, UI_RECORD, UI_SAVING,
    UI_NOTES, UI_DETAIL, UI_SETTINGS
};

static UiState state       = UI_IDLE;
static UiState prevState   = UI_IDLE;
static bool    needRedraw  = true;
static uint32_t vuLastMs   = 0;
static uint8_t  vuLevel    = 0;
static int      noteScroll = 0;
static int      noteSel    = 0;

// ---- Display helpers ----

static void displayInit_() {
    epd.Init();
    paint.SetRotate(ROTATE_0);
    paint.SetWidth(200);
    paint.SetHeight(200);
    paint.SetImage(framebuf);
}

static void partialFlush() {
    epd.Init_Partial();
    epd.DisplayPartial(framebuf);
}

static void fullFlush() {
    epd.Init();
    epd.DisplayFrame(framebuf);
}

static void requestRedraw() { needRedraw = true; }

// ---- State renderers ----

static void drawSleep() {
    paint.Clear(UNCOLORED);
    // Draw Amar logo bitmap centred
    const int bmpW = 100, bmpH = 100;
    int x0 = (200 - bmpW) / 2;
    int y0 = (200 - bmpH) / 2;
    paint.DrawBitmap(x0, y0, AMAR_LOGO_BITMAP, bmpW, bmpH, COLORED);
    fullFlush();
}

static void drawIdle() {
    paint.Clear(UNCOLORED);
    paint.DrawStringAt(10, 10,  "Amar Note",       &Font20, COLORED);
    paint.DrawStringAt(10, 40,  "Tap REC to record", &Font12, COLORED);
    if (networkIsConnected())
        paint.DrawStringAt(10, 170, "WiFi OK", &Font12, COLORED);
    else
        paint.DrawStringAt(10, 170, "No WiFi", &Font12, COLORED);
    partialFlush();
}

static void drawRecord() {
    paint.Clear(UNCOLORED);
    paint.DrawStringAt(10, 10, "Recording…",  &Font20, COLORED);
    paint.DrawStringAt(10, 40, "Tap REC to stop", &Font12, COLORED);
    // VU bar
    int barW = (int)(vuLevel * 180 / 255);
    paint.DrawFilledRectangle(10, 100, 10 + barW, 120, COLORED);
    partialFlush();
}

static void drawSaving() {
    paint.Clear(UNCOLORED);
    paint.DrawStringAt(10, 80, "Saving note…", &Font16, COLORED);
    partialFlush();
}

static void drawSettings() {
    paint.Clear(UNCOLORED);
    paint.DrawStringAt(10, 10, "Settings",     &Font20, COLORED);
    paint.DrawStringAt(10, 40, "[Erase All]",  &Font12, COLORED);
    paint.DrawStringAt(10, 60, "Hold PWR=back",  &Font12, COLORED);
    partialFlush();
}

// ---- Public API ----

void displayInit() { displayInit_(); }

void uiInit() {
    state = UI_IDLE;
    needRedraw = true;
}

void uiService() {
    // VU meter update during recording
    if (state == UI_RECORD && millis() - vuLastMs > 100) {
        vuLastMs = millis();
        // TODO: read actual level from record.cpp
        vuLevel = (uint8_t)((millis() / 100) & 0xFF);
        requestRedraw();
    }

    if (!needRedraw) return;
    needRedraw = false;

    switch (state) {
        case UI_SLEEP:    drawSleep();    break;
        case UI_IDLE:     drawIdle();     break;
        case UI_RECORD:   drawRecord();   break;
        case UI_SAVING:   drawSaving();   break;
        case UI_SETTINGS: drawSettings(); break;
        default: break;
    }
}

// ---- Button event handlers ----

void uiOnRecTap() {
    if (state == UI_RECORD) {
        recordStop();
        state = UI_SAVING;
        requestRedraw();
    } else if (state == UI_IDLE || state == UI_NOTES) {
        recordStart();
        state = UI_RECORD;
        requestRedraw();
    }
}

void uiOnRecLong() {
    // Select / open in list
    if (state == UI_NOTES) {
        state = UI_DETAIL;
        requestRedraw();
    }
}

void uiOnPwrTap() {
    // Scroll / navigate
    if (state == UI_IDLE) {
        state = UI_NOTES;
        requestRedraw();
    } else if (state == UI_NOTES) {
        noteScroll++;
        requestRedraw();
    } else if (state == UI_SETTINGS) {
        noteSel = (noteSel + 1) % 2;
        requestRedraw();
    }
}

void uiOnPwrLong() {
    // Back / up / sleep
    if (state == UI_DETAIL || state == UI_NOTES) {
        state = UI_IDLE;
        requestRedraw();
    } else if (state == UI_SETTINGS) {
        state = UI_IDLE;
        requestRedraw();
    } else if (state == UI_IDLE) {
        drawSleep();
        state = UI_SLEEP;
        epd.Sleep();
    }
}

void uiOnRecordStart() { /* handled by uiOnRecTap */ }
void uiOnRecordStop()  { state = UI_IDLE; requestRedraw(); }
