#pragma once

// otaCheckGithub() — queries the GitHub releases API for the latest release.
// Returns true if a newer version was found and populates latestTag + assetUrl.
// Returns false on network error or if already up to date.
bool otaCheckGithub(String& latestTag, String& assetUrl);
