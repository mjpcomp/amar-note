#pragma once
#include <Arduino.h>

// otaCheckGithub() — queries the GitHub releases API for the latest release.
// Returns true if a newer version was found and populates latestTag + assetUrl.
// The assetUrl returned is the API asset URL (api.github.com/repos/.../releases/assets/{id}),
// NOT browser_download_url — it must be fetched with Authorization + Accept: octet-stream.
// Returns false on network error or if already up to date.
bool otaCheckGithub(String& latestTag, String& assetUrl);

// otaDownloadAndFlash() — downloads firmware from the GitHub API asset URL and
// writes it directly to the inactive OTA partition using Update.h.
// progressCb is called repeatedly with (bytesWritten, totalBytes); pass nullptr to skip.
// Returns true on success (device should reboot immediately after).
bool otaDownloadAndFlash(const String& apiAssetUrl,
                         void (*progressCb)(size_t, size_t) = nullptr);
