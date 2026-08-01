#pragma once
#include <Arduino.h>

namespace cfg {

void    begin();

// ── WiFi ────────────────────────────────────────────────────────────────────
String  wifiSsid();
String  wifiPass();
bool    hasWifi();
bool    setWifi(const String& ssid, const String& pass);

// ── OpenAI ──────────────────────────────────────────────────────────────────
String  openaiKey();
bool    hasOpenAiKey();
bool    setOpenAiKey(const String& key);

// ── Groq ────────────────────────────────────────────────────────────────────
String  groqKey();
bool    hasGroqKey();
bool    setGroqKey(const String& key);

// ── STT provider ────────────────────────────────────────────────────────────
// 0 = OpenAI whisper-1  |  1 = Groq whisper-large-v3-turbo
uint8_t sttProvider();
void    setSttProvider(uint8_t p);
bool    hasSttKey();

// ── AI enrichment provider + model ──────────────────────────────────────────
// enrichProvider(): 0 = OpenAI gpt-4o-mini  |  1 = Groq (model selectable)
// enrichModel():    one of the three validated Groq model IDs below.
//   "llama-3.3-70b-versatile"       – best quality,  30 RPM free
//   "llama-3.1-8b-instant"          – fastest/lightest, 30 RPM free
//   "llama-4-scout-17b-16e-instruct" – newer model,   15 RPM free
// When provider is OpenAI the model is always gpt-4o-mini (not configurable).
uint8_t enrichProvider();
String  enrichModel();
void    setEnrichProvider(uint8_t p);
void    setEnrichModel(const String& model);

// ── GitHub / Obsidian vault ──────────────────────────────────────────────────
String  githubToken();
String  githubRepo();
String  githubBranch();
String  githubDir();
bool    githubEnabled();
bool    githubAiEnrich();
bool    hasGithub();

bool    setGithubToken(const String& token);
bool    setGithubRepo(const String& ownerRepo);
bool    setGithubBranch(const String& branch);
bool    setGithubDir(const String& dir);
void    setGithubEnabled(bool on);
void    setGithubAiEnrich(bool on);

// ── Touch behaviour ──────────────────────────────────────────────────────────
// idleTouchRecord(): when true, tapping the idle screen starts a recording.
// Default false — prevents accidental pocket recordings.
bool    idleTouchRecord();
void    setIdleTouchRecord(bool on);

void    factoryReset();
void    resetWifi();   // erases ssid + pass only; leaves keys intact
void    resetKeys();   // erases all API/service keys; leaves WiFi intact

}  // namespace cfg
