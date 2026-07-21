# Amar Note 🎙️→📝

**A pocket voice-note device that records your voice, transcribes it, and uses AI to turn rambling speech into clean, coherent notes — synced straight to a GitHub repo and ready for Obsidian.**

Hold a button, talk, let go. A few seconds later a tidy Markdown note — with an AI-written title, a one-line summary, and a cleaned-up body — appears in your notes vault. The raw transcript is always preserved, too.

---

## ✨ What it does

- **One-button voice capture** on a tiny e-ink device — no screen-tapping, no phone.
- **On-device transcription** via OpenAI Whisper.
- **AI note cleanup (the headline upgrade):** a language model rewrites your messy, filler-filled speech into **coherent, succinct prose**, generates a **title**, a **one-sentence summary**, and **topic links** — automatically, every time you sync.
- **Verbatim safety net:** the original raw transcript is tucked into a foldable callout, so nothing you said is ever lost.
- **Syncs to GitHub** as clean Markdown with YAML frontmatter and tags — drop it into **Obsidian** and your notes organise themselves.
- **No app, no account lock-in, no cloud middleman** — it talks directly to OpenAI and to *your* GitHub repo.

---

## 🙏 Credits

Amar Note builds on **Forrest Note**, which itself builds on the original **Pala Note** firmware — full credit to the upstream authors for the hardware bring-up, the e-ink/audio/codec drivers, the recording engine, and the device UI. This project stands on top of both layers of work.

> Original project: **Pala Note** — <https://ko-fi.com/s/674a1a82e0>
> Huge thanks to the Pala Note author for the original device and firmware that made this possible.

### What the original Pala Note already did

Credit where it's due — the original firmware already provided: voice **recording**, **Whisper transcription**, a local note-transfer web server, on-device **tags**, sleep/power management, and all the low-level **e-ink, audio (ES8311/ES7210) and codec drivers** — plus the **physical device and 3D-printable case** design.

### What Forrest Note added

| Upgrade | Type | What changed |
|---|---|---|
| 🤖 **AI note cleanup** | ➕ New | A `gpt-4o-mini` pass rewrites each transcript into coherent prose. |
| 🧠 **AI metadata** | ➕ New | Auto-generated note **title**, one-line **summary**, and **topic backlinks**. |
| 🗂️ **Original transcript preserved** | ➕ New | Verbatim transcript in a foldable callout. |
| ☁️ **GitHub → Obsidian sync** | ➕ New | Notes pushed to your own GitHub repo as Markdown via the GitHub Contents API. |
| 📶 **Runtime provisioning** | ➕ New | Wi-Fi hotspot + captive portal for on-device NVS storage of credentials. |
| 🔒 **Real TLS validation** | 🔁 Changed | HTTPS validates against the Mozilla CA bundle. |
| 🔄 **OTA updates** | ➕ New | Firmware updates over the air from the portal (`/ota`). |
| 🐛 **Chunked-HTTP fix** | ➕ New | HTTPS client now decodes `Transfer-Encoding: chunked` responses. |
| 📊 **Snappier level meter** | 🔁 Changed | VU meter refresh ~2 Hz → ~10 Hz, plus async e-paper refresh. |
| 🔐 **Zero secrets in code** | 🔁 Changed | Wi-Fi and API keys live on-device, not in the repo. |
| 🏷️ **Title-named files** | 🔁 Changed | Notes saved under their one-word topic (`Soho.md`) instead of `note_001.md`. |
| 📅 **Calendar events** | ➕ New | AI extracts dated plans into `event_*` frontmatter. |
| 🧹 **Two-way delete** | ➕ New | Deleting a note on-device also removes it from the GitHub vault. |
| 🗑️ **Erase All** | ➕ New | **Settings → Erase All** with Device-only or Device+GitHub options. |

### What this fork (Amar Note) adds

| Upgrade | Type | What changed |
|---|---|---|
| 🎨 **Rebrand** | 🔁 Changed | All user-facing strings, portal pages, hotspot name, boot banner, and sleep-screen logo updated to Amar Note. |
| 🔘 **Button UX redesign** | 🔁 Changed | Two-button state machine rebuilt for instant tap-to-record; long-press/nav model simplified. |
| 🖥️ **Display speed tuning** | 🔁 Changed | Full refresh now only on boot/wake; all normal navigation uses strict partial refresh. |

---

## 🧰 Hardware

This firmware is built for the **Waveshare ESP32-S3 1.54″ e-Paper AIoT Development Board**:

> 🛒 **Reference board:** [Waveshare ESP32-S3 1.54inch e-Paper AIoT Dev Board](https://www.waveshare.com/esp32-s3-epaper-1.54.htm) (the **B/W**, non-"G" variant).

- **MCU:** ESP32-S3 (Xtensa LX7 dual-core @ 240 MHz) — **N8R8**: 8 MB flash + 8 MB OPI PSRAM
- **Display:** 1.54″ **200×200** e-paper (black/white)
- **Audio:** ES8311 codec + ES7210 ADC mic + speaker
- **Storage:** microSD (SD_MMC 1-bit)
- **Extras:** RTC (PCF85063), SHTC3 temp/humidity, LiPo charge management
- **Buttons:** Record (GPIO0 / BOOT) and Power (GPIO18)
- **Wireless:** 2.4 GHz Wi-Fi + BLE 5

### 🧊 3D-printable case

The printable enclosure is the original creator's hardware design — download from **Pala Note**: <https://ko-fi.com/s/674a1a82e0>.

---

## 📋 What you'll need

1. Assembled Pala Note hardware + USB-C cable.
2. An **OpenAI API key** — <https://platform.openai.com/api-keys>.
3. A **GitHub repo** for notes + a fine-grained PAT with **Contents: Read and write**.
4. Your **2.4 GHz Wi-Fi** name + password.

---

## 🚀 Installation

### Option A — With Claude Code

**1. Install toolchain & build:**
```
Set up my environment to build this Amar Note ESP32-S3 firmware. Install arduino-cli if missing,
install the esp32 board core version 3.2.0, and install the "Adafruit GFX Library" and "ArduinoJson"
libraries. Then compile the sketch in ./forrest_note for an ESP32-S3 N8R8 board using these options:
PSRAM=opi, PartitionScheme=custom, CDCOnBoot=cdc, FlashSize=8M. Report any errors.
```

**2. Flash it:**
```
Flash the Amar Note firmware in ./forrest_note to my connected ESP32-S3 device. Hold the record
button (BOOT/GPIO0), plug in USB while holding, keep holding until the write finishes.
Detect the serial port, then run the upload with PSRAM=opi, PartitionScheme=custom,
CDCOnBoot=cdc, FlashSize=8M. Confirm when hash is verified.
```

### Option B — Manual

**1. Install `arduino-cli`:**
```bash
brew install arduino-cli
```

**2. Install ESP32 core + libraries:**
```bash
arduino-cli config init
arduino-cli config add board_manager.additional_urls \
  https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
arduino-cli core update-index
arduino-cli core install esp32:esp32@3.2.0
arduino-cli lib install "Adafruit GFX Library" "ArduinoJson"
```

**3. Compile:**
```bash
arduino-cli compile \
  -b "esp32:esp32:esp32s3:PSRAM=opi,PartitionScheme=custom,CDCOnBoot=cdc,FlashSize=8M" \
  ./forrest_note
```

**4. Flash:** Hold BOOT button, plug in USB, keep holding:
```bash
arduino-cli board list
arduino-cli compile --upload -p /dev/cu.usbmodemXXXX \
  -b "esp32:esp32:esp32s3:PSRAM=opi,PartitionScheme=custom,CDCOnBoot=cdc,FlashSize=8M" \
  ./forrest_note
```

---

## 🔧 Setup — provision over the hotspot

1. Power on. With no Wi-Fi stored, device broadcasts **`AmarNote-Setup`**.
2. Connect phone/laptop to `AmarNote-Setup`.
3. Open browser to **`http://192.168.4.1`**.
4. Fill in Wi-Fi, OpenAI key, GitHub repo/token, vault folder, enable sync + AI.
5. Tap Save. Device reboots onto your Wi-Fi.

---

## 🎛️ Using it

| Action | Control |
|---|---|
| **Record a note** | Tap **record** (instant — no hold needed) |
| **Stop recording** | Tap **record** again |
| **Scroll / next** | Tap **power** |
| **Select / open** | Hold **record** briefly |
| **Back** | Long-hold **power** |
| **Delete a note** | Long-hold **power** while viewing a note |
| **Erase all notes** | **Settings → Erase All** |

---

## 🤖 How the AI pipeline works

1. **Transcribes** audio with **Whisper** (`whisper-1`).
2. **Enriches** with **`gpt-4o-mini`**: topic title, summary, cleaned body, tags, calendar event.
3. **Writes Markdown** and **pushes** to your GitHub repo.

Example output:
```markdown
---
title: "Soho"
date: 2026-06-22T18:10:00Z
source: amar-note
tags: ["Note"]
event_title: "Soho House"
event_start: 2026-06-23T19:00
---

> [!summary] Planning to meet friends at Soho House tomorrow evening.

I'm heading to Soho House tomorrow at seven...

> [!quote]- Original transcript
> So I'm going to Soho House tomorrow, like seven-ish...
```

---

## 🗂️ Obsidian integration

1. Clone your notes repo locally.
2. Open as an Obsidian vault.
3. Install **Obsidian Git**, enable auto-pull on launch + 1-min interval pull.

---

## ⚙️ Configuration reference

All runtime config lives in NVS (namespace `forrest`) and is set via the portal — never in code:
`WIFI_SSID`, `WIFI_PASS`, `OPENAI_KEY`, `repo`, `branch`, `dir`, `token`, `enabled`, `ai-enrich`.

---

## 🛠️ Troubleshooting

- **Device won't stay on USB for flashing** → hold BOOT button while plugging in and through the write.
- **Build error about duplicate `.cpp`** → delete any `* 2.cpp` / `* copy.cpp` files.
- **Boot loop / PSRAM errors** → confirm **PSRAM = OPI** and **Flash Size = 8MB**.
- **Notes sync but no AI summary** → check **AI titles + topic links** is ticked and your OpenAI key has billing/chat access.
- **Wi-Fi won't connect** → ESP32-S3 is **2.4 GHz only**.

---

## 🔒 Security & privacy

- No secrets in this repo. Keys stored in on-device NVS.
- Notes go only to OpenAI (transcription/cleanup) and your own GitHub repo.

---

## 📄 License

Amar Note's additions are released under the **MIT License**.
Built on **Forrest Note** (MIT) and the original **Pala Note** project — please honour the original author's license terms for their portions of the code.
