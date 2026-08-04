#pragma once
#include <Arduino.h>

// otaCheckGithub() — queries the GitHub releases API for the latest release.
// Returns true if a newer version was found and populates:
//   latestTag  — the release tag string (e.g. "v1.5.2")
//   assetUrl   — the browser_download_url for the .bin (no auth required)
//   assetSize  — the true firmware binary size in bytes (from the API JSON);
//               use this as the totalBytes denominator in progressCb, not
//               the CDN Content-Length which reflects full flash capacity.
// Returns false on network error or if already up to date.
bool otaCheckGithub(String& latestTag, String& assetUrl, size_t& assetSize);

// otaDownloadAndFlash() — downloads firmware from a browser_download_url and
// writes it directly to the inactive OTA partition using Update.h.
// assetSize should come from otaCheckGithub(); pass 0 to skip accurate progress.
// progressCb is called repeatedly with (bytesWritten, totalBytes); pass nullptr to skip.
// Returns true on success (device should reboot immediately after).
bool otaDownloadAndFlash(const String& browserDownloadUrl,
                         size_t assetSize,
                         void (*progressCb)(size_t, size_t) = nullptr);
