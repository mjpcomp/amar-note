#include "Arduino.h"
#include "../../config.h"
#include "config_store.h"
#include "WiFiClientSecure.h"
#include <ArduinoJson.h>
#include "ota.h"

extern const uint8_t x509_crt_bundle_start[] asm("_binary_x509_crt_bundle_start");
extern const uint8_t x509_crt_bundle_end[]   asm("_binary_x509_crt_bundle_end");

// Simple semver-style comparison: strips leading 'v', splits on '.', compares numerically.
// Returns true if remoteTag is strictly newer than localTag.
static bool isNewer(const String& remoteTag, const String& localTag) {
  auto parseVer = [](const String& tag, int& maj, int& min, int& pat) {
    String s = tag;
    if (s.startsWith("v") || s.startsWith("V")) s = s.substring(1);
    int d1 = s.indexOf('.');
    int d2 = (d1 >= 0) ? s.indexOf('.', d1 + 1) : -1;
    maj = s.substring(0, d1 >= 0 ? d1 : s.length()).toInt();
    min = (d1 >= 0) ? s.substring(d1 + 1, d2 >= 0 ? d2 : s.length()).toInt() : 0;
    pat = (d2 >= 0) ? s.substring(d2 + 1).toInt() : 0;
  };
  int rmaj, rmin, rpat, lmaj, lmin, lpat;
  parseVer(remoteTag, rmaj, rmin, rpat);
  parseVer(localTag,  lmaj, lmin, lpat);
  if (rmaj != lmaj) return rmaj > lmaj;
  if (rmin != lmin) return rmin > lmin;
  return rpat > lpat;
}

bool otaCheckGithub(String& latestTag, String& assetUrl) {
  const char* host = "api.github.com";
  // Build path: /repos/OWNER/REPO/releases/latest
  String path = "/repos/" OTA_GITHUB_OWNER "/" OTA_GITHUB_REPO "/releases/latest";

  WiFiClientSecure client;
  client.setCACertBundle(x509_crt_bundle_start,
                         (size_t)(x509_crt_bundle_end - x509_crt_bundle_start));
  client.setHandshakeTimeout(15);

  if (!client.connect(host, 443, 15000)) {
    Serial.println("[OTA] GitHub connect failed");
    return false;
  }

  // Build auth header — use stored GitHub token if available (needed for private repos)
  String authHeader = "";
  String tok = cfg::githubToken();
  if (tok.length() > 0) authHeader = "Authorization: Bearer " + tok + "\r\n";

  client.printf(
    "GET %s HTTP/1.1\r\n"
    "Host: %s\r\n"
    "User-Agent: AmarNote/" FIRMWARE_VERSION "\r\n"
    "Accept: application/vnd.github+json\r\n"
    "%s"
    "Connection: close\r\n\r\n",
    path.c_str(), host, authHeader.c_str()
  );

  uint32_t deadline = millis() + 15000;
  while (!client.available() && millis() < deadline) delay(20);

  // Skip HTTP headers
  bool inBody = false;
  String body = "";
  while (client.available() || (client.connected() && millis() < deadline)) {
    if (!client.available()) { delay(10); continue; }
    String line = client.readStringUntil('\n');
    if (!inBody) {
      if (line == "\r" || line.length() == 0) inBody = true;
      // Check for non-200
      if (line.startsWith("HTTP/") && line.indexOf(" 200 ") < 0) {
        Serial.printf("[OTA] GitHub API: %s\n", line.c_str());
        client.stop();
        return false;
      }
    } else {
      body += line;
      if (body.length() > 8192) break; // safety cap
    }
  }
  client.stop();

  if (body.length() == 0) {
    Serial.println("[OTA] empty response from GitHub");
    return false;
  }

  // Parse JSON — we only need tag_name and the first .bin asset browser_download_url
  DynamicJsonDocument doc(8192);
  DeserializationError err = deserializeJson(doc, body);
  if (err) {
    Serial.printf("[OTA] JSON parse error: %s\n", err.c_str());
    return false;
  }

  latestTag = doc["tag_name"] | "";
  if (latestTag.length() == 0) {
    Serial.println("[OTA] no tag_name in release");
    return false;
  }

  // Find first .bin asset
  JsonArray assets = doc["assets"].as<JsonArray>();
  for (JsonObject asset : assets) {
    String name = asset["name"] | "";
    if (name.endsWith(".bin")) {
      assetUrl = asset["browser_download_url"] | "";
      break;
    }
  }

  if (assetUrl.length() == 0) {
    Serial.printf("[OTA] release %s has no .bin asset\n", latestTag.c_str());
    return false;
  }

  Serial.printf("[OTA] latest=%s local=%s url=%s\n",
                latestTag.c_str(), FIRMWARE_VERSION, assetUrl.c_str());

  return isNewer(latestTag, FIRMWARE_VERSION);
}
