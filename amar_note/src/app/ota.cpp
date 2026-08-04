#include "Arduino.h"
#include "../../config.h"
#include "config_store.h"
#include "WiFiClientSecure.h"
#include <ArduinoJson.h>
#include <Update.h>
#include "ota.h"

extern const uint8_t x509_crt_bundle_start[] asm("_binary_x509_crt_bundle_start");
extern const uint8_t x509_crt_bundle_end[]   asm("_binary_x509_crt_bundle_end");

// ─── Helpers ──────────────────────────────────────────────────────────────────

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

// ─── otaCheckGithub ───────────────────────────────────────────────────────────
bool otaCheckGithub(String& latestTag, String& assetUrl) {
  const char* host = "api.github.com";
  String path = "/repos/" OTA_GITHUB_OWNER "/" OTA_GITHUB_REPO "/releases/latest";

  WiFiClientSecure client;
  client.setCACertBundle(x509_crt_bundle_start,
                         (size_t)(x509_crt_bundle_end - x509_crt_bundle_start));
  client.setHandshakeTimeout(15);

  if (!client.connect(host, 443, 15000)) {
    Serial.println("[OTA] GitHub connect failed");
    return false;
  }

  // Token is optional — used only to avoid GitHub API rate-limits (60 req/hr
  // unauthenticated vs 5000/hr authenticated). Not required for public repos.
  String tok = cfg::githubToken();
  String authHeader = (tok.length() > 0) ? "Authorization: Bearer " + tok + "\r\n" : "";

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

  bool inBody = false;
  String body = "";
  while (client.available() || (client.connected() && millis() < deadline)) {
    if (!client.available()) { delay(10); continue; }
    String line = client.readStringUntil('\n');
    if (!inBody) {
      if (line == "\r" || line.length() == 0) inBody = true;
      if (line.startsWith("HTTP/") && line.indexOf(" 200 ") < 0) {
        Serial.printf("[OTA] GitHub API: %s\n", line.c_str());
        client.stop(); return false;
      }
    } else {
      body += line;
      if (body.length() > 8192) break;
    }
  }
  client.stop();

  if (body.length() == 0) { Serial.println("[OTA] empty GitHub response"); return false; }

  DynamicJsonDocument doc(8192);
  if (deserializeJson(doc, body)) { Serial.println("[OTA] JSON parse error"); return false; }

  latestTag = doc["tag_name"] | "";
  if (latestTag.length() == 0) { Serial.println("[OTA] no tag_name"); return false; }

  // Use browser_download_url — the direct github.com CDN URL.
  // This works on public repos with no auth, unlike the API asset URL.
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

  Serial.printf("[OTA] latest=%s local=%s\n[OTA] asset=%s\n",
                latestTag.c_str(), FIRMWARE_VERSION, assetUrl.c_str());
  return isNewer(latestTag, FIRMWARE_VERSION);
}

// ─── flash helper ─────────────────────────────────────────────────────────────
// Streams bytes from `client` into the OTA partition.
// Uses UPDATE_SIZE_UNKNOWN so Update.begin() sizes itself against the actual
// partition — avoids "Bad Size Given" when Content-Length equals the full
// flash capacity rather than the firmware binary size.
static bool streamToFlash(WiFiClientSecure& client, size_t contentLength,
                           void (*progressCb)(size_t, size_t),
                           uint32_t timeoutMs = 120000) {
  if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
    Serial.printf("[OTA] Update.begin failed: %s\n", Update.errorString());
    return false;
  }

  uint8_t buf[1024];
  size_t written = 0;
  uint32_t deadline = millis() + timeoutMs;

  while (client.available() || (client.connected() && millis() < deadline)) {
    if (!client.available()) { delay(5); continue; }
    int n = client.read(buf, sizeof(buf));
    if (n <= 0) continue;
    if (Update.write(buf, n) != (size_t)n) {
      Serial.printf("[OTA] write error: %s\n", Update.errorString());
      Update.abort();
      return false;
    }
    written += n;
    if (progressCb) progressCb(written, contentLength);
  }

  if (!Update.end(true)) {
    Serial.printf("[OTA] Update.end failed: %s\n", Update.errorString());
    return false;
  }
  Serial.printf("[OTA] flash complete (%u bytes)\n", (unsigned)written);
  return true;
}

// ─── otaDownloadAndFlash ──────────────────────────────────────────────────────
//
// Downloads firmware from a browser_download_url (github.com release CDN).
// No auth required for public repos. GitHub redirects github.com/releases/...
// to release-assets.githubusercontent.com with a short-lived signed URL.
// We follow that single 302 and stream the binary directly into the inactive
// OTA partition via Update.h.
bool otaDownloadAndFlash(const String& browserDownloadUrl,
                         void (*progressCb)(size_t, size_t)) {

  if (!browserDownloadUrl.startsWith("https://")) {
    Serial.println("[OTA] URL must be https");
    return false;
  }

  auto parseUrl = [](const String& url, String& host, String& path) {
    String rest = url.substring(8); // strip "https://"
    int slash = rest.indexOf('/');
    host = (slash >= 0) ? rest.substring(0, slash) : rest;
    path = (slash >= 0) ? rest.substring(slash) : "/";
  };

  // ── helper: read HTTP response headers from a connected client ────────────
  auto readHeaders = [](WiFiClientSecure& c, int& code, size_t& clen, String& loc,
                        uint32_t timeoutMs) {
    uint32_t deadline = millis() + timeoutMs;
    while (!c.available() && millis() < deadline) delay(20);
    while (c.available() || (c.connected() && millis() < deadline)) {
      if (!c.available()) { delay(5); continue; }
      String line = c.readStringUntil('\n');
      line.trim();
      if (line.length() == 0) break;
      if (line.startsWith("HTTP/")) {
        int s1 = line.indexOf(' '), s2 = line.indexOf(' ', s1 + 1);
        code = line.substring(s1 + 1, s2 > 0 ? s2 : line.length()).toInt();
      } else if (line.startsWith("Content-Length:") || line.startsWith("content-length:")) {
        clen = (size_t)line.substring(line.indexOf(':') + 1).toInt();
      } else if ((line.startsWith("Location:") || line.startsWith("location:")) && loc.length() == 0) {
        loc = line.substring(line.indexOf(':') + 1);
        loc.trim();
      }
    }
  };

  String host, path;
  parseUrl(browserDownloadUrl, host, path);

  WiFiClientSecure client;
  client.setCACertBundle(x509_crt_bundle_start,
                         (size_t)(x509_crt_bundle_end - x509_crt_bundle_start));
  client.setHandshakeTimeout(20);

  Serial.printf("[OTA] connecting to %s\n", host.c_str());
  if (!client.connect(host.c_str(), 443, 20000)) {
    Serial.printf("[OTA] connect failed: %s\n", host.c_str());
    return false;
  }

  client.printf(
    "GET %s HTTP/1.1\r\n"
    "Host: %s\r\n"
    "User-Agent: AmarNote/" FIRMWARE_VERSION "\r\n"
    "Connection: close\r\n\r\n",
    path.c_str(), host.c_str()
  );

  int httpCode = 0;
  size_t contentLength = 0;
  String location;
  readHeaders(client, httpCode, contentLength, location, 20000);
  Serial.printf("[OTA] HTTP %d  content-length=%u\n", httpCode, (unsigned)contentLength);

  // ── Follow one redirect ───────────────────────────────────────────────────
  if (httpCode >= 300 && httpCode < 400) {
    client.stop();
    if (location.length() == 0) { Serial.println("[OTA] redirect with no Location"); return false; }
    if (!location.startsWith("https://")) { Serial.println("[OTA] non-HTTPS redirect"); return false; }

    Serial.printf("[OTA] following redirect to: %s\n", location.c_str());

    String rHost, rPath;
    parseUrl(location, rHost, rPath);

    WiFiClientSecure c2;
    c2.setCACertBundle(x509_crt_bundle_start,
                       (size_t)(x509_crt_bundle_end - x509_crt_bundle_start));
    c2.setHandshakeTimeout(20);
    if (!c2.connect(rHost.c_str(), 443, 20000)) {
      Serial.printf("[OTA] redirect connect failed: %s\n", rHost.c_str()); return false;
    }
    c2.printf(
      "GET %s HTTP/1.1\r\n"
      "Host: %s\r\n"
      "User-Agent: AmarNote/" FIRMWARE_VERSION "\r\n"
      "Connection: close\r\n\r\n",
      rPath.c_str(), rHost.c_str()
    );

    httpCode = 0; contentLength = 0; location = "";
    readHeaders(c2, httpCode, contentLength, location, 20000);
    Serial.printf("[OTA] CDN HTTP %d  size=%u\n", httpCode, (unsigned)contentLength);

    if (httpCode != 200) {
      Serial.printf("[OTA] CDN error %d\n", httpCode); c2.stop(); return false;
    }

    bool ok = streamToFlash(c2, contentLength, progressCb);
    c2.stop();
    return ok;
  }

  // ── Direct 200 (no redirect) ──────────────────────────────────────────────
  if (httpCode != 200) {
    Serial.printf("[OTA] unexpected HTTP %d\n", httpCode);
    client.stop(); return false;
  }

  bool ok = streamToFlash(client, contentLength, progressCb);
  client.stop();
  return ok;
}
