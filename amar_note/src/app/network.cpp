#include "Arduino.h"
#include "../../config.h"
#include "../../globals.h"
#include "../../types.h"
#include "network.h"
#include "notes.h"
#include "rtc.h"
#include "ui.h"
#include "battery.h"
#include "config_store.h"
#include "ota.h"
#include "WiFi.h"
#include "WiFiClientSecure.h"
#include <WebServer.h>
#include <HTTPUpdate.h>
#include <ArduinoJson.h>
#include "SD_MMC.h"
#include "esp_heap_caps.h"
#include "../../secrets.h"

extern const uint8_t x509_crt_bundle_start[] asm("_binary_x509_crt_bundle_start");
extern const uint8_t x509_crt_bundle_end[]   asm("_binary_x509_crt_bundle_end");

// ─── OTA progress globals (file-scope so lambdas and the API handler share them)
volatile int         g_otaPct   = 0;
volatile const char* g_otaStage = "idle";

// ─── Transcription ────────────────────────────────────────────────────────────
static bool transcribeOnce(const String& wavPath, int noteNum) {
  // Pick provider: 0 = OpenAI whisper-1, 1 = Groq whisper-large-v3-turbo
  uint8_t prov  = cfg::sttProvider();
  String  key   = (prov == 1) ? cfg::groqKey()   : cfg::openaiKey();
  const char* host  = (prov == 1) ? "api.groq.com" : "api.openai.com";
  const char* model = (prov == 1) ? "whisper-large-v3-turbo" : "whisper-1";
  const char* path  = "/openai/v1/audio/transcriptions";  // same path for both

  if (key.length() == 0) {
    Serial.printf("[Whisper] no API key for provider %d\n", prov);
    return false;
  }

  File f = SD_MMC.open(wavPath.c_str());
  if (!f) return false;
  size_t fileSize = f.size();

  String bnd = "----AmarBoundary";
  String pre = "--" + bnd + "\r\nContent-Disposition: form-data; name=\"model\"\r\n\r\n" + String(model) + "\r\n"
               "--" + bnd + "\r\nContent-Disposition: form-data; name=\"file\"; filename=\"note.wav\"\r\nContent-Type: audio/wav\r\n\r\n";
  String post = "\r\n--" + bnd + "--\r\n";
  size_t totalLen = pre.length() + fileSize + post.length();

  WiFiClientSecure client;
  client.setCACertBundle(x509_crt_bundle_start,
                         (size_t)(x509_crt_bundle_end - x509_crt_bundle_start));
  client.setHandshakeTimeout(15);

  if (!client.connect(host, 443, 15000)) { f.close(); return false; }

  client.printf("POST %s HTTP/1.1\r\n"
                "Host: %s\r\n"
                "Authorization: Bearer %s\r\n"
                "Content-Type: multipart/form-data; boundary=%s\r\n"
                "Content-Length: %u\r\n"
                "Connection: close\r\n\r\n",
                path, host, key.c_str(), bnd.c_str(), (unsigned)totalLen);
  client.print(pre);

  uint8_t* chunk = (uint8_t*)heap_caps_malloc(4096, MALLOC_CAP_8BIT);
  if (!chunk) { f.close(); client.stop(); return false; }
  while (f.available()) {
    int n = f.read(chunk, 4096);
    if (n <= 0) break;
    client.write(chunk, n);
  }
  heap_caps_free(chunk);
  f.close();
  client.print(post);

  uint32_t deadline = millis() + 90000;
  while (!client.available() && millis() < deadline) delay(20);

  String resp = "";
  bool inBody = false;
  while (client.available() || (client.connected() && millis() < deadline)) {
    if (!client.available()) { delay(10); continue; }
    String line = client.readStringUntil('\n');
    if (!inBody) {
      if (line == "\r" || line == "") inBody = true;
      if (line.startsWith("HTTP/") && line.indexOf(" 200 ") < 0) {
        Serial.printf("[Whisper] %s\n", line.c_str());
        client.stop(); return false;
      }
    } else {
      resp += line;
      if (resp.length() > 131072) break;
    }
  }
  client.stop();

  DynamicJsonDocument doc(resp.length() + 1024);
  DeserializationError jerr = deserializeJson(doc, resp);
  if (jerr) { Serial.printf("[Whisper] json: %s\n", jerr.c_str()); return false; }
  String text = doc["text"] | "";
  if (text.length() == 0) { Serial.println("[Whisper] empty response"); return false; }

  String tp = wavPath; tp.replace(".wav", ".txt");
  File tf = SD_MMC.open(tp.c_str(), FILE_WRITE);
  if (tf) { tf.print(text); tf.close(); }

  updateIndexHasText(noteNum);
  return true;
}

bool transcribe(const String& wavPath, int noteNum) {
  for (int attempt = 0; attempt < 3; attempt++) {
    if (WiFi.status() != WL_CONNECTED) return false;
    if (transcribeOnce(wavPath, noteNum)) return true;
    if (attempt < 2) { Serial.printf("[Whisper] retry %d/2\n", attempt + 1); delay(3000); }
  }
  return false;
}

void transcribeAll() {
  // Gate on whichever key matches the configured STT provider.
  if (!cfg::hasSttKey()) {
    Serial.printf("[Whisper] no key for stt_prov=%d; skipping sync\n", cfg::sttProvider());
    return;
  }

  int pending = 0;
  for (int i=0; i<(int)noteIndex.size(); i++) if(!noteIndex[i].hasText) pending++;
  int done = 0;
  for (int i=0; i<(int)noteIndex.size(); i++) {
    if (noteIndex[i].hasText) continue;
    if (WiFi.status() != WL_CONNECTED) {
      Serial.printf("[Whisper] wifi lost; %d note(s) stay pending\n", pending - done);
      break;
    }
    showTranscribing(done, pending);
    char wp[64]; snprintf(wp, sizeof(wp), "%s/note_%03d.wav", NOTES_DIR, noteIndex[i].num);
    if (transcribe(String(wp), noteIndex[i].num)) done++;
  }
  Serial.printf("[Whisper] synced %d/%d pending\n", done, pending);
}

// ─── Portal helpers ───────────────────────────────────────────────────────────
String htmlEscape(const String& s) {
  String out = s;
  out.replace("&", "&amp;"); out.replace("<", "&lt;");
  out.replace(">", "&gt;"); out.replace("\"", "&quot;");
  return out;
}

String readSmallFile(const char* path, size_t maxLen) {
  File f = SD_MMC.open(path);
  if (!f) return "";
  String out;
  while (f.available() && out.length() < maxLen) out += (char)f.read();
  f.close();
  return out;
}

String urlDecodeSimple(String s) {
  s.replace("+", " ");
  String out = "";
  for (int i = 0; i < (int)s.length(); i++) {
    if (s[i] == '%' && i + 2 < (int)s.length()) {
      String hex = s.substring(i + 1, i + 3);
      out += (char)strtol(hex.c_str(), nullptr, 16);
      i += 2;
    } else {
      out += s[i];
    }
  }
  return out;
}

// ─── Shared CSS ───────────────────────────────────────────────────────────────
String portalCss() {
  return String(
    "<style>"
    // ── Reset & base
    "*{box-sizing:border-box;margin:0;padding:0}"
    ":root{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;color:#111;background:#f3f0e9}"
    "body{background:#f3f0e9;padding:0 0 40px}"
    "a{color:inherit}"

    // ── Top header bar
    ".header{background:#111;color:#f3f0e9;padding:14px 20px;display:flex;align-items:center;justify-content:space-between;gap:12px}"
    ".header-brand{font-size:18px;font-weight:800;letter-spacing:-.04em;text-decoration:none;color:#f3f0e9}"
    ".header-status{display:flex;align-items:center;gap:12px;font-size:12px;color:#aaa8a3}"
    ".batt-wrap{display:flex;align-items:center;gap:5px}"
    ".batt-bar{width:28px;height:13px;border:1.5px solid #aaa8a3;border-radius:3px;position:relative;display:inline-block}"
    ".batt-bar::after{content:'';position:absolute;right:-4px;top:50%;transform:translateY(-50%);width:3px;height=6px;background:#aaa8a3;border-radius:0 2px 2px 0}"
    ".batt-fill{position:absolute;left:1px;top:1px;bottom:1px;border-radius:1px;background:#6db56d;transition:width .3s}"
    ".batt-fill.low{background:#c0392b}"
    ".batt-fill.charging{background:#f0b429}"
    ".header-time{white-space:nowrap}"

    // ── Breadcrumb nav
    ".breadcrumb{font-size:12px;color:#6a665f;padding:10px 20px 0;letter-spacing:.02em}"
    ".breadcrumb a{color:#6a665f;text-decoration:none;border-bottom:1px solid #ccc9c2}"
    ".breadcrumb a:hover{color:#111}"
    ".breadcrumb span{margin:0 5px;color:#bbb}"

    // ── Page wrap
    ".wrap{max-width:800px;margin:0 auto;padding:0 20px}"

    // ── Page title
    ".page-title{font-size:40px;font-weight:800;letter-spacing:-.06em;line-height:.9;margin:24px 0 6px}"
    ".page-sub{font-size:12px;text-transform:uppercase;letter-spacing:.12em;color:#6a665f;margin-bottom:22px}"

    // ── Cards
    ".card{background:#fffaf1;border:1.5px solid #111;border-radius:20px;padding:18px 20px;box-shadow:4px 4px 0 #111;margin-bottom:16px}"
    ".card-title{font-size:11px;font-weight:700;text-transform:uppercase;letter-spacing:.12em;color:#6a665f;margin-bottom:14px;display:flex;align-items:center;gap:8px}"
    ".badge{display:inline-block;font-size:10px;font-weight:700;padding:2px 7px;border-radius:999px;letter-spacing:.06em}"
    ".badge-ok{background:#d4edda;color:#1a6630}"
    ".badge-warn{background:#fff3cd;color:#856404}"
    ".badge-off{background:#e9ecef;color:#6c757d}"
    ".badge-new{background:#cfe2ff;color:#084298}"

    // ── Rows inside cards
    ".row{display:flex;justify-content:space-between;gap:16px;align-items:flex-start}"
    ".note-num{font-size:12px;letter-spacing:.08em;text-transform:uppercase;color:#6a665f;margin-bottom:6px}"
    ".note-date{font-size:12px;color:#6a665f;margin:-2px 0 10px}"
    ".note-title{font-size:22px;line-height:1.05;letter-spacing:-.04em;font-weight:750;margin:0 0 10px}"
    ".tag-chip{border:1px solid #111;border-radius:999px;padding:4px 9px;font-size:12px;white-space:nowrap;background:#111;color:#fff}"
    ".note-text{font-size:14px;line-height:1.5;color:#333;white-space:pre-wrap;margin:0 0 12px}"
    "audio{width:100%;margin-top:6px;border-radius:8px}"

    // ── Empty state
    ".empty{border:1.5px dashed #bbb;border-radius:20px;padding:34px;text-align:center;color:#6a665f;font-size:14px}"

    // ── Action rows
    ".actions{display:flex;flex-wrap:wrap;gap:8px}"
    ".actions.top{margin-bottom:20px}"

    // ── Buttons
    "a.btn,button.btn,button[type=submit]{display:inline-flex;align-items:center;font:inherit;font-size:13px;font-weight:600;"
    "color:#111;text-decoration:none;border:1.5px solid #111;border-radius:999px;padding:8px 14px;"
    "background:#f3f0e9;cursor:pointer;white-space:nowrap;transition:background .15s,color .15s}"
    "a.btn:hover,button.btn:hover,button[type=submit]:hover{background:#111;color:#f3f0e9}"
    "a.btn.primary,button.btn.primary,button[type=submit]{background:#111;color:#f3f0e9}"
    "a.btn.primary:hover,button.btn.primary:hover,button[type=submit]:hover{background:#333}"
    "a.btn.danger{border-color:#c0392b;color:#c0392b;background:#fffaf1}"
    "a.btn.danger:hover{background:#c0392b;color:#fff}"

    // ── Forms
    ".form-section{margin-bottom:18px}"
    ".form-label{font-size:12px;font-weight:600;letter-spacing:.04em;color:#555;display:block;margin-bottom:5px}"
    "input[type=text],input[type=password],input[type=url],select{"
    "font:inherit;font-size:14px;padding:10px 14px;border:1.5px solid #ccc9c2;"
    "border-radius:999px;background:#fff;width:100%;outline:none;"
    "transition:border-color .15s}"
    "input[type=text]:focus,input[type=password]:focus,input[type=url]:focus,select:focus{border-color:#111}"
    "select{border-radius:10px;padding-right:32px;appearance:none;"
    "background-image:url(\"data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='12' height='8' viewBox='0 0 12 8'%3E%3Cpath d='M1 1l5 5 5-5' stroke='%23666' stroke-width='1.5' fill='none' stroke-linecap='round'/%3E%3C/svg%3E\");"
    "background-repeat:no-repeat;background-position:right 12px center}"
    ".radio-group{display:flex;gap:10px;flex-wrap:wrap;margin-top:4px}"
    ".radio-group label{display:flex;align-items:center;gap:6px;font-size:13px;cursor:pointer;"
    "border:1.5px solid #ccc9c2;border-radius:999px;padding:7px 13px;background:#fff;"
    "transition:border-color .15s,background .15s}"
    ".radio-group label:has(input:checked){border-color:#111;background:#111;color:#f3f0e9}"
    ".radio-group input[type=radio]{display:none}"
    ".hint{font-size:12px;color:#6a665f;line-height:1.5;margin-top:4px}"
    "hr.divider{border:none;border-top:1px solid #e6e3dc;margin:18px 0}"

    // ── Tags page
    ".tag-row{display:flex;justify-content:space-between;align-items:center;gap:12px;border-top:1px solid #e6e3dc;padding:12px 0}"
    ".tag-row:first-child{border-top:none;padding-top:0}"
    ".tag-name{font-size:18px;font-weight:700}"
    ".tag-meta{font-size:12px;color:#6a665f;margin-top:3px}"
    ".msg{border:1.5px solid #111;border-radius:14px;padding:10px 14px;background:#fff;font-size:13px;margin-bottom:12px}"

    // ── Note count pill
    ".count-pill{display:inline-flex;align-items:center;border:1.5px solid #111;border-radius:999px;padding:7px 13px;font-size:13px;font-weight:600;background:#fffaf1}"

    // ── OTA version banner
    ".ver-banner{display:flex;align-items:center;gap:10px;flex-wrap:wrap;padding:12px 0 4px}"
    ".ver-item{font-size:13px}"
    ".ver-arrow{color:#6a665f;font-size:16px}"

    // ── OTA progress bar (shown on /ota/run page)
    ".ota-bar-wrap{background:#e6e3dc;border-radius:999px;overflow:hidden;height:14px;margin:18px 0 6px}"
    ".ota-bar{height:100%;background:#111;border-radius:999px;width:0%;transition:width .4s ease}"
    ".ota-stage{font-size:12px;text-transform:uppercase;letter-spacing:.1em;color:#6a665f}"

    // ── Responsive
    "@media(max-width:520px){"
    "body{padding:0 0 32px}"
    ".page-title{font-size:32px}"
    ".card{border-radius:16px;padding:14px 16px}"
    ".note-title{font-size:19px}"
    ".header{padding:11px 16px}"
    "}"
    "</style>"
  );
}

// ─── Shared header HTML (injected at top of every page) ──────────────────────
// Battery and time values are populated client-side via /api/status fetch.
String portalHeader(const char* pageTitle, const char* breadcrumb) {
  String h = "";
  // Top bar
  h += "<div class='header'>";
  h += "<a class='header-brand' href='/'>amar note</a>";
  h += "<div class='header-status'>";
  h += "<div class='batt-wrap' id='batt-wrap' style='display:none'>";
  h += "<div class='batt-bar'><div class='batt-fill' id='batt-fill' style='width:0%'></div></div>";
  h += "<span id='batt-pct'></span>";
  h += "</div>";
  h += "<span class='header-time' id='hdr-time'></span>";
  h += "</div></div>";
  // Breadcrumb
  if (breadcrumb && strlen(breadcrumb) > 0) {
    h += "<div class='breadcrumb'><a href='/'>portal</a><span>&#8250;</span>";
    h += String(breadcrumb);
    h += "</div>";
  }
  // JS: fetch /api/status once and populate header
  h += "<script>";
  h += "(function(){";
  h += "fetch('/api/status').then(function(r){return r.json();}).then(function(d){";
  // Time
  h += "if(d.time){var t=new Date(d.time);if(!isNaN(t)){document.getElementById('hdr-time').textContent=t.toLocaleTimeString([],{hour:'2-digit',minute:'2-digit'})+' \u00b7 '+t.toLocaleDateString([],{weekday:'short',month:'short',day:'numeric'});}}";
  // Battery
  h += "if(typeof d.batt==='number'){";
  h += "var w=document.getElementById('batt-wrap');var f=document.getElementById('batt-fill');var p=document.getElementById('batt-pct');";
  h += "w.style.display='flex';";
  h += "var pct=Math.max(0,Math.min(100,d.batt));";
  h += "f.style.width=pct+'%';";
  h += "f.className='batt-fill'+(pct<=20?' low':d.charging?' charging':'');";
  h += "p.textContent=pct+'%'+(d.charging?' \u26A1':'');";
  h += "}";
  h += "}).catch(function(){});";
  h += "})();";
  h += "</script>";
  return h;
}

// ─── /api/status ─────────────────────────────────────────────────────────────
void handleApiStatus() {
  int   batt     = readBatteryPercent();
  bool  charging = isBatteryCharging();
  String timeStr = rtcUtcIso();
  int   noteCount = (int)noteIndex.size();

  String json = "{";
  json += "\"batt\":" + String(batt) + ",";
  json += "\"charging\":" + String(charging ? "true" : "false") + ",";
  json += "\"notes\":" + String(noteCount);
  if (timeStr.length() > 0) json += ",\"time\":\"" + timeStr + "\"";
  json += "}";

  transferServer.sendHeader("Cache-Control", "no-cache");
  transferServer.send(200, "application/json", json);
}

// ─── Portal root (/) ─────────────────────────────────────────────────────────
void handlePortalRoot() {
  loadIndex();
  Serial.println("[HTTP] GET /");
  String filter = "All";
  if (transferServer.hasArg("tag")) filter = transferServer.arg("tag");

  transferServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
  transferServer.send(200, "text/html", "");

  String html = "<!doctype html><html><head><meta charset='utf-8'>"
                "<meta name='viewport' content='width=device-width,initial-scale=1'>"
                "<title>Amar Note Portal</title>" + portalCss() + "</head><body>";
  html += portalHeader("Portal", "");
  html += "<div class='wrap'>";
  html += "<div class='page-title'>notes</div>";
  html += "<div class='page-sub'>"
          "<a href='/tags'>tags</a> &middot; "
          "<a href='/provision'>setup</a> &middot; "
          "<a href='/ota'>update</a>"
          "</div>";

  // Tag filter + count row
  html += "<div class='actions top'>";
  html += "<a class='btn " + String(filter == "All" ? "primary" : "") + "' href='/'>All</a>";
  for (int t = 0; t < tagCount; t++) {
    String tag = String(tags[t]);
    html += "<a class='btn " + String(filter == tag ? "primary" : "") + "' href='/?tag=" + htmlEscape(tag) + "'>" + htmlEscape(tag) + "</a>";
  }
  html += "<span style='margin-left:auto'><span class='count-pill'>" + String((int)noteIndex.size()) + " notes</span></span>";
  html += "</div>";

  // Export
  html += "<div class='actions' style='margin-bottom:22px'>";
  html += "<a class='btn primary' href='/export.txt'>&#8681; Export all TXT</a>";
  if (filter != "All")
    html += "<a class='btn' href='/export.txt?tag=" + htmlEscape(filter) + "'>&#8681; Export " + htmlEscape(filter) + "</a>";
  html += "</div>";

  transferServer.sendContent(html); html = "";

  // Notes
  int visibleCount = 0;
  for (int i = 0; i < (int)noteIndex.size(); i++)
    if (filter == "All" || filter == String(noteIndex[i].tag)) visibleCount++;

  if (visibleCount <= 0) {
    html += "<div class='empty'>No notes" + String(filter != "All" ? " for this tag." : " yet.") + "</div>";
  } else {
    for (int v = 0; v < (int)noteIndex.size(); v++) {
      int i = (int)noteIndex.size() - 1 - v;
      if (!(filter == "All" || filter == String(noteIndex[i].tag))) continue;
      int num = noteIndex[i].num;

      char txtPath[64], wavPath[64];
      snprintf(txtPath, sizeof(txtPath), "%s/note_%03d.txt", NOTES_DIR, num);
      snprintf(wavPath, sizeof(wavPath), "%s/note_%03d.wav", NOTES_DIR, num);

      String transcript = readSmallFile(txtPath, 1200);
      if (transcript.length() == 0)
        transcript = noteIndex[i].hasText ? "(empty transcript)" : "Not transcribed yet.";

      String title = transcript; title.replace("\n", " "); title.trim();
      if (title.length() > 58) title = title.substring(0, 58) + "\u2026";
      if (title.length() == 0 || title == "Not transcribed yet.") title = String("Voice note ") + String(num);

      html += "<div class='card'>";
      html += "<div class='row'><div style='flex:1;min-width:0'>";
      html += "<div class='note-num'>#" + String(num) + "</div>";
      html += "<h2 class='note-title'>" + htmlEscape(title) + "</h2>";
      String createdUtc = noteCreatedUtc(num);
      if (createdUtc.length() > 0)
        html += "<div class='note-date' data-utc='" + createdUtc + "'>" + createdUtc + "</div>";
      else
        html += "<div class='note-date'>time not set</div>";
      html += "</div>";
      html += "<div><span class='tag-chip'>" + htmlEscape(String(noteIndex[i].tag)) + "</span></div></div>";
      html += "<p class='note-text'>" + htmlEscape(transcript) + "</p>";
      if (SD_MMC.exists(wavPath))
        html += "<audio controls src='/audio?num=" + String(num) + "'></audio>";
      html += "<div class='actions' style='margin-top:12px'>";
      html += "<a class='btn primary' href='/txt?num=" + String(num) + "'>&#8681; TXT</a>";
      if (SD_MMC.exists(wavPath))
        html += "<a class='btn' href='/wav?num=" + String(num) + "'>&#8681; WAV</a>";
      html += "<a class='btn danger' style='margin-left:auto' "
              "href='/note/delete?num=" + String(num) + "' "
              "onclick=\"return confirm('Delete note #" + String(num) + "? This cannot be undone.')\">Delete</a>";
      html += "</div></div>";

      if (html.length() > 2048) { transferServer.sendContent(html); html = ""; }
    }
  }

  // Date localisation
  html += "<script>document.querySelectorAll('[data-utc]').forEach(function(el){"
          "var d=new Date(el.dataset.utc);"
          "if(!isNaN(d))el.textContent=d.toLocaleString([],{year:'numeric',month:'short',day:'2-digit',hour:'2-digit',minute:'2-digit'});"
          "});</script>";
  html += "</div></body></html>";
  transferServer.sendContent(html);
  transferServer.sendContent("");
}

// ─── /api/notes JSON ─────────────────────────────────────────────────────────
void handlePortalJson() {
  loadIndex();
  String json = "[";
  for (int v = 0; v < (int)noteIndex.size(); v++) {
    int i = (int)noteIndex.size() - 1 - v;
    if (v > 0) json += ",";
    json += "{";
    json += "\"num\":" + String(noteIndex[i].num) + ",";
    json += "\"tag\":\"" + String(noteIndex[i].tag) + "\",";
    json += "\"hasText\":" + String(noteIndex[i].hasText ? "true" : "false");
    json += "}";
  }
  json += "]";
  transferServer.send(200, "application/json", json);
}

// ─── /export.txt ─────────────────────────────────────────────────────────────
void handleExportTxt() {
  loadIndex();
  String filter = "All";
  if (transferServer.hasArg("tag")) filter = transferServer.arg("tag");

  String filename = "amar_notes_export";
  if (filter != "All") filename += "_" + filter;
  filename += ".txt";

  transferServer.sendHeader("Content-Disposition", "attachment; filename=\"" + filename + "\"");
  transferServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
  transferServer.send(200, "text/plain", "");

  String chunk = "Amar Note Export\nFilter: " + filter + "\n------------------------------\n\n";
  for (int v = 0; v < (int)noteIndex.size(); v++) {
    int i = (int)noteIndex.size() - 1 - v;
    if (!(filter == "All" || filter == String(noteIndex[i].tag))) continue;
    int num = noteIndex[i].num;
    char txtPath[64]; snprintf(txtPath, sizeof(txtPath), "%s/note_%03d.txt", NOTES_DIR, num);
    String transcript = readSmallFile(txtPath, 4000);
    if (transcript.length() == 0)
      transcript = noteIndex[i].hasText ? "(empty transcript)" : "Not transcribed yet.";
    chunk += "#";
    if (num < 100) chunk += "0";
    if (num < 10)  chunk += "0";
    chunk += String(num) + " \u00b7 " + String(noteIndex[i].tag) + "\n";
    String createdUtc = noteCreatedUtc(num);
    if (createdUtc.length() > 0) chunk += createdUtc + "\n";
    chunk += "\n" + transcript + "\n\n------------------------------\n\n";
    if (chunk.length() > 2048) { transferServer.sendContent(chunk); chunk = ""; }
  }
  transferServer.sendContent(chunk);
  transferServer.sendContent("");
}

// ─── File download helpers ───────────────────────────────────────────────────
void sendFileByNum(const char* ext, const char* mime, bool attachment) {
  if (!transferServer.hasArg("num")) { transferServer.send(400, "text/plain", "Missing num"); return; }
  int num = transferServer.arg("num").toInt();
  if (num <= 0) { transferServer.send(400, "text/plain", "Invalid num"); return; }
  char path[64]; snprintf(path, sizeof(path), "%s/note_%03d.%s", NOTES_DIR, num, ext);
  File f = SD_MMC.open(path);
  if (!f) { transferServer.send(404, "text/plain", "File not found"); return; }
  if (attachment) {
    String filename = String("note_") + String(num) + "." + String(ext);
    transferServer.sendHeader("Content-Disposition", "attachment; filename=\"" + filename + "\"");
  }
  transferServer.streamFile(f, mime);
  f.close();
}

// ─── /tags ───────────────────────────────────────────────────────────────────
void handleTagAdd() {
  if (!transferServer.hasArg("name")) {
    transferServer.sendHeader("Location", "/tags?msg=missing"); transferServer.send(303); return;
  }
  String name = urlDecodeSimple(transferServer.arg("name"));
  bool ok = addCustomTag(name.c_str());
  transferServer.sendHeader("Location", ok ? "/tags?msg=added" : "/tags?msg=exists");
  transferServer.send(303);
}

void handleTagDelete() {
  if (!transferServer.hasArg("name")) {
    transferServer.sendHeader("Location", "/tags?msg=missing"); transferServer.send(303); return;
  }
  String name = urlDecodeSimple(transferServer.arg("name"));
  bool hadNotes = tagHasNotes(name.c_str());
  bool ok = deleteTag(name.c_str());
  if (ok && hadNotes) transferServer.sendHeader("Location", "/tags?msg=moved");
  else                transferServer.sendHeader("Location", ok ? "/tags?msg=deleted" : "/tags?msg=protected");
  transferServer.send(303);
}

void handleTagsPage() {
  loadTags();
  loadIndex();
  activeFilter = -1;

  String html = "<!doctype html><html><head><meta charset='utf-8'>"
                "<meta name='viewport' content='width=device-width,initial-scale=1'>"
                "<title>Amar Note \u00b7 Tags</title>" + portalCss() + "</head><body>";
  html += portalHeader("Tags", "tags");
  html += "<div class='wrap'>";
  html += "<div class='page-title'>tags</div>";
  html += "<div class='page-sub'>manage note categories</div>";

  if (transferServer.hasArg("msg")) {
    String msg = transferServer.arg("msg");
    const char* text = "Unknown message.";
    if      (msg == "added")     text = "\u2713 Tag added.";
    else if (msg == "exists")    text = "Tag already exists or is invalid.";
    else if (msg == "deleted")   text = "\u2713 Tag deleted.";
    else if (msg == "moved")     text = "\u2713 Tag deleted. Existing notes moved to Untagged.";
    else if (msg == "protected") text = "This tag cannot be deleted.";
    else if (msg == "missing")   text = "Please enter a tag name.";
    html += "<div class='msg'>" + String(text) + "</div>";
  }

  // Add tag form
  html += "<div class='card'>";
  html += "<div class='card-title'>Add tag</div>";
  html += "<form class='actions' action='/tag/add' method='get' style='gap:10px'>";
  html += "<input type='text' name='name' maxlength='31' placeholder='New tag name' style='max-width:280px'>";
  html += "<button type='submit' class='btn primary'>Add</button></form>";
  html += "<p class='hint' style='margin-top:10px'>Keep tags short \u2014 they display on the e-paper screen on the device.</p>";
  html += "</div>";

  // Tag list
  html += "<div class='card'>";
  html += "<div class='card-title'>" + String(tagCount) + " tag" + String(tagCount == 1 ? "" : "s") + "</div>";
  for (int i = 0; i < tagCount; i++) {
    int cnt = 0;
    for (int n = 0; n < (int)noteIndex.size(); n++)
      if (strcmp(noteIndex[n].tag, tags[i]) == 0) cnt++;
    html += "<div class='tag-row'><div>";
    html += "<div class='tag-name'>" + htmlEscape(String(tags[i])) + "</div>";
    html += "<div class='tag-meta'>" + String(cnt) + (cnt == 1 ? " note" : " notes");
    if (cnt > 0) html += " \u00b7 notes move to Untagged if deleted";
    html += "</div></div>";
    if (strcasecmp(tags[i], "Untagged") != 0)
      html += "<a class='btn danger' href='/tag/delete?name=" + htmlEscape(String(tags[i])) + "' "
              "onclick=\"return confirm('Delete tag? Notes will move to Untagged.')\">Delete</a>";
    html += "</div>";
  }
  html += "</div>";
  html += "</div></body></html>";
  transferServer.send(200, "text/html", html);
}

// ─── /note/delete ────────────────────────────────────────────────────────────
void handleNoteDelete() {
  if (!transferServer.hasArg("num")) { transferServer.send(400, "text/plain", "Missing num"); return; }
  int num = transferServer.arg("num").toInt();
  if (num <= 0) { transferServer.send(400, "text/plain", "Invalid num"); return; }
  deleteNote(num);
  transferServer.sendHeader("Location", "/");
  transferServer.send(303);
}

// ─── /provision (GET) ───────────────────────────────────────────────────────
void handleProvisionPage() {
  Serial.println("[HTTP] GET /provision");

  // Read current state for status badges and pre-fill
  bool hasWifi   = cfg::hasWifi();
  bool hasGroq   = cfg::hasGroqKey();
  bool hasOai    = cfg::hasOpenAiKey();
  bool hasGh     = cfg::hasGithub();
  bool ghOn      = cfg::githubEnabled();
  bool ghAi      = cfg::githubAiEnrich();
  uint8_t sttProv     = cfg::sttProvider();    // 0=OpenAI, 1=Groq
  uint8_t enrichProv  = cfg::enrichProvider(); // 0=OpenAI, 1=Groq
  String enrichModel  = cfg::enrichModel();
  String ghRepo   = cfg::githubRepo();
  String ghBranch = cfg::githubBranch();
  String ghDir    = cfg::githubDir();

  String html = "<!doctype html><html><head><meta charset='utf-8'>"
                "<meta name='viewport' content='width=device-width,initial-scale=1'>"
                "<title>Amar Note \u00b7 Setup</title>" + portalCss() + "</head><body>";
  html += portalHeader("Setup", "setup");
  html += "<div class='wrap'>";
  html += "<div class='page-title'>setup</div>";
  html += "<div class='page-sub'>device provisioning</div>";

  // ── Status summary row
  html += "<div class='card'>";
  html += "<div class='card-title'>current status</div>";
  html += "<div class='actions' style='gap:8px'>";
  auto badge = [&](const char* label, bool ok, const char* offLabel = nullptr) -> String {
    String cls = ok ? "badge-ok" : "badge-warn";
    String lbl = ok ? String(label) + " \u2713" : String(offLabel ? offLabel : label) + " \u26A0";
    return "<span class='badge " + cls + "'>" + lbl + "</span>";
  };
  html += badge("Wi-Fi", hasWifi);
  html += badge("Groq", hasGroq);
  html += badge("OpenAI", hasOai);
  html += badge("GitHub", hasGh, ghOn ? "GitHub (key missing)" : "GitHub (off)");
  html += "</div></div>";

  html += "<form action='/provision/save' method='post'>";

  // ── Wi-Fi card
  html += "<div class='card'>";
  html += "<div class='card-title'>Wi-Fi " + badge("connected", hasWifi, "not configured") + "</div>";
  html += "<div class='form-section'><label class='form-label'>Network name (SSID)</label>"
          "<input type='text' name='ssid' placeholder='Your Wi-Fi network name'></div>";
  html += "<div class='form-section'><label class='form-label'>Password</label>"
          "<input type='password' name='pass' placeholder='Wi-Fi password'></div>";
  html += "<p class='hint'>Leave blank to keep the current network.</p>";
  html += "</div>";

  // ── Transcription & AI card
  html += "<div class='card'>";
  html += "<div class='card-title'>Transcription &amp; AI</div>";

  // STT provider
  html += "<div class='form-section'>";
  html += "<label class='form-label'>Transcription engine</label>";
  html += "<div class='radio-group'>";
  html += "<label><input type='radio' name='stt_prov' value='1'" + String(sttProv == 1 ? " checked" : "") + ">";
  html += "Groq Whisper <span style='font-size:11px;color:#6a665f'>(recommended \u00b7 free)</span></label>";
  html += "<label><input type='radio' name='stt_prov' value='0'" + String(sttProv == 0 ? " checked" : "") + ">";
  html += "OpenAI Whisper</label>";
  html += "</div></div>";

  // Groq key
  html += "<div class='form-section'>";
  html += "<label class='form-label'>Groq API key " + badge("set", hasGroq, "not set") + "</label>";
  html += "<input type='password' name='groq' placeholder='gsk_\u2026 \u2014 free key at console.groq.com/keys'>";
  html += "</div>";

  // OpenAI key
  html += "<div class='form-section'>";
  html += "<label class='form-label'>OpenAI API key " + badge("set", hasOai, "not set") + "</label>";
  html += "<input type='password' name='openai' placeholder='sk-\u2026 \u2014 platform.openai.com/api-keys'>";
  html += "</div>";

  html += "<hr class='divider'>";

  // AI enrichment provider
  html += "<div class='form-section'>";
  html += "<label class='form-label'>AI enrichment engine <span class='hint' style='display:inline'>(titles &amp; topic links)</span></label>";
  html += "<div class='radio-group'>";
  html += "<label><input type='radio' name='enrich_prov' value='1'" + String(enrichProv == 1 ? " checked" : "") + ">Groq</label>";
  html += "<label><input type='radio' name='enrich_prov' value='0'" + String(enrichProv == 0 ? " checked" : "") + ">OpenAI GPT</label>";
  html += "</div></div>";

  // Groq enrichment model
  html += "<div class='form-section' id='groq-model-row' style='" + String(enrichProv == 1 ? "" : "display:none") + "'>";
  html += "<label class='form-label'>Groq model</label>";
  html += "<select name='enrich_model'>";
  const char* models[] = {"llama-3.3-70b-versatile", "llama-3.1-8b-instant", "llama-4-scout-17b-16e-instruct"};
  const char* modelLabels[] = {"Llama 3.3 70B Versatile (recommended)", "Llama 3.1 8B Instant (fast)", "Llama 4 Scout 17B (experimental)"};
  for (int m = 0; m < 3; m++) {
    html += "<option value='" + String(models[m]) + "'" + String(enrichModel == models[m] ? " selected" : "") + ">";
    html += String(modelLabels[m]) + "</option>";
  }
  html += "</select></div>";

  html += "<p><label style='display:flex;align-items:center;gap:8px;font-size:13px;cursor:pointer'>";
  html += "<input type='checkbox' name='gh_ai' value='1'" + String(ghAi ? " checked" : "") + ">";
  html += "Enable AI enrichment (titles + topic links)</label></p>";
  html += "<p class='hint' style='margin-top:4px'>Requires either a Groq or OpenAI key above.</p>";
  html += "</div>"; // end AI card

  // ── GitHub / Obsidian card
  html += "<div class='card'>";
  html += "<div class='card-title'>GitHub vault " + badge("active", hasGh, ghOn ? "key missing" : "off") + "</div>";
  html += "<p class='hint' style='margin-bottom:14px'>Push transcribed notes as Markdown to a GitHub repo. "
          "Point Obsidian at the same repo to have notes appear automatically.</p>";

  html += "<div class='form-section'><label class='form-label'>Repository <span style='font-weight:400'>(owner/name)</span></label>"
          "<input type='text' name='gh_repo' placeholder='yourname/vault' value='" + htmlEscape(ghRepo) + "'></div>";
  html += "<div class='form-section'><label class='form-label'>Branch</label>"
          "<input type='text' name='gh_branch' placeholder='main' value='" + htmlEscape(ghBranch) + "'></div>";
  html += "<div class='form-section'><label class='form-label'>Vault folder</label>"
          "<input type='text' name='gh_dir' placeholder='VoiceNotes' value='" + htmlEscape(ghDir) + "'></div>";
  html += "<div class='form-section'><label class='form-label'>Personal access token</label>"
          "<input type='password' name='gh_token' placeholder='github_pat_\u2026'></div>";
  html += "<p class='hint'>Create a fine-grained token with <b>Contents: Read &amp; Write</b> at "
          "<a href='https://github.com/settings/tokens' target='_blank'>github.com/settings/tokens</a>.</p>";

  html += "<hr class='divider'>";
  html += "<p><label style='display:flex;align-items:center;gap:8px;font-size:13px;cursor:pointer'>";
  html += "<input type='checkbox' name='gh_on' value='1'" + String(ghOn ? " checked" : "") + ">";
  html += "Enable GitHub sync</label></p>";
  html += "</div>"; // end GitHub card

  html += "<p class='hint' style='margin-bottom:18px'>Leave any key field blank to keep its current value.</p>";
  html += "<button type='submit' class='btn primary'>Save settings</button>";
  html += "</form>";
  html += "</div></body>";

  // Show/hide Groq model selector based on enrichment provider radio
  html += "<script>"
          "document.querySelectorAll('input[name=enrich_prov]').forEach(function(r){"
          "r.addEventListener('change',function(){"
          "document.getElementById('groq-model-row').style.display=(this.value==='1'?'block':'none');"
          "});});"
          "</script>";
  html += "</html>";
  transferServer.send(200, "text/html", html);
}

// ─── /provision/save (POST) ───────────────────────────────────────────────────
void handleProvisionSave() {
  String ssid    = transferServer.hasArg("ssid")   ? transferServer.arg("ssid")   : "";
  String pass    = transferServer.hasArg("pass")   ? transferServer.arg("pass")   : "";
  String groqKey = transferServer.hasArg("groq")   ? transferServer.arg("groq")   : "";
  String oaiKey  = transferServer.hasArg("openai") ? transferServer.arg("openai") : "";
  ssid.trim(); groqKey.trim(); oaiKey.trim();
  bool changed = false;

  if (ssid.length()    > 0) { cfg::setWifi(ssid, pass);     changed = true; }
  if (groqKey.length() > 0) { cfg::setGroqKey(groqKey);     changed = true; }
  if (oaiKey.length()  > 0) { cfg::setOpenAiKey(oaiKey);    changed = true; }

  // STT provider
  if (transferServer.hasArg("stt_prov")) {
    cfg::setSttProvider((uint8_t)transferServer.arg("stt_prov").toInt());
    changed = true;
  }

  // AI enrichment provider + model
  if (transferServer.hasArg("enrich_prov")) {
    cfg::setEnrichProvider((uint8_t)transferServer.arg("enrich_prov").toInt());
    changed = true;
  }
  if (transferServer.hasArg("enrich_model")) {
    String m = transferServer.arg("enrich_model"); m.trim();
    if (m.length() > 0) { cfg::setEnrichModel(m); changed = true; }
  }

  // GitHub
  if (transferServer.hasArg("gh_repo")) {
    String r = transferServer.arg("gh_repo"); r.trim();
    if (r.length() > 0) { cfg::setGithubRepo(r); changed = true; }
  }
  if (transferServer.hasArg("gh_branch")) {
    String b = transferServer.arg("gh_branch"); b.trim();
    if (b.length() > 0) { cfg::setGithubBranch(b); changed = true; }
  }
  if (transferServer.hasArg("gh_dir")) {
    String d = transferServer.arg("gh_dir"); d.trim();
    if (d.length() > 0) { cfg::setGithubDir(d); changed = true; }
  }
  if (transferServer.hasArg("gh_token")) {
    String t = transferServer.arg("gh_token"); t.trim();
    if (t.length() > 0) { cfg::setGithubToken(t); changed = true; }
  }
  cfg::setGithubEnabled(transferServer.hasArg("gh_on"));
  cfg::setGithubAiEnrich(transferServer.hasArg("gh_ai"));
  changed = true;

  String html = "<!doctype html><html><head><meta charset='utf-8'>"
                "<meta name='viewport' content='width=device-width,initial-scale=1'>"
                "<title>Amar Note \u00b7 Setup</title>" + portalCss() + "</head><body>";
  html += portalHeader("Setup", "setup");
  html += "<div class='wrap'>";
  html += "<div class='card' style='margin-top:24px'>";
  html += "<div class='page-title' style='margin-top:0'>" + String(changed ? "saved \u2713" : "no change") + "</div>";
  html += "<p class='hint' style='margin:10px 0 18px'>" + String(changed
    ? "Settings stored on the device. Re-open Transfer or Sync to use them."
    : "Nothing was submitted.") + "</p>";
  html += "<div class='actions'>";
  html += "<a class='btn primary' href='/provision'>Back to setup</a>";
  html += "<a class='btn' href='/'>Notes</a>";
  html += "</div></div></div></body></html>";
  transferServer.send(200, "text/html", html);
}

// ─── /ota/check ──────────────────────────────────────────────────────────────
void handleOtaCheck() {
  if (WiFi.status() != WL_CONNECTED) {
    transferServer.sendHeader("Location", "/ota?err=1");
    transferServer.send(303);
    return;
  }
  String latestTag, assetUrl;
  bool newer = otaCheckGithub(latestTag, assetUrl);
  if (latestTag.length() == 0) {
    transferServer.sendHeader("Location", "/ota?err=1");
    transferServer.send(303);
    return;
  }
  if (!newer) {
    transferServer.sendHeader("Location", "/ota?uptodate=1&latest=" + latestTag);
    transferServer.send(303);
    return;
  }
  String enc = "";
  for (int i = 0; i < (int)assetUrl.length(); i++) {
    char c = assetUrl[i];
    if (isAlphaNumeric(c) || c == '-' || c == '_' || c == '.' || c == '~' || c == ':' || c == '/' || c == '?' || c == '=' || c == '&') {
      enc += c;
    } else {
      char buf[4]; snprintf(buf, sizeof(buf), "%%%02X", (unsigned char)c);
      enc += buf;
    }
  }
  transferServer.sendHeader("Location", "/ota?update=1&tag=" + latestTag + "&url=" + enc);
  transferServer.send(303);
}

// ─── /ota ────────────────────────────────────────────────────────────────────
void handleOtaPage() {
  bool hasUpdate  = transferServer.hasArg("update")   && transferServer.arg("update")   == "1";
  bool upToDate   = transferServer.hasArg("uptodate") && transferServer.arg("uptodate") == "1";
  bool checkErr   = transferServer.hasArg("err")      && transferServer.arg("err")      == "1";
  String prefillUrl = hasUpdate ? urlDecodeSimple(transferServer.arg("url")) : "";
  String latestTag  = hasUpdate ? transferServer.arg("tag") :
                      (upToDate ? transferServer.arg("latest") : "");

  String html = "<!doctype html><html><head><meta charset='utf-8'>"
                "<meta name='viewport' content='width=device-width,initial-scale=1'>"
                "<title>Amar Note \u00b7 Update</title>" + portalCss() + "</head><body>";
  html += portalHeader("Update", "update");
  html += "<div class='wrap'>";
  html += "<div class='page-title'>update</div>";
  html += "<div class='page-sub'>firmware " FW_VERSION "</div>";

  html += "<div class='card'>";
  html += "<div class='card-title'>github release check</div>";

  if (hasUpdate) {
    html += "<div class='ver-banner'>";
    html += "<span class='ver-item'>current &nbsp;<strong>" FW_VERSION "</strong></span>";
    html += "<span class='ver-arrow'>&#8594;</span>";
    html += "<span class='ver-item'><strong>" + htmlEscape(latestTag) + "</strong> &nbsp;<span class='badge badge-new'>update available</span></span>";
    html += "</div>";
    html += "<p class='hint' style='margin-bottom:14px'>A new release was found. The URL below has been pre-filled \u2014 tap <b>Flash firmware</b> to install it.</p>";
  } else if (upToDate) {
    html += "<div class='ver-banner'>";
    html += "<span class='ver-item'>current &nbsp;<strong>" FW_VERSION "</strong></span>";
    html += "<span class='ver-arrow'>&#183;</span>";
    html += "<span class='ver-item'>latest &nbsp;<strong>" + htmlEscape(latestTag) + "</strong> &nbsp;<span class='badge badge-ok'>up to date &#10003;</span></span>";
    html += "</div>";
  } else if (checkErr) {
    html += "<p class='hint' style='margin-bottom:10px;color:#c0392b'>&#9888; Could not reach GitHub. Check Wi-Fi and try again.</p>";
  } else {
    html += "<p class='hint' style='margin-bottom:14px'>Check for the latest release on GitHub &#40;" OTA_GITHUB_OWNER "/" OTA_GITHUB_REPO "&#41;. "
            "If a newer version exists its download URL will be pre-filled automatically.</p>";
  }

  html += "<a class='btn primary' href='/ota/check'>Check GitHub</a>";
  html += "</div>";

  html += "<div class='card'>";
  html += "<div class='card-title'>over-the-air flash</div>";
  html += "<p class='hint' style='margin-bottom:14px'>Paste an HTTPS URL to a compiled <code>.bin</code> firmware file, or use the pre-filled URL from the check above. "
          "The device verifies the server certificate, flashes the inactive OTA slot, "
          "and reboots into it &mdash; rolling back automatically if it fails to boot.</p>";
  html += "<form action='/ota/run' method='post'>";
  html += "<div class='form-section'><label class='form-label'>Firmware URL</label>";
  html += "<input type='url' name='url' placeholder='https://host/amar-note.bin' value='" + htmlEscape(prefillUrl) + "'></div>";
  html += "<button type='submit' class='btn primary'>Flash firmware</button>";
  html += "</form>";
  html += "<hr class='divider'>";
  html += "<p class='hint'>\u26A0\uFE0F Keep the device plugged in during the update. "
          "Do not close this page until the device reboots. "
          "If the update fails, the device stays on the current firmware automatically.</p>";
  html += "</div>";
  html += "<a class='btn' href='/'>Back to notes</a>";
  html += "</div></body></html>";
  transferServer.send(200, "text/html", html);
}

// ─── /ota/run (POST) ─────────────────────────────────────────────────────────
void handleOtaRun() {
  if (!transferServer.hasArg("url") || transferServer.arg("url").length() == 0) {
    transferServer.send(400, "text/plain", "Missing url"); return;
  }
  String url = transferServer.arg("url");

  // ── Send browser page immediately (device blocks during httpUpdate.update)
  transferServer.send(200, "text/html",
    "<!doctype html><html><head><meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>Amar Note &middot; Updating&hellip;</title>"
    + portalCss() +
    "</head><body>"
    "<div class='header'>"
    "<span class='header-brand'>amar note</span>"
    "</div>"
    "<div class='wrap' style='padding-top:32px'>"
    "<div class='page-title'>updating&hellip;</div>"
    "<div class='page-sub'>do not close this page</div>"
    "<div class='card'>"
    "<div class='card-title'>flashing firmware</div>"
    "<div class='ota-bar-wrap'><div class='ota-bar' id='bar'></div></div>"
    "<div class='ota-stage' id='stage'>connecting&hellip;</div>"
    "<p class='hint' style='margin-top:16px'>"
    "Watch the device screen for live progress. "
    "The device reboots automatically when done &mdash; "
    "if it fails it stays on the current version.</p>"
    "</div></div>"
    // Poll /api/ota-progress every 1 s and update the bar
    "<script>"
    "(function(){"
    "var bar=document.getElementById('bar');"
    "var stage=document.getElementById('stage');"
    "function poll(){"
    "fetch('/api/ota-progress').then(function(r){return r.json();}).then(function(d){"
    "bar.style.width=d.pct+'%';"
    "stage.textContent=d.stage||'';"
    "if(d.pct<100)setTimeout(poll,1000);"
    "}).catch(function(){setTimeout(poll,2000);});}"
    "setTimeout(poll,1000);"
    "})();"
    "</script>"
    "</body></html>");
  delay(250);

  // ── State shared between callbacks (stack-safe: small ints + const ptr)
  static int s_lastPct = -1;
  s_lastPct = -1;

  // Reset progress globals before starting
  g_otaPct   = 0;
  g_otaStage = "connecting";

  // ── onStart: show blank bar at 0%
  httpUpdate.onStart([]() {
    g_otaPct   = 0;
    g_otaStage = "downloading";
    showOtaProgress(0, "downloading");
    Serial.println("[OTA] start");
  });

  // ── onProgress: gate redraws to every >=5% change
  httpUpdate.onProgress([](int current, int total) {
    int pct = (total > 0) ? constrain((current * 100) / total, 0, 99) : 0;
    g_otaPct = pct;
    if (pct >= s_lastPct + 5 || pct == 0) {
      s_lastPct = pct;
      showOtaProgress(pct, "downloading");
      Serial.printf("[OTA] progress %d%%\n", pct);
    }
  });

  // ── onEnd: flash complete, show 100% before reboot
  httpUpdate.onEnd([]() {
    g_otaPct   = 100;
    g_otaStage = "rebooting";
    showOtaProgress(100, "rebooting");
    Serial.println("[OTA] done — rebooting");
  });

  // ── onError: show error screen
  httpUpdate.onError([](int err) {
    g_otaPct   = 0;
    g_otaStage = "failed";
    showError("ota failed");
    Serial.printf("[OTA] error %d: %s\n", err, httpUpdate.getLastErrorString().c_str());
  });

  WiFiClientSecure client;
  client.setCACertBundle(x509_crt_bundle_start,
                         (size_t)(x509_crt_bundle_end - x509_crt_bundle_start));
  httpUpdate.rebootOnUpdate(true);
  t_httpUpdate_return r = httpUpdate.update(client, url, FW_VERSION);
  if (r == HTTP_UPDATE_FAILED)
    Serial.printf("[OTA] failed (%d): %s\n",
                  httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
  else if (r == HTTP_UPDATE_NO_UPDATES)
    Serial.println("[OTA] no update available");
}

// ─── /api/ota-progress ───────────────────────────────────────────────────────
// Polled by the browser page while httpUpdate.update() is blocking.
void handleOtaProgressApi() {
  String json = "{\"pct\":";
  json += String((int)g_otaPct);
  json += ",\"stage\":\"";
  json += String((const char*)g_otaStage);
  json += "\"}";
  transferServer.sendHeader("Cache-Control", "no-cache");
  transferServer.send(200, "application/json", json);
}

// ─── Server setup ─────────────────────────────────────────────────────────────
void setupTransferServer() {
  transferServer.on("/",                HTTP_GET,  handlePortalRoot);
  transferServer.on("/api/status",      HTTP_GET,  handleApiStatus);
  transferServer.on("/api/ota-progress",HTTP_GET,  handleOtaProgressApi);
  transferServer.on("/provision",       HTTP_GET,  handleProvisionPage);
  transferServer.on("/provision/save",  HTTP_POST, handleProvisionSave);
  transferServer.on("/ota",             HTTP_GET,  handleOtaPage);
  transferServer.on("/ota/check",       HTTP_GET,  handleOtaCheck);
  transferServer.on("/ota/run",         HTTP_POST, handleOtaRun);
  transferServer.on("/tags",            HTTP_GET,  handleTagsPage);
  transferServer.on("/tag/add",         HTTP_GET,  handleTagAdd);
  transferServer.on("/tag/delete",      HTTP_GET,  handleTagDelete);
  transferServer.on("/note/delete",     HTTP_GET,  handleNoteDelete);
  transferServer.on("/api/notes",       HTTP_GET,  handlePortalJson);
  transferServer.on("/export.txt",      HTTP_GET,  handleExportTxt);
  transferServer.on("/txt",   HTTP_GET, [](){ sendFileByNum("txt", "text/plain", true); });
  transferServer.on("/wav",   HTTP_GET, [](){ sendFileByNum("wav", "audio/wav",  true); });
  transferServer.on("/audio", HTTP_GET, [](){ sendFileByNum("wav", "audio/wav",  false); });
  transferServer.onNotFound([](){
    Serial.printf("[HTTP] miss: %s\n", transferServer.uri().c_str());
    if (captivePortalActive) {
      transferServer.sendHeader("Location", "http://" + transferUrl + "/provision", true);
      transferServer.send(302, "text/plain", "");
    } else {
      transferServer.send(404, "text/plain", "Not found");
    }
  });
}

void stopTransferMode() {
  if (transferServerActive) {
    transferServer.stop();
    transferServerActive = false;
  }
  if (captivePortalActive) {
    dnsServer.stop();
    captivePortalActive = false;
  }
  WiFi.softAPdisconnect(true);
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  transferUrl = "";
}
