#include "config_store.h"
#include <Preferences.h>
#include <string.h>
#include <stdio.h>

// ============================================================
// config_store — Amar Note NVS-backed settings
// All keys live in the NVS namespace defined by NVS_NAMESPACE ("amar").
// ============================================================

static Preferences prefs;

void configLoad() {
    prefs.begin(NVS_NAMESPACE, false);
}

void configSave() {
    // Preferences auto-commits on each put; this is a no-op hook for callers.
}

// Wi-Fi
String configGetSSID()     { return prefs.getString("wifi_ssid", ""); }
String configGetPass()     { return prefs.getString("wifi_pass", ""); }
void   configSetSSID(const String& v) { prefs.putString("wifi_ssid", v); }
void   configSetPass(const String& v) { prefs.putString("wifi_pass", v); }

// OpenAI
String configGetOpenAIKey()                { return prefs.getString("openai_key", ""); }
void   configSetOpenAIKey(const String& v) { prefs.putString("openai_key", v); }

// Groq free-tier STT
String configGetGroqKey()                { return prefs.getString("groq_key", ""); }
void   configSetGroqKey(const String& v) { prefs.putString("groq_key", v); }

// STT provider: 0 = OpenAI Whisper, 1 = Groq Whisper
uint8_t configGetSttProvider()               { return (uint8_t)prefs.getUChar("stt_provider", 0); }
void    configSetSttProvider(uint8_t v)      { prefs.putUChar("stt_provider", v); }

// GitHub
String configGetGHRepo()    { return prefs.getString("gh_repo",   ""); }
String configGetGHBranch()  { return prefs.getString("gh_branch", "main"); }
String configGetGHDir()     { return prefs.getString("gh_dir",    "VoiceNotes"); }
String configGetGHToken()   { return prefs.getString("gh_token",  ""); }
bool   configGetGHEnabled() { return prefs.getBool("gh_enabled", false); }
bool   configGetAIEnrich()  { return prefs.getBool("ai_enrich",  false); }

void configSetGHRepo(const String& v)   { prefs.putString("gh_repo",   v); }
void configSetGHBranch(const String& v) { prefs.putString("gh_branch", v); }
void configSetGHDir(const String& v)    { prefs.putString("gh_dir",    v); }
void configSetGHToken(const String& v)  { prefs.putString("gh_token",  v); }
void configSetGHEnabled(bool v)         { prefs.putBool("gh_enabled",  v); }
void configSetAIEnrich(bool v)          { prefs.putBool("ai_enrich",   v); }

// Convenience: did the user complete first-time setup?
bool configIsProvisioned() {
    return configGetSSID().length() > 0 &&
           (configGetOpenAIKey().length() > 0 || configGetGroqKey().length() > 0);
}
