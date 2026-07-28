#include "config_store.h"
#include <Preferences.h>
#include <string.h>
#include "../../secrets.h"

namespace {
  Preferences prefs;
  const char* NS = "amar";

  // secrets.h ships with "...." placeholders; treat all-dots or empty as unset.
  bool isPlaceholder(const char* s) {
    if (!s || s[0] == '\0') return true;
    return strspn(s, ".") == strlen(s);
  }

  // Validated Groq model IDs for AI enrichment.
  // Keep in sync with the portal dropdown in network.cpp.
  const char* const GROQ_ENRICH_MODELS[] = {
    "llama-3.3-70b-versatile",
    "llama-3.1-8b-instant",
    "llama-4-scout-17b-16e-instruct"
  };
  const int GROQ_ENRICH_MODEL_COUNT = 3;
  const char* GROQ_ENRICH_DEFAULT   = "llama-3.3-70b-versatile";
}

namespace cfg {

void begin() {
  prefs.begin(NS, false);

  // One-time migration: seed NVS from compiled secrets.h only for real
  // (non-placeholder) values that aren't already stored.
  if (!prefs.isKey("ssid") && !isPlaceholder(WIFI_SSID)) {
    prefs.putString("ssid", WIFI_SSID);
    prefs.putString("pass", WIFI_PASS);
  }
  if (!prefs.isKey("oaikey") && !isPlaceholder(OPENAI_KEY)) {
    prefs.putString("oaikey", OPENAI_KEY);
  }

  // cfgv=1: force AI-enrich back on if it was ever inadvertently cleared.
  if (!prefs.isKey("cfgv")) {
    prefs.putBool("ghai", true);
    prefs.putUInt("cfgv", 1);
  }

  // cfgv=2: auto-default enrichment provider.
  // If a Groq key already exists and no enrichment provider has been chosen
  // yet, pre-select Groq so existing Groq users get the free path immediately.
  if (!prefs.isKey("enrichprov")) {
    uint8_t def = (groqKey().length() > 0) ? 1 : 0;
    prefs.putUChar("enrichprov", def);
  }
  if (!prefs.isKey("enrichmdl")) {
    prefs.putString("enrichmdl", GROQ_ENRICH_DEFAULT);
  }
}

String  wifiSsid()  { return prefs.getString("ssid",    ""); }
String  wifiPass()  { return prefs.getString("pass",    ""); }
String  openaiKey() { return prefs.getString("oaikey",  ""); }
String  groqKey()   { return prefs.getString("groqkey", ""); }

uint8_t sttProvider() { return (uint8_t)prefs.getUChar("sttprov", 0); }

bool hasWifi()      { return wifiSsid().length() > 0; }
bool hasOpenAiKey() { return openaiKey().length() > 0; }
bool hasGroqKey()   { return groqKey().length() > 0; }
bool hasSttKey()    { return hasOpenAiKey() || hasGroqKey(); }

bool setWifi(const String& ssid, const String& pass) {
  if (ssid.length() == 0) return false;
  prefs.putString("ssid", ssid);
  prefs.putString("pass", pass);
  return true;
}

bool setOpenAiKey(const String& key) {
  prefs.putString("oaikey", key);
  return true;
}

bool setGroqKey(const String& key) {
  prefs.putString("groqkey", key);
  return true;
}

void setSttProvider(uint8_t p) { prefs.putUChar("sttprov", p); }

// ── AI enrichment ────────────────────────────────────────────────────────────

uint8_t enrichProvider() {
  return (uint8_t)prefs.getUChar("enrichprov", 0);
}

String enrichModel() {
  String m = prefs.getString("enrichmdl", GROQ_ENRICH_DEFAULT);
  // Validate — reject anything not in the allowlist.
  for (int i = 0; i < GROQ_ENRICH_MODEL_COUNT; i++)
    if (m == GROQ_ENRICH_MODELS[i]) return m;
  return String(GROQ_ENRICH_DEFAULT);
}

void setEnrichProvider(uint8_t p) { prefs.putUChar("enrichprov", p); }

void setEnrichModel(const String& model) {
  // Only store validated model IDs.
  for (int i = 0; i < GROQ_ENRICH_MODEL_COUNT; i++) {
    if (model == GROQ_ENRICH_MODELS[i]) {
      prefs.putString("enrichmdl", model);
      return;
    }
  }
  // Silently ignore unknown values.
}

// ── GitHub / Obsidian vault ──────────────────────────────────────────────────

String githubToken()  { return prefs.getString("ghtok",    ""); }
String githubRepo()   { return prefs.getString("ghrepo",   ""); }
String githubBranch() { String b = prefs.getString("ghbranch", ""); return b.length() ? b : "main"; }
String githubDir()    { String d = prefs.getString("ghdir",    ""); return d.length() ? d : "VoiceNotes"; }
bool   githubEnabled()  { return prefs.getBool("ghon", false); }
bool   githubAiEnrich() { return prefs.getBool("ghai", true);  }

bool hasGithub() {
  return githubEnabled() && githubToken().length() > 0 && githubRepo().indexOf('/') > 0;
}

bool setGithubToken(const String& token) { prefs.putString("ghtok", token); return true; }

bool setGithubRepo(const String& ownerRepo) {
  String r = ownerRepo; r.trim();
  while (r.endsWith("/")) r.remove(r.length() - 1);
  if (r.indexOf('/') <= 0 || r.indexOf('/') != r.lastIndexOf('/')) return false;
  prefs.putString("ghrepo", r);
  return true;
}

bool setGithubBranch(const String& branch) {
  String b = branch; b.trim();
  prefs.putString("ghbranch", b);
  return true;
}

bool setGithubDir(const String& dir) {
  String d = dir; d.trim();
  while (d.endsWith("/"))   d.remove(d.length() - 1);
  while (d.startsWith("/")) d.remove(0, 1);
  prefs.putString("ghdir", d);
  return true;
}

void setGithubEnabled(bool on)  { prefs.putBool("ghon", on); }
void setGithubAiEnrich(bool on) { prefs.putBool("ghai", on); }

// ── Touch behaviour ──────────────────────────────────────────────────────────

bool idleTouchRecord()         { return prefs.getBool("idlerec", false); }
void setIdleTouchRecord(bool on) { prefs.putBool("idlerec", on); }

void factoryReset() { prefs.clear(); }

}  // namespace cfg
