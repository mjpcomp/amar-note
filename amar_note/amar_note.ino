#include "Arduino.h"
#include "SD_MMC.h"
#include "WiFi.h"
#include "HTTPClient.h"
#include "WiFiClientSecure.h"
#include <WebServer.h>
#include <vector>
#include "driver/i2c_master.h"
#include "esp_heap_caps.h"

extern "C" {
#include "config.h"
#include "src/i2c_bsp/i2c_bsp.h"
#include "src/audio/audio_bsp.h"
}

#include "src/power/board_power_bsp.h"
#include "src/display/epaper_driver_bsp.h"
#include "logo_bitmap.h"
#include "secrets.h"
#include "sounds.h"

#include <Adafruit_GFX.h>
#include <pgmspace.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>

#include "types.h"
#include "globals.h"
#include "src/app/draw.h"
#include "src/app/battery.h"
#include "src/app/rtc.h"
#include "src/app/notes.h"
#include "src/app/ui.h"
#include "src/app/buttons.h"
#include "src/app/network.h"
#include "src/app/sleep.h"
#include "src/app/record.h"
#include "src/app/config_store.h"
#include "src/app/obsidian.h"
#include "src/app/usb_msc.h"
#include "src/app/touch.h"

// All pin, timing, path and threshold constants live in config.h.

// ─── Content arrays ──────────────────────────────────────────────
const char* DEFAULT_TAGS[]    = { "Note", "Work", "Idea", "Buy", "Private" };
const char* MENU_ITEMS[]     = { "Notes", "Tags", "Sync", "Settings", "USB", "Pet" };
const char* SETTINGS_ITEMS[] = { "Sounds", "Transfer", "Device", "Erase All", "idle rec", "Reset" };

// ─── Global variable definitions ───────────────────────────────────────────
board_power_bsp_t      board(EPD_PWR_PIN, Audio_PWR_PIN, VBAT_PWR_PIN);
epaper_driver_display* display = nullptr;

std::vector<NoteEntry> noteIndex;

AppState state          = STATE_IDLE;
int      listCursor     = 0;
int      tagCursor      = 2;
int      menuCursor     = 0;
int      settingsCursor = 0;
int      eraseAllCursor = 0;
int      activeFilter   = -1;
int      lastRecNum     = -1;

uint32_t lastActivityMs      = 0;
bool     wokeFromUltraSleep  = false;
bool     wakeToMenuRequested = false;
bool     wakeToRecRequested  = false;

uint32_t tickerLastMs = 0;
int      tickerOffset = 0;
int      tickerCursor = -1;

WebServer transferServer(80);
bool      transferServerActive = false;
String    transferUrl          = "";
DNSServer dnsServer;
bool      captivePortalActive  = false;

bool timeReady    = false;
bool audioPlaying = false;
bool stopPlayback = false;

int detailScrollPage = 0;
int detailTotalLines = 0;

uint32_t lastBatCheckMs    = 0;
bool     batLowWarned      = false;
bool     batWarnActive     = false;
uint32_t batWarnShowUntilMs = 0;

char tags[20][32];
int  tagCount = 0;

// ─── Power latch ───────────────────────────────────────────────────────────
void keepBatteryPowerOn() {
  pinMode(PWR_HOLD_PIN, OUTPUT);
  digitalWrite(PWR_HOLD_PIN, HIGH);
}

// ─── Flow functions ─────────────────────────────────────────────────────────
void startRecordFlow() {
  state = STATE_RECORDING;
  showRecording();

  amarSoundSetEnabled(false);
  g_stopRecording = false;
  bool recOk = record();
  amarSoundSetEnabled(true);

  if (!recOk) {
    showError("REC FAIL");
    delay(1600);
    state = STATE_IDLE;
    showIdle();
    return;
  }

  soundSaved();

  state = STATE_SAVED;
  showSaved(lastRecNum);
  delay(900);

  tagCursor = min(2, max(tagCount - 1, 0));
  state = STATE_TAG_SELECT;
  showTagSelect(tagCursor);
}

void startSyncFlow() {
  if (!cfg::hasWifi()) {
    showError("NO WIFI CFG");
    delay(1800);
    showIdle();
    return;
  }

  const int MAX_TRIES = 20;
  showWifiConnecting(0, MAX_TRIES);

  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  String ssid = cfg::wifiSsid(), pass = cfg::wifiPass();
  WiFi.begin(ssid.c_str(), pass.c_str());
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < MAX_TRIES) {
    delay(500); tries++;
    showWifiConnecting(tries, MAX_TRIES);
  }

  if (WiFi.status() == WL_CONNECTED) {
    syncTimeFromNTP(6000);
    transcribeAll();
    loadIndex();
    obsidianSyncAll();
    WiFi.disconnect(true);
    showDone();
    soundSuccess();
    delay(1600);
  } else {
    showError("NO WIFI");
    delay(1800);
  }

  if (wakeToMenuRequested) {
    menuCursor = 0;
    state = STATE_MENU;
    showMenu(menuCursor);
  } else {
    showIdle();
  }
}

void startTransferMode() {
  state = STATE_TRANSFER;
  showTransferConnecting();

  if (!cfg::hasWifi()) {
    WiFi.persistent(false);
    WiFi.mode(WIFI_AP);
    bool apOk = WiFi.softAP(SETUP_SSID);
    delay(200);
    IPAddress apIP = WiFi.softAPIP();
    Serial.printf("[Transfer] SoftAP '%s' start=%d ip=%s\n",
                  SETUP_SSID, apOk, apIP.toString().c_str());

    dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
    dnsServer.start(53, "*", apIP);
    captivePortalActive = true;

    setupTransferServer();
    transferServer.begin();
    transferServerActive = true;
    transferUrl = apIP.toString();
    Serial.println("[Transfer] HTTP server started on :80");
    showTransferMode(transferUrl.c_str());
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  String ssid = cfg::wifiSsid(), pass = cfg::wifiPass();
  WiFi.begin(ssid.c_str(), pass.c_str());

  const int MAX_TRIES = 24;
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < MAX_TRIES) {
    delay(500); tries++;
  }

  if (WiFi.status() != WL_CONNECTED) {
    showError("NO WIFI");
    delay(1600);
    state = STATE_SETTINGS;
    showSettings(settingsCursor);
    return;
  }

  syncTimeFromNTP(8000);
  setupTransferServer();
  transferServer.begin();
  transferServerActive = true;

  IPAddress ip = WiFi.localIP();
  transferUrl = ip.toString();
  showTransferMode(transferUrl.c_str());
}

// ─── Setup ──────────────────────────────────────────────────────────────
void setup() {
  keepBatteryPowerOn();

  Serial.begin(115200);
  delay(300);

  cfg::begin();

  pinMode(BTN_REC, INPUT_PULLUP);
  pinMode(BTN_PWR, INPUT_PULLUP);

  board.VBAT_POWER_ON();
  board.POWEER_EPD_ON();
  board.POWEER_Audio_ON();
  delay(200);

  custom_lcd_spi_t dispCfg = {};
  dispCfg.cs       = EPD_CS_PIN;
  dispCfg.dc       = EPD_DC_PIN;
  dispCfg.rst      = EPD_RST_PIN;
  dispCfg.busy     = EPD_BUSY_PIN;
  dispCfg.mosi     = EPD_MOSI_PIN;
  dispCfg.scl      = EPD_SCK_PIN;
  dispCfg.spi_host = EPD_SPI_NUM;
  dispCfg.buffer_len = (200*200)/8;

  display = new epaper_driver_display(200, 200, dispCfg);
  display->EPD_Init();
  display->EPD_Clear();
  display->EPD_DisplayPartBaseImage();
  display->EPD_Init_Partial();

  usb_msc_check_boot_flag();

  Serial.println("\n=== Amar Note " FIRMWARE_VERSION " ===");

  wokeFromUltraSleep  = (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT1);
  delay(50);

  wakeToMenuRequested = (wokeFromUltraSleep && digitalRead(BTN_PWR) == LOW);
  wakeToRecRequested  = (wokeFromUltraSleep && digitalRead(BTN_REC) == LOW);

  resetActivity();

  i2c_master_Init();
  delay(50);

  touchInit();

  audio_bsp_init();
  audio_play_init();

  SD_MMC.setPins(SD_CLK, SD_CMD, SD_D0);
  if (!SD_MMC.begin("/sdcard", true)) {
    showError("SD ERR");
    while (true) delay(1000);
  }
  if (!SD_MMC.exists(NOTES_DIR)) SD_MMC.mkdir(NOTES_DIR);
  loadTags();
  loadIndex();
  Serial.printf("[SD] %d notes\n", (int)noteIndex.size());

  if (wakeToMenuRequested) {
    menuCursor = 0;
    state = STATE_MENU;
    showMenu(menuCursor);
  } else if (wakeToRecRequested) {
    startRecordFlow();
  } else {
    showIdle();
  }
}

// ─── Serial provisioning ───────────────────────────────────────────────────
void handleSerialConfig() {
  static String line;
  static String pendingSsid;
  while (Serial.available()) {
    char c = Serial.read();
    if (c != '\n' && c != '\r') {
      line += c;
      if (line.length() > 256) line = "";
      continue;
    }
    line.trim();
    if (line.length() == 0) { line = ""; continue; }

    if (line.startsWith("SSID=")) {
      pendingSsid = line.substring(5);
      Serial.printf("[cfg] ssid buffered ('%s'); now send PASS=<password>\n", pendingSsid.c_str());
    } else if (line.startsWith("PASS=")) {
      if (pendingSsid.length() > 0) {
        cfg::setWifi(pendingSsid, line.substring(5));
        Serial.printf("[cfg] wifi saved for ssid '%s'\n", pendingSsid.c_str());
        pendingSsid = "";
      } else {
        Serial.println("[cfg] send SSID=<network> first");
      }
    } else if (line.startsWith("KEY=")) {
      cfg::setOpenAiKey(line.substring(4));
      Serial.println("[cfg] openai key saved");
    } else if (line.startsWith("GHTOKEN=")) {
      cfg::setGithubToken(line.substring(8));
      Serial.println("[cfg] github token saved");
    } else if (line.startsWith("GHREPO=")) {
      bool ok = cfg::setGithubRepo(line.substring(7));
      Serial.printf("[cfg] github repo %s\n", ok ? "saved" : "rejected (need owner/name)");
    } else if (line.startsWith("GHBRANCH=")) {
      cfg::setGithubBranch(line.substring(9));
      Serial.println("[cfg] github branch saved");
    } else if (line.startsWith("GHDIR=")) {
      cfg::setGithubDir(line.substring(6));
      Serial.println("[cfg] github dir saved");
    } else if (line == "GHON")  { cfg::setGithubEnabled(true);  Serial.println("[cfg] github sync ON");
    } else if (line == "GHOFF") { cfg::setGithubEnabled(false); Serial.println("[cfg] github sync OFF");
    } else if (line == "SHOW") {
      Serial.printf("[cfg] wifi=%s  openai_key=%s\n",
        cfg::hasWifi() ? cfg::wifiSsid().c_str() : "(none)",
        cfg::hasOpenAiKey() ? "set" : "(none)");
      Serial.printf("[cfg] github=%s branch=%s dir=%s token=%s enabled=%d ai=%d ready=%d\n",
        cfg::githubRepo().length() ? cfg::githubRepo().c_str() : "(none)",
        cfg::githubBranch().c_str(), cfg::githubDir().c_str(),
        cfg::githubToken().length() ? "set" : "(none)",
        cfg::githubEnabled(), cfg::githubAiEnrich(), cfg::hasGithub());
    } else if (line == "RESET") {
      cfg::factoryReset();
      Serial.println("[cfg] factory reset done");
    } else {
      Serial.println("[cfg] cmds: SSID= PASS= KEY= GHREPO= GHBRANCH= GHDIR= GHTOKEN= GHON GHOFF SHOW RESET");
    }
    line = "";
  }
}

// ─── Touch dispatch helper ────────────────────────────────────────────────────
//
// Hit-zone geometry mirrors showMenu() layout constants in ui.cpp exactly.
//
// Menu cursor mapping:
//   0 = Notes      tile x=10,  y=28,  w=88, h=48
//   1 = Tags       tile x=102, y=28,  w=88, h=48
//   2 = Sync       tile x=10,  y=82,  w=88, h=48
//   3 = Settings   tile x=102, y=82,  w=88, h=48
//   4 = USB        tile x=10,  y=134, w=88, h=48
//   5 = Pet        tile x=102, y=134, w=88, h=48
//
static bool handleTouch() {
  if (state == STATE_RECORDING || state == STATE_USB_MSC) return false;

  uint16_t tx, ty;
  if (!touchRead(&tx, &ty)) return false;

  resetActivity();

  if (state == STATE_IDLE) {
    if (cfg::idleTouchRecord()) {
      startRecordFlow();
      return true;
    }
    return false;
  }

  // ── MENU ────────────────────────────────────────────────────────────────
  if (state == STATE_MENU) {
    // Row A
    if (touchHitTest(tx, ty, 10, 28, 88, 48)) {          // Notes
      soundSelect();
      activeFilter = -1; listCursor = 0;
      state = STATE_NOTE_LIST;
      showNoteList(listCursor);
      return true;
    }
    if (touchHitTest(tx, ty, 102, 28, 88, 48)) {         // Tags
      soundSelect();
      tagCursor = 0;
      state = STATE_TAG_BROWSER;
      showTagBrowser(tagCursor);
      return true;
    }
    // Row B — 2x2 grid
    if (touchHitTest(tx, ty, 10,  82, 88, 48)) {         // Sync
      soundSelect();
      startSyncFlow();
      return true;
    }
    if (touchHitTest(tx, ty, 102, 82, 88, 48)) {         // Settings
      soundSelect();
      settingsCursor = 0;
      state = STATE_SETTINGS;
      showSettings(settingsCursor);
      return true;
    }
    if (touchHitTest(tx, ty, 10,  134, 88, 48)) {        // USB
      soundSelect();
      enterMscMode();
      return true;
    }
    if (touchHitTest(tx, ty, 102, 134, 88, 48)) {        // Pet
      soundSelect();
      state = STATE_PET;
      showPet();
      return true;
    }
    return false;
  }

  // ── SETTINGS ─────────────────────────────────────────────────────────
  if (state == STATE_SETTINGS) {
    const int rowY[6] = { 38, 64, 90, 116, 142, 168 };
    for (int i = 0; i < SETTINGS_COUNT; i++) {
      if (touchHitTest(tx, ty, 16, rowY[i], 168, 26)) {
        soundSelect();
        settingsCursor = i;
        if (i == 0) {
          amarSoundSetEnabled(!amarSoundIsEnabled());
          showSettings(settingsCursor);
        } else if (i == 1) {
          startTransferMode();
        } else if (i == 2) {
          state = STATE_DEVICE_INFO;
          showDeviceInfo();
        } else if (i == 3) {
          eraseAllCursor = 0;
          state = STATE_DELETE_ALL_CONFIRM;
          showDeleteAllConfirm((int)noteIndex.size(), eraseAllCursor);
        } else if (i == 4) {
          cfg::setIdleTouchRecord(!cfg::idleTouchRecord());
          showSettings(settingsCursor);
        } else {
          state = STATE_RESET_CONFIRM;
          showResetConfirm();
        }
        return true;
      }
    }
    if (ty >= 180) {
      soundBack();
      state = STATE_MENU;
      showMenu(menuCursor);
      return true;
    }
    return false;
  }

  // ── TAG SELECT (post-record) ──────────────────────────────────────────
  if (state == STATE_TAG_SELECT) {
    if (tagCount > 0) {
      const int x = 36, w = 128, h = 21, gap = 7, y0 = 40;
      for (int i = 0; i < tagCount; i++) {
        int y = y0 + i * (h + gap);
        if (touchHitTest(tx, ty, x, y, w, h)) {
          soundSelect();
          tagCursor = i;
          saveTag(lastRecNum, tags[constrain(tagCursor, 0, tagCount - 1)]);
          enterUltraSleep();
          return true;
        }
      }
    }
    return false;
  }

  // ── TAG BROWSER ─────────────────────────────────────────────────────
  if (state == STATE_TAG_BROWSER) {
    if (touchHitTest(tx, ty, 28, 56, 144, 54)) {
      soundSelect();
      activeFilter = tagCursor; listCursor = 0;
      state = STATE_NOTE_LIST;
      showNoteList(listCursor);
      return true;
    }
    if (ty >= 180) {
      soundBack();
      state = STATE_MENU;
      showMenu(menuCursor);
      return true;
    }
    if (tagCount > 0) {
      soundNext();
      tagCursor = (tagCursor + 1) % tagCount;
      requestRedraw();
      return true;
    }
    return false;
  }

  // ── NOTE LIST ────────────────────────────────────────────────────────
  if (state == STATE_NOTE_LIST) {
    int count = filteredCount();
    if (ty >= 180) {
      soundBack();
      state = STATE_MENU;
      showMenu(menuCursor);
      return true;
    }
    if (count > 0) {
      const int pageSize = 3;
      int pageStart = (listCursor / pageSize) * pageSize;
      const int y0 = 43, step = 47;
      for (int row = 0; row < pageSize; row++) {
        int vis = pageStart + row;
        if (vis >= count) break;
        if (touchHitTest(tx, ty, 16, y0 + row * step, 168, 39)) {
          soundSelect();
          listCursor = vis;
          detailScrollPage = 0;
          state = STATE_NOTE_DETAIL;
          showNoteDetail(listCursor);
          return true;
        }
      }
    }
    return false;
  }

  // ── NOTE DETAIL ─────────────────────────────────────────────────────
  if (state == STATE_NOTE_DETAIL) {
    if (ty >= 180) {
      soundBack();
      detailScrollPage = 0;
      state = STATE_NOTE_LIST;
      showNoteList(listCursor);
      return true;
    }
    if (ty < 140) {
      int idx = noteAtFilteredIndex(listCursor);
      if (idx >= 0) {
        char wavPath[64];
        snprintf(wavPath, sizeof(wavPath), "%s/note_%03d.wav", NOTES_DIR, noteIndex[idx].num);
        showPlaybackOverlay();
        playWavFile(wavPath);
        showNoteDetail(listCursor);
      }
      return true;
    }
    {
      soundNext();
      const int linesPerPage = 7;
      int totalPages = (detailTotalLines + linesPerPage - 1) / linesPerPage;
      if (detailScrollPage + 1 < totalPages) {
        detailScrollPage++;
      } else {
        detailScrollPage = 0;
        int count = filteredCount();
        if (count > 0) listCursor = (listCursor + 1) % count;
      }
      requestRedraw();
      return true;
    }
  }

  // ── DELETE CONFIRM ───────────────────────────────────────────────────
  if (state == STATE_DELETE_CONFIRM) {
    if (tx < 100) {
      int idx = noteAtFilteredIndex(listCursor);
      if (idx >= 0) { deleteNote(noteIndex[idx].num); soundDelete(); }
      detailScrollPage = 0;
      listCursor = constrain(listCursor, 0, max(filteredCount() - 1, 0));
      state = STATE_NOTE_LIST;
      showNoteList(listCursor);
    } else {
      soundBack();
      state = STATE_NOTE_DETAIL;
      showNoteDetail(listCursor);
    }
    return true;
  }

  // ── RESET CONFIRM ───────────────────────────────────────────────────
  if (state == STATE_RESET_CONFIRM) {
    if (tx < 100) {
      cfg::factoryReset();
      soundDelete();
      showResetDone();
      delay(1400);
      ESP.restart();
    } else {
      soundBack();
      state = STATE_SETTINGS;
      showSettings(settingsCursor);
    }
    return true;
  }

  // ── DELETE ALL CONFIRM ───────────────────────────────────────────────
  if (state == STATE_DELETE_ALL_CONFIRM) {
    if (ty >= 180) {
      soundBack();
      state = STATE_SETTINGS;
      showSettings(settingsCursor);
      return true;
    }
    if (touchHitTest(tx, ty, 20, 76, 160, 28)) {
      eraseAllCursor = 0;
      showDeleteAllConfirm((int)noteIndex.size(), eraseAllCursor);
      return true;
    }
    if (touchHitTest(tx, ty, 20, 110, 160, 28)) {
      eraseAllCursor = 1;
      showDeleteAllConfirm((int)noteIndex.size(), eraseAllCursor);
      return true;
    }
    if (touchHitTest(tx, ty, 20, 76 + eraseAllCursor * 34, 160, 28)) {
      bool alsoVault = (eraseAllCursor == 1);
      deleteAllNotes(alsoVault);
      activeFilter = -1; listCursor = 0;
      soundDelete();
      showDeleteAllDone(alsoVault);
      delay(1300);
      menuCursor = 0;
      state = STATE_MENU;
      showMenu(menuCursor);
      return true;
    }
    return false;
  }

  // ── TRANSFER ────────────────────────────────────────────────────────────
  if (state == STATE_TRANSFER) {
    soundBack();
    stopTransferMode();
    state = STATE_SETTINGS;
    showSettings(settingsCursor);
    return true;
  }

  // ── DEVICE INFO ─────────────────────────────────────────────────────────
  if (state == STATE_DEVICE_INFO) {
    soundBack();
    state = STATE_SETTINGS;
    showSettings(settingsCursor);
    return true;
  }

  // ── PET ────────────────────────────────────────────────────────────────────
  if (state == STATE_PET) {
    soundBack();
    state = STATE_MENU;
    showMenu(menuCursor);
    return true;
  }

  return false;
}

// ─── Main loop ─────────────────────────────────────────────────────────────
void loop() {
  handleSerialConfig();

  if (transferServerActive) {
    transferServer.handleClient();
    if (captivePortalActive) dnsServer.processNextRequest();
  }

  if (handleTouch()) return;

  ButtonEvent rec = pollButton(BTN_REC, BTN_REC_LONG_MS);
  ButtonEvent pwr = pollButton(BTN_PWR, BTN_PWR_LONG_MS);

  // ─ Battery monitor ─
  if (millis() - lastBatCheckMs > BAT_CHECK_INTERVAL_MS) {
    lastBatCheckMs = millis();
    int pct = readBatteryPercent();
    if (pct >= 0 && pct <= BAT_LOW_PCT && !batLowWarned) {
      batLowWarned  = true;
      batWarnActive = true;
      batWarnShowUntilMs = millis() + 4000;
      showBatteryLow(pct);
      return;
    }
    if (pct > BAT_LOW_PCT) batLowWarned = false;
  }
  if (batWarnActive) {
    if (millis() >= batWarnShowUntilMs) {
      batWarnActive = false;
      if      (state == STATE_IDLE)     showIdle();
      else if (state == STATE_MENU)     showMenu(menuCursor);
      else if (state == STATE_SETTINGS) showSettings(settingsCursor);
    }
    return;
  }

  // ─ Auto-sleep ─
  if (state != STATE_RECORDING && state != STATE_TRANSFER &&
      state != STATE_USB_MSC   && state != STATE_TAG_SELECT &&
      millis() - lastActivityMs > SLEEP_TIMEOUT_MS) {
    enterUltraSleep();
    return;
  }

  // ─ Ticker redraw ─
  if (state == STATE_NOTE_LIST) {
    if (millis() - tickerLastMs > TICKER_STEP_MS) {
      tickerLastMs = millis();
      tickerOffset++;
      showNoteList(listCursor);
    }
    return;
  }

  // ─ STATE_IDLE ─
  if (state == STATE_IDLE) {
    if (rec == EV_SINGLE || rec == EV_LONG) {
      resetActivity();
      startRecordFlow();
    } else if (pwr == EV_SINGLE || pwr == EV_LONG) {
      resetActivity();
      menuCursor = 0;
      state = STATE_MENU;
      showMenu(menuCursor);
    }
    return;
  }

  // ─ STATE_MENU ─
  if (state == STATE_MENU) {
    if (rec == EV_SINGLE) {
      resetActivity();
      soundSelect();
      if      (menuCursor == 0) { activeFilter = -1; listCursor = 0; state = STATE_NOTE_LIST; showNoteList(listCursor); }
      else if (menuCursor == 1) { tagCursor = 0; state = STATE_TAG_BROWSER; showTagBrowser(tagCursor); }
      else if (menuCursor == 2) { startSyncFlow(); }
      else if (menuCursor == 3) { settingsCursor = 0; state = STATE_SETTINGS; showSettings(settingsCursor); }
      else if (menuCursor == 4) { enterMscMode(); }
      else if (menuCursor == 5) { state = STATE_PET; showPet(); }
    } else if (rec == EV_LONG) {
      resetActivity();
      state = STATE_IDLE;
      showIdle();
    } else if (pwr == EV_SINGLE) {
      resetActivity();
      soundNext();
      menuCursor = (menuCursor + 1) % MENU_COUNT;
      showMenu(menuCursor);
    } else if (pwr == EV_LONG) {
      resetActivity();
      state = STATE_IDLE;
      showIdle();
    }
    return;
  }

  // ─ STATE_SETTINGS ─
  if (state == STATE_SETTINGS) {
    if (rec == EV_SINGLE) {
      resetActivity();
      soundSelect();
      if (settingsCursor == 0) {
        amarSoundSetEnabled(!amarSoundIsEnabled());
        showSettings(settingsCursor);
      } else if (settingsCursor == 1) {
        startTransferMode();
      } else if (settingsCursor == 2) {
        state = STATE_DEVICE_INFO;
        showDeviceInfo();
      } else if (settingsCursor == 3) {
        eraseAllCursor = 0;
        state = STATE_DELETE_ALL_CONFIRM;
        showDeleteAllConfirm((int)noteIndex.size(), eraseAllCursor);
      } else if (settingsCursor == 4) {
        cfg::setIdleTouchRecord(!cfg::idleTouchRecord());
        showSettings(settingsCursor);
      } else {
        state = STATE_RESET_CONFIRM;
        showResetConfirm();
      }
    } else if (pwr == EV_SINGLE) {
      resetActivity();
      soundNext();
      settingsCursor = (settingsCursor + 1) % SETTINGS_COUNT;
      showSettings(settingsCursor);
    } else if (pwr == EV_LONG || rec == EV_LONG) {
      resetActivity();
      soundBack();
      state = STATE_MENU;
      showMenu(menuCursor);
    }
    return;
  }

  // ─ STATE_TAG_SELECT ─
  if (state == STATE_TAG_SELECT) {
    if (rec == EV_SINGLE) {
      resetActivity();
      soundSelect();
      saveTag(lastRecNum, tags[constrain(tagCursor, 0, tagCount - 1)]);
      enterUltraSleep();
    } else if (pwr == EV_SINGLE) {
      resetActivity();
      soundNext();
      tagCursor = (tagCursor + 1) % max(tagCount, 1);
      showTagSelect(tagCursor);
    }
    return;
  }

  // ─ STATE_TAG_BROWSER ─
  if (state == STATE_TAG_BROWSER) {
    if (rec == EV_SINGLE) {
      resetActivity();
      soundSelect();
      activeFilter = tagCursor; listCursor = 0;
      state = STATE_NOTE_LIST;
      showNoteList(listCursor);
    } else if (pwr == EV_SINGLE) {
      resetActivity();
      soundNext();
      tagCursor = (tagCursor + 1) % max(tagCount, 1);
      showTagBrowser(tagCursor);
    } else if (pwr == EV_LONG || rec == EV_LONG) {
      resetActivity();
      soundBack();
      state = STATE_MENU;
      showMenu(menuCursor);
    }
    return;
  }

  // ─ STATE_NOTE_LIST (handled by ticker, button fallthrough) ─
  if (state == STATE_NOTE_LIST) {
    if (pwr == EV_LONG || rec == EV_LONG) {
      resetActivity();
      soundBack();
      state = STATE_MENU;
      showMenu(menuCursor);
    } else if (pwr == EV_SINGLE) {
      resetActivity();
      soundNext();
      int count = filteredCount();
      if (count > 0) { listCursor = (listCursor + 1) % count; tickerOffset = 0; tickerLastMs = millis(); }
      showNoteList(listCursor);
    } else if (rec == EV_SINGLE) {
      resetActivity();
      soundSelect();
      detailScrollPage = 0;
      state = STATE_NOTE_DETAIL;
      showNoteDetail(listCursor);
    }
    return;
  }

  // ─ STATE_NOTE_DETAIL ─
  if (state == STATE_NOTE_DETAIL) {
    if (pwr == EV_LONG || rec == EV_LONG) {
      resetActivity();
      soundBack();
      state = STATE_NOTE_LIST;
      showNoteList(listCursor);
    } else if (pwr == EV_SINGLE) {
      resetActivity();
      soundNext();
      const int linesPerPage = 7;
      int totalPages = (detailTotalLines + linesPerPage - 1) / linesPerPage;
      if (detailScrollPage + 1 < totalPages) {
        detailScrollPage++;
        showNoteDetail(listCursor);
      } else {
        detailScrollPage = 0;
        int count = filteredCount();
        if (count > 0) { listCursor = (listCursor + 1) % count; }
        showNoteDetail(listCursor);
      }
    } else if (rec == EV_SINGLE) {
      resetActivity();
      int idx = noteAtFilteredIndex(listCursor);
      if (idx >= 0) {
        char wavPath[64];
        snprintf(wavPath, sizeof(wavPath), "%s/note_%03d.wav", NOTES_DIR, noteIndex[idx].num);
        showPlaybackOverlay();
        playWavFile(wavPath);
        showNoteDetail(listCursor);
      }
    } else if (rec == EV_DOUBLE) {
      resetActivity();
      soundDelete();
      int idx = noteAtFilteredIndex(listCursor);
      if (idx >= 0) {
        showDeleteConfirm(noteIndex[idx].num);
        state = STATE_DELETE_CONFIRM;
      }
    }
    return;
  }

  // ─ STATE_DELETE_CONFIRM ─
  if (state == STATE_DELETE_CONFIRM) {
    if (rec == EV_SINGLE) {
      resetActivity();
      int idx = noteAtFilteredIndex(listCursor);
      if (idx >= 0) { deleteNote(noteIndex[idx].num); soundDelete(); }
      detailScrollPage = 0;
      listCursor = constrain(listCursor, 0, max(filteredCount() - 1, 0));
      state = STATE_NOTE_LIST;
      showNoteList(listCursor);
    } else if (pwr == EV_SINGLE) {
      resetActivity();
      soundBack();
      state = STATE_NOTE_DETAIL;
      showNoteDetail(listCursor);
    }
    return;
  }

  // ─ STATE_RESET_CONFIRM ─
  if (state == STATE_RESET_CONFIRM) {
    if (rec == EV_SINGLE) {
      cfg::factoryReset();
      soundDelete();
      showResetDone();
      delay(1400);
      ESP.restart();
    } else if (pwr == EV_SINGLE) {
      resetActivity();
      soundBack();
      state = STATE_SETTINGS;
      showSettings(settingsCursor);
    }
    return;
  }

  // ─ STATE_DELETE_ALL_CONFIRM ─
  if (state == STATE_DELETE_ALL_CONFIRM) {
    if (rec == EV_SINGLE) {
      resetActivity();
      bool alsoVault = (eraseAllCursor == 1);
      deleteAllNotes(alsoVault);
      activeFilter = -1; listCursor = 0;
      soundDelete();
      showDeleteAllDone(alsoVault);
      delay(1300);
      menuCursor = 0;
      state = STATE_MENU;
      showMenu(menuCursor);
    } else if (pwr == EV_SINGLE) {
      resetActivity();
      soundNext();
      eraseAllCursor = (eraseAllCursor + 1) % 2;
      showDeleteAllConfirm((int)noteIndex.size(), eraseAllCursor);
    } else if (pwr == EV_LONG || rec == EV_LONG) {
      resetActivity();
      soundBack();
      state = STATE_SETTINGS;
      showSettings(settingsCursor);
    }
    return;
  }

  // ─ STATE_TRANSFER ─
  if (state == STATE_TRANSFER) {
    if (rec == EV_LONG) {
      resetActivity();
      stopTransferMode();
      state = STATE_SETTINGS;
      showSettings(settingsCursor);
    }
    return;
  }

  // ─ STATE_DEVICE_INFO ─
  if (state == STATE_DEVICE_INFO) {
    if (pwr == EV_SINGLE || rec == EV_SINGLE || pwr == EV_LONG || rec == EV_LONG) {
      resetActivity();
      soundBack();
      state = STATE_SETTINGS;
      showSettings(settingsCursor);
    }
    return;
  }

  // ─ STATE_PET ─
  if (state == STATE_PET) {
    if (pwr == EV_SINGLE || pwr == EV_LONG || rec == EV_LONG) {
      resetActivity();
      soundBack();
      state = STATE_MENU;
      showMenu(menuCursor);
    }
    return;
  }

  // ─ STATE_USB_MSC ─ (handled entirely in usb_msc.cpp; no button dispatch here)
  if (state == STATE_USB_MSC) return;
}
