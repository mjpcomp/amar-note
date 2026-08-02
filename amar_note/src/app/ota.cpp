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

// Parse host, path (and optional port) out of an https:// URL.
// Returns false if the URL doesn't look like https://.
static bool parseHttpsUrl(const String& url, String& host, int& port, String& path) {
  if (!url.startsWith("https://")) return false;
  String rest = url.substring(8); // strip "https://"
  int slash = rest.indexOf('/');
  String hostPort = (slash >= 0) ? rest.substring(0, slash) : rest;
  path = (slash >= 0) ? rest.substring(slash) : "/";
  int colon = hostPort.indexOf(':');
  if (colon >= 0) {
    host = hostPort.substring(0, colon);
    port = hostPort.substring(colon + 1).toInt();
  } else {
    host = hostPort;
    port = 443;
  }
  return host.length() > 0;
}

// Follow a single HTTPS redirect (one hop only — GitHub release asset
// browser_download_url always redirects once to objects.githubusercontent.com).
// Returns the Location header value on a 3xx, or the original url on 200,
// or empty string on error.
static String resolveRedirect(const String& url) {
  String host, path;
  int port = 443;
  if (!parseHttpsUrl(url, host, path, port)) {
    Serial.printf("[OTA] resolveRedirect: bad url: %s\n", url.c_str());
    return "";
  }

  WiFiClientSecure client;
  client.setCACertBundle(x509_crt_bundle_start,
                         (size_t)(x509_crt_bundle_end - x509_crt_bundle_start));
  client.setHandshakeTimeout(15);

  if (!client.connect(host.c_str(), port, 10000)) {
    Serial.printf("[OTA] resolveRedirect: connect failed to %s\n", host.c_str());
    return "";
  }

  // Use HEAD so we don't pull the whole binary just to get the redirect.
  client.printf(
    "HEAD %s HTTP/1.1\r\n"
    "Host: %s\r\n"
    "User-Agent: AmarNote/" FIRMWARE_VERSION "\r\n"
    "Connection: close\r\n\r\n",
    path.c_str(), host.c_str()
  );

  uint32_t deadline = millis() + 10000;
  while (!client.available() && millis() < deadline) delay(20);

  String location = "";
  bool is3xx = false;
  while (client.available() || (client.connected() && millis() < deadline)) {
    if (!client.available()) { delay(5); continue; }
    String line = client.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) break; // end of headers
    if (line.startsWith("HTTP/")) {
      // e.g. "HTTP/1.1 302 Found"
      int sp1 = line.indexOf(' ');
      int sp2 = line.indexOf(' ', sp1 + 1);
      int code = line.substring(sp1 + 1, sp2 > 0 ? sp2 : line.length()).toInt();
      is3xx = (code >= 300 && code < 400);
      if (code == 200) {
        // No redirect — return the original URL as-is.
        client.stop();
        return url;
      }
      if (!is3xx) {
        Serial.printf("[OTA] resolveRedirect: unexpected status %d\n", code);
        client.stop();
        return "";
      }
    } else if (is3xx) {
      if (line.startsWith("Location:") || line.startsWith("location:")) {
        location = line.substring(line.indexOf(':') + 1);
        location.trim();
      }
    }
  }
  client.stop();

  if (location.length() > 0) {
    Serial.printf("[OTA] resolved redirect -> %s\n", location.c_str());
    return location;
  }
  Serial.println("[OTA] resolveRedirect: no Location header found");
  return "";
}

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

  bool inBody = false;
  String body = "";
  while (client.available() || (client.connected() && millis() < deadline)) {
    if (!client.available()) { delay(10); continue; }
    String line = client.readStringUntil('\n');
    if (!inBody) {
      if (line == "\r" || line.length() == 0) inBody = true;
      if (line.startsWith("HTTP/") && line.indexOf(" 200 ") < 0) {
        Serial.printf("[OTA] GitHub API: %s\n", line.c_str());
        client.stop();
        return false;
      }
    } else {
      body += line;
      if (body.length() > 8192) break;
    }
  }
  client.stop();

  if (body.length() == 0) {
    Serial.println("[OTA] empty response from GitHub");
    return false;
  }

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

  // Find first .bin asset — grab browser_download_url then resolve the redirect.
  String redirectUrl = "";
  JsonArray assets = doc["assets"].as<JsonArray>();
  for (JsonObject asset : assets) {
    String name = asset["name"] | "";
    if (name.endsWith(".bin")) {
      redirectUrl = asset["browser_download_url"] | "";
      break;
    }
  }

  if (redirectUrl.length() == 0) {
    Serial.printf("[OTA] release %s has no .bin asset\n", latestTag.c_str());
    return false;
  }

  // Resolve the GitHub redirect to the final objects.githubusercontent.com URL.
  // HTTPUpdate does not follow redirects itself, so we must do it here.
  assetUrl = resolveRedirect(redirectUrl);
  if (assetUrl.length() == 0) {
    Serial.printf("[OTA] could not resolve asset URL for %s\n", latestTag.c_str());
    // Fall back to the original URL — it may work in some cases.
    assetUrl = redirectUrl;
  }

  Serial.printf("[OTA] latest=%s local=%s\n[OTA] asset=%s\n",
                latestTag.c_str(), FIRMWARE_VERSION, assetUrl.c_str());

  return isNewer(latestTag, FIRMWARE_VERSION);
}
