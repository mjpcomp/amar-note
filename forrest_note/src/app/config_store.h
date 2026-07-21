#pragma once
#include <Arduino.h>

// Runtime configuration / secrets, persisted in NVS (the `nvs` partition) rather
// than baked into the app image. Keeps Wi-Fi and API credentials out of the
// firmware binary that gets flashed/shared. Provisioned at runtime over the
// setup portal (SoftAP) or seeded once from secrets.h for backward compat.
namespace cfg {
  void   begin();                                       // load NVS, one-time seed from secrets.h
  String wifiSsid();
  String wifiPass();
  String openaiKey();
  String groqKey();                                     // Groq free-tier STT key (gsk_...)
  uint8_t sttProvider();                                // 0 = OpenAI Whisper, 1 = Groq
  bool   hasWifi();                                     // Wi-Fi credentials present
  bool   hasOpenAiKey();                                // OpenAI key present
  bool   hasGroqKey();                                  // Groq key present
  bool   hasSttKey();                                   // any STT key present (OpenAI or Groq)
  bool   setWifi(const String& ssid, const String& pass);
  bool   setOpenAiKey(const String& key);
  bool   setGroqKey(const String& key);
  void   setSttProvider(uint8_t p);                     // 0=OpenAI 1=Groq

  // GitHub / Obsidian vault sync
  String githubToken();
  String githubRepo();                                  // "owner/repo"
  String githubBranch();                                // default "main"
  String githubDir();                                   // vault subfolder, default "VoiceNotes"
  bool   githubEnabled();                               // master on/off
  bool   githubAiEnrich();                              // AI title/topics, default on
  bool   hasGithub();                                   // enabled + token + valid "owner/repo"
  bool   setGithubToken(const String& token);
  bool   setGithubRepo(const String& ownerRepo);        // validates one '/'
  bool   setGithubBranch(const String& branch);
  bool   setGithubDir(const String& dir);
  void   setGithubEnabled(bool on);
  void   setGithubAiEnrich(bool on);

  void   factoryReset();                                // wipe all stored config
}
