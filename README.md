# Amar Note 🎙️→📝

**A pocket voice-note device that records your voice, transcribes it, and uses AI to turn rambling speech into clean, coherent notes — synced straight to a GitHub repo and ready for Obsidian.**

Tap a button, talk, tap again. A few seconds later a tidy Markdown note — with an AI-written title, a one-line summary, and a cleaned-up body — appears in your notes vault. The raw transcript is always preserved, too.

---

## ✨ What it does

- **One-button voice capture** on a tiny e-ink device — no screen-tapping, no phone.
- **On-device transcription** via OpenAI Whisper or **Groq Whisper (free tier)**.
- **AI note cleanup (the headline upgrade):** a language model rewrites your messy, filler-filled speech into **coherent, succinct prose**, generates a **title**, a **one-sentence summary**, and **topic links** — automatically, every time you sync.
- **Verbatim safety net:** the original raw transcript is tucked into a foldable callout, so nothing you said is ever lost.
- **Syncs to GitHub** as clean Markdown with YAML frontmatter and tags — drop it into **Obsidian** and your notes organise themselves.
- **USB Mass Storage** — mount the device's SD card directly on your computer, no cables or apps required.
- **No app, no account lock-in, no cloud middleman** — it talks directly to your chosen STT and enrichment providers and to *your* GitHub repo.

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
| 🐛 **Chunked-HTTP decode fix** | 🔁 Changed | HTTPS client now decodes `Transfer-Encoding: chunked` responses. |
| 📊 **Snappier level meter** | 🔁 Changed | VU meter refresh ~2 Hz → ~10 Hz, plus async e-paper refresh. |
| 🔐 **Zero secrets in code** | 🔁 Changed | Wi-Fi and API keys live on-device, not in the repo. |
| 🏷️ **Title-named files** | 🔁 Changed | Notes saved under their one-word topic (`Soho.md`) instead of `note_001.md`. |
| 📅 **Calendar events** | ➕ New | AI extracts dated plans into `event_*` frontmatter. |
| 🧹 **Two-way delete** | ➕ New | Deleting a note on-device also removes it from the GitHub vault. |
| 🗑️ **Erase All** | ➕ New | **Settings → Erase All** with Device-only or Device+GitHub options. |

### What this fork (Amar Note) adds

| Upgrade | Type | What changed |
|---|---|---|
| 🎨 **Rebrand** | 🔁 Changed | All user-facing strings, portal pages, hotspot name (`AmarNote-Setup`), boot banner, and sleep-screen logo updated to Amar Note. |
| 🎤 **Groq STT (free tier)** | ➕ New | Optional Groq `whisper-large-v3-turbo` transcription — no credit card needed. Switch providers in the portal. |
| 🤖 **Selectable AI enrichment backend** | ➕ New | AI note cleanup can use **OpenAI `gpt-4o-mini`** (default) or any of three **Groq Llama models** (`llama-3.3-70b-versatile`, `llama-3.1-8b-instant`, `llama-4-scout-17b-16e-instruct`). Switch provider and model in the portal — no reflash needed. |
| 🗂️ **Portal provider links** | ➕ New | Setup page now shows direct links to OpenAI, Groq, and GitHub PAT pages. |
| 🖥️ **Portal visual redesign** | 🔁 Changed | Captive-portal UI overhauled: battery level + real-time clock shown in the header, STT provider and enrichment model each have their own dedicated selector UI. |
| ⏱️ **POSIX timezone** | 🔁 Changed | Replaced the vestigial `LOCAL_TIME_OFFSET_MIN` constant with a proper DST-aware POSIX TZ string (`DEVICE_TZ_POSIX`). Note timestamps and calendar events now reflect real local time. |
| 👆 **Touch screen support** | ➕ New | Full support for the **ESP32-S3-Touch-ePaper-1.54** variant. The touch layer is additive — all menus and navigation work by direct tap. Buttons remain fully functional on both variants (see [Hardware](#-hardware)). |
| 🔌 **Correct hardware pins** | 🔁 Changed | All EPD SPI, I2C, power-control, and button pins corrected to the official Waveshare ESP32-S3-ePaper-1.54 / Touch-ePaper-1.54 definitions. |
| 📡 **HTTPS chunked read fix** | 🔁 Changed | HTTP client reads in 512-byte chunks instead of single bytes — eliminates timeout drops on slow connections. |
| 🏷️ **Dirty-tag MOC rebuild** | 🔁 Changed | Tag index (`_MOC`) files are only rewritten when their tag set actually changes, not on every sync. |
| 🗄️ **NVS namespace** | 🔁 Changed | NVS partition namespace renamed `forrest` → `amar`. ⚠️ Existing devices need a flash-erase on first install of this firmware. |
| 💾 **USB Mass Storage** | ➕ New | Menu item **"USB Drive"** mounts the SD card as a USB MSC drive on any host computer — browse, copy, or delete files directly. Hold REC to exit MSC mode; the SD remounts and the note index reloads automatically. Requires ESP32 Arduino core ≥ 2.0. |
| 🕒 **Portable UTC epoch conversion** | 🔁 Changed | `utcTmToEpoch()` now uses a portable `mktime()` emulation (TZ save/restore) instead of `timegm()`, which is not available in the ESP32 Arduino/ESP-IDF toolchain. |
| 🐱 **Tamagotchi** | ➕ New | Sixth menu tile (cursor 5) — cat-face icon, `STATE_TAMAGOTCHI` state, `showTamagotchi()` stub screen. Pet logic ships in its own module; the tile and screen are fully wired. Based on **[pala-nekogotchi](https://github.com/defcon1702/pala-nekogotchi)** by defcon1702 — an offline virtual cat designed for this hardware, with real-time RTC aging, mood states, and 1-bit pixel-art sprites. |

### Third-party addons & acknowledgements

#### 🐱 Nekogotchi — by [defcon1702](https://github.com/defcon1702)

The Tamagotchi feature module is derived from **[pala-nekogotchi](https://github.com/defcon1702/pala-nekogotchi)** — a fully offline virtual cat addon designed specifically for this hardware. It provides:

- A real-time aging model driven by the on-board **PCF85063 RTC**, so the cat keeps living across deep-sleep.
- Three stats (`hunger`, `happiness`, `energy`) that decay and recover over wall-clock time.
- Five moods and multiple action poses rendered as **1-bit 150×150 pixel-art sprites** via `drawBitmap1BPP`.
- State persistence to `/pet.dat` on the SD card.
- A two-button control scheme matching the device's existing input convention.

> **pala-nekogotchi** is released under the **MIT License** by defcon1702.
> Source: <https://github.com/defcon1702/pala-nekogotchi>

---

## 🧰 Hardware

Amar Note supports **two Waveshare ESP32-S3 1.54″ e-Paper boards** — pick whichever suits you:

| Board | Touch | Buttons | Notes |
|---|---|---|---|
| **[ESP32-S3-ePaper-1.54](https://www.waveshare.com/esp32-s3-epaper-1.54.htm)** | ✗ | ✅ REC + PWR | Original variant — button-only |
| **[ESP32-S3-Touch-ePaper-1.54](https://www.waveshare.com/esp32-s3-touch-epaper-1.54.htm)** | ✅ FT6336 | ✅ REC + PWR | Touch adds tap navigation; buttons still work |

> **Note:** The two Waveshare boards share the same MCU, display, audio codec, and pin assignments. They use the same firmware binary — no compile-time flag needed. The touch driver initialises on startup; on the non-touch board it silently finds no FT6336 device and the touch layer is a no-op.

Both boards are the **B/W** (black-and-white, non-"G") 200×200 e-paper variant.

### Hardware version — V2 boards required

> ⚠️ **This firmware requires the V2 (current) board revision.**

Waveshare has shipped two MCU revisions of the ESP32-S3-ePaper-1.54 / Touch-ePaper-1.54 boards:

| Revision | MCU module | Flash | PSRAM | Status |
|---|---|---|---|---|
| **V1** | ESP32-S3FH4R2 | 4 MB | 2 MB | ❌ Not supported |
| **V2** (current) | **ESP32-S3-PICO-1-N8R8** | **8 MB** | **8 MB OPI** | ✅ Supported |

Amar Note is compiled and tuned for the **N8R8** variant (8 MB flash + 8 MB OPI PSRAM). The build flags, PSRAM configuration (`PSRAM=opi`), custom partition table, and ring-buffer recording engine all depend on this. The V1 board's 2 MB PSRAM is insufficient for the PSRAM-backed ring buffer and cannot be supported without significant re-engineering.

If you're purchasing new hardware, both current Waveshare product pages ship V2 boards. If you have an older unit, check the MCU module label on the back of the board: the V2 module is marked **ESP32-S3-PICO-1**.

### Common hardware specs (V2)

- **MCU:** ESP32-S3-PICO-1-N8R8 (Xtensa LX7 dual-core @ 240 MHz) — **8 MB flash + 8 MB OPI PSRAM**
- **Display:** 1.54″ **200×200** e-paper (black/white)
- **Audio:** ES8311 codec + ES7210 ADC mic + speaker
- **Storage:** microSD (SD_MMC 1-bit)
- **Extras:** RTC (PCF85063), SHTC3 temp/humidity, LiPo charge management
- **Buttons:** Record (GPIO0 / BOOT) and Power (GPIO18) — present on both variants
- **Wireless:** 2.4 GHz Wi-Fi + BLE 5

### 🧊 3D-printable case

The printable enclosure is the original creator's hardware design — download from **Pala Note**: <https://ko-fi.com/s/674a1a82e0>.

---

## 📋 What you'll need

1. Assembled Waveshare ESP32-S3-ePaper-1.54 **or** ESP32-S3-Touch-ePaper-1.54 board (**V2 / N8R8**) + USB-C cable.
2. *(For AI enrichment with OpenAI)* An **OpenAI API key** — <https://platform.openai.com/api-keys>.
3. *(Optional, free alternative for both STT and enrichment)* A **Groq API key** — <https://console.groq.com/keys>.
4. A **GitHub repo** for notes + a fine-grained PAT with **Contents: Read and write** — <https://github.com/settings/tokens>.
5. Your **2.4 GHz Wi-Fi** name + password.

> **Minimum API requirement:** you need at least one of an OpenAI key or a Groq key. Groq's free tier covers both transcription (`whisper-large-v3-turbo`) and enrichment (Llama models) with no credit card required.

---

## 🚀 Installation

### Option A — With Claude Code

**1. Install toolchain & build:**
```
Set up my environment to build this Amar Note ESP32-S3 firmware. Install arduino-cli if missing,
install the esp32 board core version 3.2.0, and install the "Adafruit GFX Library" and "ArduinoJson"
libraries. Then compile the sketch in ./amar_note for an ESP32-S3 N8R8 board using these options:
PSRAM=opi, PartitionScheme=custom, CDCOnBoot=cdc, FlashSize=8M. Report any errors.
```

**2. Flash it:**
```
Flash the Amar Note firmware in ./amar_note to my connected ESP32-S3 device. Hold the record
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
  ./amar_note
```

**4. Flash:** Hold BOOT button, plug in USB, keep holding:
```bash
arduino-cli board list
arduino-cli compile --upload -p /dev/cu.usbmodemXXXX \
  -b "esp32:esp32:esp32s3:PSRAM=opi,PartitionScheme=custom,CDCOnBoot=cdc,FlashSize=8M" \
  ./amar_note
```

---

## 🔧 Setup — provision over the hotspot

1. Power on. With no Wi-Fi stored, device broadcasts **`AmarNote-Setup`**.
2. Connect phone/laptop to `AmarNote-Setup`.
3. Open browser to **`http://192.168.4.1`**.
4. Fill in Wi-Fi credentials, GitHub repo/token, and vault folder.
5. Enter your **OpenAI API key** and/or **Groq API key** depending on which providers you want to use.
6. Set **STT Provider**: **OpenAI** (whisper-1) or **Groq** (whisper-large-v3-turbo, free).
7. Set **Enrichment Provider**: **OpenAI** (gpt-4o-mini) or **Groq** (Llama — select model).
8. Enable sync and AI enrichment. Tap **Save**. Device reboots onto your Wi-Fi.

---

## 🎛️ Using it

### Buttons (both variants)

Two physical buttons control everything: **record** (GPIO0 / BOOT) and **power** (GPIO18).
A **tap** is any press released before the long-hold threshold (~450 ms); a **long-hold** crosses that threshold and fires immediately — no need to release first.

| Action | Control |
|---|---|
| **Record a note** | Tap **record** |
| **Stop recording** | Tap **record** again |
| **Scroll / next item** | Tap **power** |
| **Select / open** | Tap **record** |
| **Back** | Long-hold **record** |
| **Play back a recording** | Tap **record** while viewing a note |
| **Delete a note** | Long-hold **power** while viewing a note |
| **Erase all notes** | **Settings → Erase All** |
| **Wake to menu** | Hold **power** while powering on |
| **Wake straight to record** | Hold **record** while powering on |
| **USB Drive mode** | Menu → **USB Drive** → tap **record** → hold **record** to exit |
| **Tamagotchi** | Menu → **Tamagotchi** tile (cursor 5) → any button to return |

### Touch screen (ESP32-S3-Touch-ePaper-1.54 only)

On the touch variant the FT6336 capacitive controller adds direct tap navigation across all menus. **Buttons remain fully functional** — touch and buttons work side by side.

#### Main menu layout

The menu uses an **Option-D hybrid layout**: two tall portrait tiles on top (Notes, Tags) and a 2×2 icon-tile grid below (Sync, Settings, USB Drive, Tamagotchi).

| Tile | Position | Icon | Cursor |
|---|---|---|---|
| **Notes** | top-left | document lines | 0 |
| **Tags** | top-right | tag shape | 1 |
| **Sync** | mid-left | circular arrows | 2 |
| **Settings** | mid-right | gear / cog | 3 |
| **USB Drive** | bottom-left | USB trident | 4 |
| **Tamagotchi** | bottom-right | cute cat face 🐱 | 5 |

#### Touch gestures

| Touch gesture | Action |
|---|---|
| ~~**Tap anywhere (idle screen)**~~ | ~~Start recording~~ — **removed** (see note below) |
| **Tap a menu tile or row** | Select that item (equivalent to REC) |
| **Tap a settings row** | Activate that setting |
| **Tap a tag pill** | Choose that tag after recording |
| **Tap the big tag card** | Drill into that tag's note list |
| **Tap outside the tag card** | Cycle to the next tag |
| **Tap a note card** | Open note detail |
| **Tap upper area of note detail** | Play back the recording |
| **Tap lower area of note detail** | Scroll to next page / advance to next note |
| **Tap bottom strip (y ≥ 180)** | Go back (equivalent to long-hold REC) |
| **Tap Tamagotchi screen (anywhere)** | Return to menu |

> **Touch on the idle screen is disabled by default** to prevent an accidental brush of the screen from silently starting a recording. Recording is always started with the physical **REC button**; only stopping is touch-accessible after recording begins (which is also button-driven).
>
> **To re-enable idle-touch recording:** Settings → **"idle rec"** toggles this at runtime with no reflash required. The setting is stored in NVS under key `idlerec`.

> **Recording is always button-driven.** Only the **record button** stops a recording. This keeps recording UX consistent across both hardware variants.

---

## 💾 USB Mass Storage

Select **USB Drive** from the main menu and tap **record**. The e-ink screen shows:

```
USB Storage
───────────
  Connected

safely eject before
      exiting

 [hold REC to exit]
```

The SD card appears on your host computer as a standard USB drive — browse, copy, rename, or delete files directly. When done, safely eject from your OS, then hold **record** on the device to exit. The SD card remounts automatically and the note index reloads.

> **Requirement:** ESP32 Arduino core ≥ 2.0 (for `USB.h` / `USBMSC.h`). The board must be connected via USB-C to a host that supports USB OTG device mode.

---

## 🤖 How the AI pipeline works

1. **Transcribes** audio with **Whisper** — either OpenAI (`whisper-1`) or Groq (`whisper-large-v3-turbo`, free tier). Set your preference in the portal.
2. **Enriches** the transcript using your chosen provider:
   - **OpenAI** — `gpt-4o-mini` (requires an OpenAI key with chat access).
   - **Groq** — one of three Llama models (free tier, no credit card):
     - `llama-3.3-70b-versatile` *(default — best quality, 30 RPM)*
     - `llama-3.1-8b-instant` *(fastest/lightest, 30 RPM)*
     - `llama-4-scout-17b-16e-instruct` *(newer model, 15 RPM)*
   - Enrichment produces: topic **title**, one-line **summary**, cleaned **body**, **tags**, and any **calendar events**.
3. **Writes Markdown** and **pushes** to your GitHub repo. Only tag index files whose tag set changed are rewritten.

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

All runtime config lives in NVS (namespace **`amar`**) and is set via the portal — never in code. The actual NVS key strings are listed below (these are what the firmware reads; useful if you ever inspect NVS directly via `esptool` or serial monitor).

### Wi-Fi

| NVS key | Default | Description |
|---|---|---|
| `ssid` | — | Wi-Fi network name |
| `pass` | — | Wi-Fi password |

### API keys

| NVS key | Default | Description |
|---|---|---|
| `oaikey` | — | OpenAI API key |
| `groqkey` | — | Groq API key |

### STT provider

| NVS key | Default | Description |
|---|---|---|
| `sttprov` | `0` | Speech-to-text provider: `0` = OpenAI (`whisper-1`), `1` = Groq (`whisper-large-v3-turbo`) |

### AI enrichment

| NVS key | Default | Description |
|---|---|---|
| `enrichprov` | `0` | Enrichment provider: `0` = OpenAI (`gpt-4o-mini`), `1` = Groq (model selectable). Auto-defaults to `1` on first boot if a Groq key is already present. |
| `enrichmdl` | `llama-3.3-70b-versatile` | Groq enrichment model. One of: `llama-3.3-70b-versatile`, `llama-3.1-8b-instant`, `llama-4-scout-17b-16e-instruct`. Ignored when `enrichprov` is `0` (OpenAI always uses `gpt-4o-mini`). |

### GitHub / Obsidian vault

| NVS key | Default | Description |
|---|---|---|
| `ghtok` | — | GitHub fine-grained PAT (Contents: Read and write) |
| `ghrepo` | — | GitHub repo in `owner/repo` format |
| `ghbranch` | `main` | Branch to push notes to |
| `ghdir` | `VoiceNotes` | Vault folder path in the repo |
| `ghon` | `false` | GitHub sync on/off |
| `ghai` | `true` | AI enrichment on/off |

### Touch behaviour

| NVS key | Default | Description |
|---|---|---|
| `idlerec` | `false` | When `true`, tapping the idle screen starts a recording. Disabled by default to prevent accidental pocket recordings. Toggle via **Settings → "idle rec"** — no reflash required. |

Timezone is set at compile time via `DEVICE_TZ_POSIX` in `config.h`. The default is US Pacific (`PST8PDT,M3.2.0,M11.1.0`). Common alternatives are documented inline in `config.h`.

### Compile-time flags (`config.h`)

| Flag | Default | Description |
|---|---|---|
| `DEVICE_TZ_POSIX` | `PST8PDT,M3.2.0,M11.1.0` | POSIX TZ string for your timezone. Controls note timestamps and calendar event times. Common examples are in `config.h`. Change before compiling. |

---

## 🛠️ Troubleshooting

- **Device won't stay on USB for flashing** → hold BOOT button while plugging in and through the write.
- **Build error about duplicate `.cpp`** → delete any `* 2.cpp` / `* 2.h` files that macOS may have created in the `src/` tree.
- **Portal not appearing after connect** → navigate to `http://192.168.4.1` manually; some phones suppress captive-portal prompts.
- **Notes not syncing** → check that `ghon` is `true` in the portal, and that your PAT has **Contents: Read and write** on the correct repo.
- **Wrong timestamps on notes** → update `DEVICE_TZ_POSIX` in `config.h` to match your timezone and reflash.
- **Enrichment silently skipped** → confirm the correct API key is set for your chosen `enrichprov`. Check the serial monitor — the firmware logs which provider it is trying and why it skipped.
- **NVS namespace mismatch after upgrading from Forrest Note** → the namespace changed from `forrest` to `amar`. Do a full flash-erase (`esptool.py erase_flash`) before flashing Amar Note for the first time, then re-provision via the portal.
