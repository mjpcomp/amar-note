#include "Arduino.h"
#include "../../config.h"
#include "config_store.h"
#include "WiFiClientSecure.h"
#include <ArduinoJson.h>
#include <Update.h>
#include "ota.h"

extern const uint8_t x509_crt_bundle_start[] asm("_binary_x509_crt_bundle_start");
extern const uint8_t x509_crt_bundle_end[]   asm("_binary_x509_crt_bundle_end");

// ─── Helpers ─────────────────────────────────────────────────────────────────

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

// ─── otaCheckGithub ──────────────────────────────────────────────────────────
bool otaCheckGithub(String& latestTag, String& assetUrl, size_t& assetSize) {
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

  assetSize = 0;
  JsonArray assets = doc["assets"].as<JsonArray>();
  for (JsonObject asset : assets) {
    String name = asset["name"] | "";
    if (name.endsWith(".bin")) {
      assetUrl  = asset["browser_download_url"] | "";
      assetSize = (size_t)(asset["size"] | 0);
      break;
    }
  }

  if (assetUrl.length() == 0) {
    Serial.printf("[OTA] release %s has no .bin asset\n", latestTag.c_str());
    return false;
  }

  Serial.printf("[OTA] latest=%s local=%s\n[OTA] asset=%s (%u bytes)\n",
                latestTag.c_str(), FIRMWARE_VERSION, assetUrl.c_str(), (unsigned)assetSize);
  return isNewer(latestTag, FIRMWARE_VERSION);
}

// ─── streamToFlash ───────────────────────────────────────────────────────────────
//
// Streams bytes from `client` into the OTA partition.
//
// chunked=true  : server sent Transfer-Encoding: chunked (HTTP/1.1 default).
//   Each chunk is preceded by a hex size line + CRLF and followed by CRLF.
//   We strip those framing bytes before passing data to Update.write().
//   A chunk size of 0 signals end-of-body.
//
// chunked=false : server sent Content-Length with a plain body (HTTP/1.0
//   style or Connection: close without chunking). Read until close.
//
// `totalBytes` is the true firmware size from the GitHub API JSON — used
// only as the denominator for progressCb.
static bool streamToFlash(WiFiClientSecure& client, size_t totalBytes,
                           bool chunked,
                           void (*progressCb)(size_t, size_t),
                           uint32_t timeoutMs = 120000) {
  if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
    Serial.printf("[OTA] Update.begin failed: %s\n", Update.errorString());
    return false;
  }

  size_t written = 0;
  uint32_t deadline = millis() + timeoutMs;

  if (chunked) {
    // ── RFC 7230 §4.1 chunked decoding ────────────────────────────────────
    // Format: <hex-size>CRLF <data>CRLF  ...  0CRLF CRLF
    uint8_t buf[1024];
    while (millis() < deadline) {
      // Wait for at least one byte (chunk-size line)
      while (!client.available() && client.connected() && millis() < deadline) delay(5);
      if (!client.available()) break;

      // Read chunk-size line (hex digits, possibly followed by chunk-extension, then CRLF)
      String sizeLine = client.readStringUntil('\n');
      sizeLine.trim();
      // Strip any chunk-extension (";") before parsing
      int semi = sizeLine.indexOf(';');
      if (semi >= 0) sizeLine = sizeLine.substring(0, semi);
      size_t chunkSize = (size_t)strtoul(sizeLine.c_str(), nullptr, 16);

      if (chunkSize == 0) break;  // last-chunk

      // Read exactly chunkSize bytes
      size_t remaining = chunkSize;
      while (remaining > 0 && millis() < deadline) {
        while (!client.available() && client.connected() && millis() < deadline) delay(5);
        if (!client.available()) break;
        size_t toRead = remaining < sizeof(buf) ? remaining : sizeof(buf);
        int n = client.read(buf, toRead);
        if (n <= 0) continue;
        if (Update.write(buf, n) != (size_t)n) {
          Serial.printf("[OTA] write error: %s\n", Update.errorString());
          Update.abort(); return false;
        }
        written += n;
        remaining -= n;
        if (progressCb) progressCb(written, totalBytes > 0 ? totalBytes : written);
      }
      // Consume trailing CRLF after chunk data
      client.readStringUntil('\n');
    }
  } else {
    // ── Plain body: read until connection closes ───────────────────────────
    uint8_t buf[1024];
    while (client.available() || (client.connected() && millis() < deadline)) {
      if (!client.available()) { delay(5); continue; }
      int n = client.read(buf, sizeof(buf));
      if (n <= 0) continue;
      if (Update.write(buf, n) != (size_t)n) {
        Serial.printf("[OTA] write error: %s\n", Update.errorString());
        Update.abort(); return false;
      }
      written += n;
      if (progressCb) progressCb(written, totalBytes > 0 ? totalBytes : written);
    }
  }

  Serial.printf("[OTA] written %u bytes\n", (unsigned)written);

  if (!Update.end(true)) {
    Serial.printf("[OTA] Update.end failed: %s\n", Update.errorString());
    return false;
  }
  Serial.printf("[OTA] flash complete (%u bytes)\n", (unsigned)written);
  return true;
}

// ─── otaDownloadAndFlash ──────────────────────────────────────────────────────────
bool otaDownloadAndFlash(const String& browserDownloadUrl,
                         size_t assetSize,
                         void (*progressCb)(size_t, size_t)) {

  if (!browserDownloadUrl.startsWith("https://")) {
    Serial.println("[OTA] URL must be https");
    return false;
  }

  auto parseUrl = [](const String& url, String& host, String& path) {
    String rest = url.substring(8);
    int slash = rest.indexOf('/');
    host = (slash >= 0) ? rest.substring(0, slash) : rest;
    path = (slash >= 0) ? rest.substring(slash) : "/";
  };

  // Captures status, Content-Length, Location, and Transfer-Encoding
  auto readHeaders = [](WiFiClientSecure& c, int& code, size_t& clen,
                        String& loc, bool& isChunked, uint32_t timeoutMs) {
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
      } else {
        // Case-insensitive header matching
        String lline = line;
        lline.toLowerCase();
        if (lline.startsWith("content-length:")) {
          clen = (size_t)line.substring(line.indexOf(':') + 1).toInt();
        } else if (lline.startsWith("location:") && loc.length() == 0) {
          loc = line.substring(line.indexOf(':') + 1);
          loc.trim();
        } else if (lline.startsWith("transfer-encoding:")) {
          String te = line.substring(line.indexOf(':') + 1);
          te.trim(); te.toLowerCase();
          if (te == "chunked") isChunked = true;
        }
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
  bool isChunked = false;
  readHeaders(client, httpCode, contentLength, location, isChunked, 20000);
  Serial.printf("[OTA] HTTP %d  content-length=%u  asset-size=%u  chunked=%d\n",
                httpCode, (unsigned)contentLength, (unsigned)assetSize, (int)isChunked);

  // ── Follow one redirect ──────────────────────────────────────────────────────
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

    httpCode = 0; contentLength = 0; location = ""; isChunked = false;
    readHeaders(c2, httpCode, contentLength, location, isChunked, 20000);
    Serial.printf("[OTA] CDN HTTP %d  size=%u  chunked=%d\n",
                  httpCode, (unsigned)contentLength, (int)isChunked);

    if (httpCode != 200) {
      Serial.printf("[OTA] CDN error %d\n", httpCode); c2.stop(); return false;
    }

    bool ok = streamToFlash(c2, assetSize, isChunked, progressCb);
    c2.stop();
    return ok;
  }

  // ── Direct 200 (no redirect) ────────────────────────────────────────────
  if (httpCode != 200) {
    Serial.printf("[OTA] unexpected HTTP %d\n", httpCode);
    client.stop(); return false;
  }

  bool ok = streamToFlash(client, assetSize, isChunked, progressCb);
  client.stop();
  return ok;
}
