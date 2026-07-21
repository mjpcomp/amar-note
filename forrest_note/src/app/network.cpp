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
// network.cpp — Amar Note Wi-Fi, captive portal, OTA
// ============================================================
// NOTE: Voice transcription (Whisper / Groq) now lives entirely in
// obsidian.cpp which owns the note-sync pipeline. The old dead-code
// whisperTranscribe / networkDrainQueue path has been removed.
// ============================================================

static WebServer server(80);
static bool      wifiConnected = false;

// --- Mozilla CA bundle ---
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
*{box-sizing:border-box}
body{font-family:system-ui,sans-serif;max-width:520px;margin:2rem auto;padding:0 1rem 3rem;background:#f7f6f2;color:#28251d}
h1{font-size:1.4rem;margin-bottom:.3rem}
.tagline{font-size:.85rem;color:#7a7974;margin-bottom:1.8rem}
/* sections */
.section{background:#fff;border:1px solid #dcd9d5;border-radius:10px;padding:1.2rem 1.2rem 1rem;margin-bottom:1.2rem}
.section-title{font-size:.7rem;font-weight:700;letter-spacing:.08em;text-transform:uppercase;color:#7a7974;margin-bottom:.9rem}
/* labels / inputs */
label{display:block;font-size:.85rem;margin:.8rem 0 .2rem;font-weight:500}
label span.opt{font-weight:400;color:#7a7974;margin-left:.3rem;font-size:.8rem}
input[type=text],input[type=password],select{width:100%;padding:.5rem .6rem;
  border:1px solid #d4d1ca;border-radius:6px;font-size:.95rem;background:#fff;color:#28251d}
input:focus,select:focus{outline:2px solid #01696f;border-color:#01696f}
/* help text */
.help{font-size:.78rem;color:#7a7974;margin:.25rem 0 0;line-height:1.45}
.help a{color:#01696f;text-decoration:none}
.help a:hover{text-decoration:underline}
/* checkbox rows */
.cb{display:flex;align-items:center;gap:.5rem;margin:.7rem 0}
.cb label{margin:0;font-weight:400}
/* submit */
.submit-wrap{margin-top:1.5rem}
button[type=submit]{width:100%;padding:.75rem;background:#01696f;color:#fff;
  border:none;border-radius:8px;font-size:1rem;font-weight:600;cursor:pointer}
button[type=submit]:hover{background:#0c4e54}
.footer-note{font-size:.78rem;color:#7a7974;text-align:center;margin-top:.8rem}
/* provider badge */
.badge{display:inline-block;font-size:.72rem;font-weight:600;padding:.1rem .45rem;
  border-radius:4px;margin-left:.4rem;vertical-align:middle}
.badge-free{background:#d4efdd;color:#1a6b38}
.badge-paid{background:#fde8cc;color:#7a3a00}
</style>
</head>
<body>
<h1>&#127897; Amar Note &mdash; Setup</h1>
<p class="tagline">Connect to your phone&rsquo;s Wi-Fi and configure your cloud services below.</p>

<form method="POST" action="/provision">

<!-- ── Wi-Fi ── -->
<div class="section">
  <div class="section-title">Wi-Fi</div>
  <label>Network name (SSID)
    <span class="opt">2.4 GHz only</span>
  </label>
  <input type="text" name="ssid" placeholder="Your Wi-Fi name" autocomplete="off">
  <label>Password</label>
  <input type="password" name="pass" placeholder="Leave blank to keep current" autocomplete="off">
  <p class="help">Amar Note connects only to 2.4 GHz networks. 5 GHz SSIDs will not work.</p>
</div>

<!-- ── Speech-to-Text ── -->
<div class="section">
  <div class="section-title">Speech-to-Text (Transcription)</div>

  <label>Provider</label>
  <select name="stt_provider" id="stt_provider" onchange="onProviderChange()">
    <option value="0">OpenAI Whisper <span class="badge badge-paid">Paid</span></option>
    <option value="1">Groq Whisper &mdash; free tier <span class="badge badge-free">Free</span></option>
  </select>
  <p class="help" id="help_stt_openai">
    OpenAI charges ~$0.006 per minute of audio.
    <a href="https://platform.openai.com/signup" target="_blank" rel="noopener">Create an OpenAI account</a>,
    then go to <a href="https://platform.openai.com/api-keys" target="_blank" rel="noopener">API Keys</a>
    to generate a key starting with <code>sk-</code>.
  </p>
  <p class="help" id="help_stt_groq" style="display:none">
    Groq is <strong>free</strong> (up to 2 hrs of audio per day) and faster than OpenAI.
    <a href="https://console.groq.com/keys" target="_blank" rel="noopener">Create a free Groq account</a>
    and generate a key starting with <code>gsk_</code>.
  </p>

  <div id="wrap_openai_key">
    <label>OpenAI API key <span class="badge badge-paid">Paid</span></label>
    <input type="text" name="openai" placeholder="sk-..." autocomplete="off">
    <p class="help">
      <a href="https://platform.openai.com/api-keys" target="_blank" rel="noopener">platform.openai.com/api-keys</a>
    </p>
  </div>

  <div id="wrap_groq_key" style="display:none">
    <label>Groq API key <span class="badge badge-free">Free</span></label>
    <input type="text" name="groq" placeholder="gsk_..." autocomplete="off">
    <p class="help">
      <a href="https://console.groq.com/keys" target="_blank" rel="noopener">console.groq.com/keys</a>
      &mdash; Model used: <code>whisper-large-v3-turbo</code>
    </p>
  </div>

  <div class="cb">
    <input type="checkbox" name="ai_en" id="ai_en" checked>
    <label for="ai_en">AI titles &amp; topic links (uses OpenAI GPT-4o-mini &mdash; separate from transcription)</label>
  </div>
  <p class="help">
    AI enrichment always uses OpenAI regardless of the transcription provider.
    If you&rsquo;re using Groq for transcription you still need an OpenAI key for this feature,
    or uncheck it above.
  </p>
</div>

<!-- ── GitHub ── -->
<div class="section">
  <div class="section-title">GitHub / Obsidian Vault Sync <span class="opt" style="text-transform:none;letter-spacing:0">(optional)</span></div>

  <div class="cb">
    <input type="checkbox" name="gh_en" id="gh_en" checked>
    <label for="gh_en">Enable GitHub sync</label>
  </div>

  <p class="help">
    Notes are pushed as Markdown files to a GitHub repo so Obsidian can read them.
    You need a free GitHub account and a Personal Access Token (PAT) with
    <strong>repo</strong> scope.
    <a href="https://github.com/signup" target="_blank" rel="noopener">Create a GitHub account</a>
    &mdash;
    <a href="https://github.com/settings/tokens/new?description=AmarNote&scopes=repo" target="_blank" rel="noopener">Generate a PAT</a>
    (select the <code>repo</code> checkbox, then copy the token starting with <code>github_pat_</code>).
  </p>

  <label>Repo <span class="opt">owner/name</span></label>
  <input type="text" name="repo" placeholder="yourname/Notes" autocomplete="off">
  <p class="help">Create a <strong>private</strong> repo first at
    <a href="https://github.com/new" target="_blank" rel="noopener">github.com/new</a>.
  </p>

  <label>Branch <span class="opt">default: main</span></label>
  <input type="text" name="branch" placeholder="main">

  <label>Vault folder <span class="opt">subfolder inside the repo</span></label>
  <input type="text" name="dir" placeholder="VoiceNotes">

  <label>GitHub Personal Access Token</label>
  <input type="text" name="token" placeholder="github_pat_..." autocomplete="off">
  <p class="help">
    <a href="https://github.com/settings/tokens/new?description=AmarNote&scopes=repo" target="_blank" rel="noopener">github.com &rarr; Settings &rarr; Developer settings &rarr; Personal access tokens</a>
  </p>
</div>

<div class="submit-wrap">
  <button type="submit">Save &amp; reboot</button>
  <p class="footer-note">Amar Note will restart and connect using the saved settings. Leave any field blank to keep its current value.</p>
</div>

</form>

<script>
function onProviderChange() {
  var v = document.getElementById('stt_provider').value;
  var isGroq = v === '1';
  document.getElementById('wrap_openai_key').style.display = isGroq ? 'none' : '';
  document.getElementById('wrap_groq_key').style.display   = isGroq ? '' : 'none';
  document.getElementById('help_stt_openai').style.display = isGroq ? 'none' : '';
  document.getElementById('help_stt_groq').style.display   = isGroq ? '' : 'none';
}
</script>

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
    if (arg("groq").length())   configSetGroqKey(arg("groq"));
    configSetSttProvider((uint8_t)arg("stt_provider").toInt());
    if (arg("repo").length())   configSetGHRepo(arg("repo"));
    if (arg("branch").length()) configSetGHBranch(arg("branch"));
    if (arg("dir").length())    configSetGHDir(arg("dir"));
    if (arg("token").length())  configSetGHToken(arg("token"));
    configSetGHEnabled(server.hasArg("gh_en"));
    configSetAIEnrich(server.hasArg("ai_en"));

    server.send(200, "text/html",
        "<!DOCTYPE html><html><head><meta charset='utf-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>Saved</title>"
        "<style>body{font-family:system-ui,sans-serif;max-width:420px;margin:3rem auto;"
        "padding:0 1rem;background:#f7f6f2;color:#28251d;text-align:center}"
        "h2{color:#01696f}p{color:#7a7974;margin-top:.5rem}</style></head>"
        "<body><h2>&#10003; Settings saved</h2>"
        "<p>Amar Note is rebooting and will connect to your Wi-Fi&hellip;</p>"
        "</body></html>");
    delay(800);
    ESP.restart();
}

// ---- Wi-Fi STA ----

bool networkConnectWifi() {
    String ssid = configGetSSID();
    String pass = configGetPass();
    if (!ssid.length()) return false;
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());
    uint32_t t = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t < 15000) delay(200);
    wifiConnected = (WiFi.status() == WL_CONNECTED);
    if (wifiConnected) Serial.printf("[net] WiFi connected: %s\n", WiFi.localIP().toString().c_str());
    else               Serial.println("[net] WiFi connect failed");
    return wifiConnected;
}

// ---- SoftAP captive portal ----

void networkStartPortal() {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(SETUP_SSID);
    server.on("/",          HTTP_GET,  handleRoot);
    server.on("/provision", HTTP_POST, handleProvision);
    server.onNotFound([](){ server.sendHeader("Location","/"); server.send(302); });
    server.begin();
    Serial.printf("[net] Portal started: SSID=%s IP=%s\n",
                  SETUP_SSID, WiFi.softAPIP().toString().c_str());
}

void networkLoop() {
    server.handleClient();
}

bool networkIsConnected() { return wifiConnected; }
