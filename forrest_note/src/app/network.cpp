#include "Arduino.h"
#include "../../config.h"
#include "../../globals.h"
#include "network.h"
#include "config_store.h"
#include "ui.h"
#include "record.h"
#include "WiFi.h"
#include "WebServer.h"
#include "HTTPClient.h"
#include "WiFiClientSecure.h"
#include "ArduinoJson.h"
#include "SD_MMC.h"
#include "esp_ota_ops.h"
#include "esp_https_ota.h"
#include "mbedtls/ssl.h"

// ============================================================
// network.cpp — Amar Note Wi-Fi, portal, transcription, GitHub sync
// ============================================================

static WebServer server(80);
static bool      wifiConnected = false;
static QueueHandle_t transcribeQueue = nullptr;

// --- Mozilla CA bundle (truncated representative root) ---
extern const uint8_t mozilla_ca_bundle[] asm("_binary_ca_bundle_pem_start");
extern const uint8_t mozilla_ca_bundle_end[] asm("_binary_ca_bundle_pem_end");

// ---- Helpers ----

static String htmlEscape(const String& s) {
    String out;
    out.reserve(s.length());
    for (char c : s) {
        if      (c == '&')  out += "&amp;";
        else if (c == '<')  out += "&lt;";
        else if (c == '>')  out += "&gt;";
        else if (c == '"')  out += "&quot;";
        else                out += c;
    }
    return out;
}

static String base64Encode(const uint8_t* data, size_t len) {
    static const char* b64 =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    String out;
    out.reserve(((len + 2) / 3) * 4);
    for (size_t i = 0; i < len; i += 3) {
        uint32_t v = ((uint32_t)data[i] << 16)
                   | (i+1 < len ? (uint32_t)data[i+1] << 8 : 0)
                   | (i+2 < len ? (uint32_t)data[i+2]      : 0);
        out += b64[(v >> 18) & 0x3F];
        out += b64[(v >> 12) & 0x3F];
        out += (i+1 < len) ? b64[(v >>  6) & 0x3F] : '=';
        out += (i+2 < len) ? b64[(v      ) & 0x3F] : '=';
    }
    return out;
}

// ---- Portal HTML ----

static const char PORTAL_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Amar Note — Setup</title>
<style>
body{font-family:system-ui,sans-serif;max-width:480px;margin:2rem auto;padding:0 1rem;background:#f7f6f2;color:#28251d}
h1{font-size:1.4rem;margin-bottom:1.5rem}
label{display:block;font-size:.85rem;margin:.8rem 0 .2rem;font-weight:500}
input[type=text],input[type=password]{width:100%;box-sizing:border-box;padding:.5rem .6rem;
  border:1px solid #d4d1ca;border-radius:6px;font-size:1rem;background:#fff}
.cb{display:flex;align-items:center;gap:.5rem;margin:.6rem 0}
button{width:100%;margin-top:1.5rem;padding:.7rem;background:#01696f;color:#fff;
  border:none;border-radius:8px;font-size:1rem;cursor:pointer}
button:hover{background:#0c4e54}
.note{font-size:.8rem;color:#7a7974;margin-top:.4rem}
</style>
</head>
<body>
<h1>🎙️ Amar Note — Setup</h1>
<form method="POST" action="/provision">
<label>Wi-Fi network (SSID)</label>
<input type="text" name="ssid" placeholder="Your 2.4 GHz Wi-Fi name">
<label>Wi-Fi password</label>
<input type="password" name="pass" placeholder="Leave blank to keep current">
<label>OpenAI API key</label>
<input type="text" name="openai" placeholder="sk-...">
<label>GitHub repo (owner/name)</label>
<input type="text" name="repo" placeholder="yourname/Notes">
<label>Branch</label>
<input type="text" name="branch" placeholder="main">
<label>Vault folder</label>
<input type="text" name="dir" placeholder="VoiceNotes">
<label>GitHub token</label>
<input type="text" name="token" placeholder="github_pat_...">
<div class="cb"><input type="checkbox" name="gh_en" id="gh_en" checked><label for="gh_en" style="margin:0">Enable GitHub sync</label></div>
<div class="cb"><input type="checkbox" name="ai_en" id="ai_en" checked><label for="ai_en" style="margin:0">AI titles + topic links</label></div>
<button type="submit">Save &amp; reboot</button>
</form>
<p class="note">Leave any field blank to keep its current value.</p>
</body>
</html>
)HTML";

// ---- Handlers ----

static void handleRoot() {
    server.send(200, "text/html", PORTAL_HTML);
}

static void handleProvision() {
    if (server.method() != HTTP_POST) { server.sendHeader("Location","/"); server.send(302); return; }

    auto arg = [&](const char* k) -> String { return server.arg(k); };
    if (arg("ssid").length())   configSetSSID(arg("ssid"));
    if (arg("pass").length())   configSetPass(arg("pass"));
    if (arg("openai").length()) configSetOpenAIKey(arg("openai"));
    if (arg("repo").length())   configSetGHRepo(arg("repo"));
    if (arg("branch").length()) configSetGHBranch(arg("branch"));
    if (arg("dir").length())    configSetGHDir(arg("dir"));
    if (arg("token").length())  configSetGHToken(arg("token"));
    configSetGHEnabled(server.hasArg("gh_en"));
    configSetAIEnrich(server.hasArg("ai_en"));

    server.send(200, "text/html",
        "<h2>Saved! Rebooting…</h2>"
        "<p>Reconnect to your Wi-Fi. Visit <b>http://&lt;device-ip&gt;/provision</b> to edit later.</p>");
    delay(800);
    ESP.restart();
}

static void handleOTA() {
    server.send(200, "text/html",
        "<h2>Amar Note OTA</h2>"
        "<form method=POST action=/ota_upload enctype=multipart/form-data>"
        "<input type=file name=firmware accept=.bin>"
        "<button>Flash</button></form>");
}

// ---- Public API ----

void networkInit() {
    transcribeQueue = xQueueCreate(8, sizeof(char)*128);

    server.on("/",         HTTP_GET,  handleRoot);
    server.on("/provision",HTTP_GET,  handleRoot);
    server.on("/provision",HTTP_POST, handleProvision);
    server.on("/ota",      HTTP_GET,  handleOTA);
    server.begin();
    Serial.println("[network] portal started");

    String ssid = configGetSSID();
    String pass = configGetPass();
    if (ssid.length() > 0) {
        WiFi.begin(ssid.c_str(), pass.c_str());
        uint32_t t = millis();
        while (WiFi.status() != WL_CONNECTED && millis()-t < 12000) delay(200);
        wifiConnected = (WiFi.status() == WL_CONNECTED);
        if (wifiConnected)
            Serial.printf("[network] Wi-Fi connected, IP %s\n",
                          WiFi.localIP().toString().c_str());
        else
            Serial.println("[network] Wi-Fi failed, portal still up");
    }
}

void networkService() { server.handleClient(); }
bool networkIsConnected() { return wifiConnected; }

void networkEnqueueTranscribe(const char* path) {
    char buf[128];
    strncpy(buf, path, sizeof(buf)-1);
    buf[sizeof(buf)-1] = '\0';
    xQueueSend(transcribeQueue, buf, 0);
}

// ---- Transcription + enrichment (blocking, call from loop when idle) ----

static String whisperTranscribe(const char* wavPath) {
    File f = SD_MMC.open(wavPath);
    if (!f) return "";
    size_t sz = f.size();
    uint8_t* buf = (uint8_t*)malloc(sz);
    if (!buf) { f.close(); return ""; }
    f.read(buf, sz);
    f.close();

    WiFiClientSecure client;
    client.setCACert((const char*)mozilla_ca_bundle);

    HTTPClient http;
    http.begin(client, "https://api.openai.com/v1/audio/transcriptions");
    http.addHeader("Authorization", "Bearer " + configGetOpenAIKey());

    // Multipart
    String boundary = "----AmarNoteBoundary";
    String head = "--" + boundary + "\r\n"
        "Content-Disposition: form-data; name=\"model\"\r\n\r\nwhisper-1\r\n"
        "--" + boundary + "\r\n"
        "Content-Disposition: form-data; name=\"file\"; filename=\"audio.wav\"\r\n"
        "Content-Type: audio/wav\r\n\r\n";
    String tail = "\r\n--" + boundary + "--\r\n";

    http.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);
    uint8_t* body = (uint8_t*)malloc(head.length() + sz + tail.length());
    if (!body) { free(buf); return ""; }
    memcpy(body,                    head.c_str(), head.length());
    memcpy(body + head.length(),    buf,          sz);
    memcpy(body + head.length()+sz, tail.c_str(), tail.length());
    free(buf);

    int code = http.POST(body, head.length() + sz + tail.length());
    free(body);

    String result;
    if (code == 200) {
        // Handle chunked transfer
        WiFiClient* stream = http.getStreamPtr();
        String raw;
        uint32_t deadline = millis() + 30000;
        while (millis() < deadline) {
            if (stream->available()) {
                raw += (char)stream->read();
            } else if (!http.connected()) break;
            else delay(1);
        }
        StaticJsonDocument<2048> doc;
        if (!deserializeJson(doc, raw))
            result = doc["text"].as<String>();
    } else {
        Serial.printf("[network] Whisper error %d\n", code);
    }
    http.end();
    return result;
}

void networkDrainQueue() {
    char path[128];
    while (xQueueReceive(transcribeQueue, path, 0) == pdTRUE) {
        Serial.printf("[network] transcribing %s\n", path);
        String transcript = whisperTranscribe(path);
        if (transcript.length() == 0) continue;

        // TODO: enrichment + GitHub push (Phase 2)
        Serial.printf("[network] transcript: %s\n", transcript.c_str());
    }
}
