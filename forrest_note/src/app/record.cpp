#include "Arduino.h"
#include "../../config.h"
#include "../../globals.h"
#include "../../types.h"
#include "record.h"
#include "SD_MMC.h"
#include "esp_heap_caps.h"
#include "notes.h"
#include "ui.h"
#include "../../sounds.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"
#include <string.h>

extern "C" {
#include "../../src/audio/audio_bsp.h"
}

// Flag set by the main loop (STATE_RECORDING + EV_SINGLE) to stop recording.
// The producer task and the consumer drain loop both observe this.
volatile bool g_stopRecording = false;

// Amar Note — audio capture (producer) and SD-write (consumer) run on separate
// cores connected by a PSRAM ring buffer. The producer keeps draining the I2S
// DMA at line rate so a slow SD write only grows the ring instead of dropping samples.
struct RecCtx {
  RingbufHandle_t   ring;
  volatile bool     running;
  volatile bool     finished;
};

static void recProducerTask(void* arg) {
  RecCtx* ctx = (RecCtx*)arg;
  int16_t* sbuf = (int16_t*)heap_caps_malloc(REC_BUF,   MALLOC_CAP_8BIT);
  int16_t* mbuf = (int16_t*)heap_caps_malloc(REC_BUF/2, MALLOC_CAP_8BIT);
  const int monoSamples = REC_BUF / 4;

  if (sbuf && mbuf) {
    while (ctx->running) {
      audio_playback_read((void*)sbuf, REC_BUF);
      for (int i = 0; i < monoSamples; i++) mbuf[i] = sbuf[i * 2];
      xRingbufferSend(ctx->ring, mbuf, monoSamples * 2, pdMS_TO_TICKS(1000));
    }
  }

  if (sbuf) heap_caps_free(sbuf);
  if (mbuf) heap_caps_free(mbuf);
  ctx->finished = true;
  vTaskDelete(NULL);
}

bool record() {
  int num = nextNoteNumber();
  char path[64]; snprintf(path, sizeof(path), "%s/note_%03d.wav", NOTES_DIR, num);
  Serial.printf("[Rec] %s\n", path);

  File f = SD_MMC.open(path, FILE_WRITE);
  if (!f) return false;

  uint8_t header[44]={}; f.write(header, 44);

  RecCtx ctx;
  ctx.ring = xRingbufferCreateWithCaps(REC_RING_LEN, RINGBUF_TYPE_BYTEBUF, MALLOC_CAP_SPIRAM);
  if (!ctx.ring) { f.close(); return false; }
  ctx.running  = true;
  ctx.finished = false;

  TaskHandle_t producer = NULL;
  if (xTaskCreatePinnedToCore(recProducerTask, "recprod", 4096, &ctx, 6, &producer, 0) != pdPASS) {
    vRingbufferDeleteWithCaps(ctx.ring);
    f.close();
    return false;
  }

  uint32_t totalMono = 0, t0 = millis();
  int      recPeak = 0;
  const uint32_t MIN_REC_MS = 1000;
  bool     btnWasDown  = false;
  uint32_t btnDownAt   = 0;

  auto drain = [&](TickType_t wait) -> bool {
    size_t got = 0;
    void* item = xRingbufferReceive(ctx.ring, &got, wait);
    if (!item) return false;
    int16_t* sp = (int16_t*)item;
    int ns = got / 2;
    for (int i = 0; i < ns; i++) { int a = abs(sp[i]); if (a > recPeak) recPeak = a; }
    size_t written = f.write((uint8_t*)item, got);
    vRingbufferReturnItem(ctx.ring, item);
    totalMono += written;
    return true;
  };

  uint32_t lastUi = 0;
  while (!g_stopRecording && (millis() - t0 < MAX_REC_MS)) {
    drain(pdMS_TO_TICKS(40));

    uint32_t now = millis();
    bool btnDown = (digitalRead(BTN_REC) == LOW);
    if (btnDown && !btnWasDown) {
      btnDownAt  = now;
      btnWasDown = true;
    } else if (!btnDown && btnWasDown) {
      uint32_t held = now - btnDownAt;
      if (held < BTN_LONG_MS && (now - t0) >= MIN_REC_MS) {
        g_stopRecording = true;
      }
      btnWasDown = false;
    }

    if (now - lastUi >= 100) {
      lastUi = now;
      int lvl = (int)((long)recPeak * 152L * 3L / 32767L);
      if (lvl > 152) lvl = 152;
      showRecordingLive(now - t0, lvl);
      recPeak = 0;
    }
  }

  ctx.running = false;
  while (!ctx.finished) drain(pdMS_TO_TICKS(50));
  while (drain(0)) {}

  vRingbufferDeleteWithCaps(ctx.ring);

  f.seek(0);
  uint32_t dB=totalMono, fS=dB+36, bR=SAMPLE_RATE*2;
  uint16_t bA=2,aF=1,ch=1,bps=16; uint32_t fL=16,sr=SAMPLE_RATE;
  f.write((uint8_t*)"RIFF",4); f.write((uint8_t*)&fS,4);
  f.write((uint8_t*)"WAVE",4); f.write((uint8_t*)"fmt ",4);
  f.write((uint8_t*)&fL,4);   f.write((uint8_t*)&aF,2);
  f.write((uint8_t*)&ch,2);   f.write((uint8_t*)&sr,4);
  f.write((uint8_t*)&bR,4);   f.write((uint8_t*)&bA,2);
  f.write((uint8_t*)&bps,2);
  f.write((uint8_t*)"data",4); f.write((uint8_t*)&dB,4);
  f.close();

  lastRecNum = num;
  Serial.printf("[Rec] done: %lu bytes\n", (unsigned long)totalMono);
  return totalMono > 1000;
}

// Walk RIFF sub-chunks to find the real PCM data offset.
// Falls back to 44 if the file doesn't look like a valid RIFF WAV.
static uint32_t wavDataOffset(File& f) {
  uint8_t id[4]; uint32_t sz;
  f.seek(0);
  if (f.read(id, 4) != 4 || memcmp(id, "RIFF", 4) != 0) return 44;
  f.read((uint8_t*)&sz, 4);
  if (f.read(id, 4) != 4 || memcmp(id, "WAVE", 4) != 0) return 44;
  while (f.available() >= 8) {
    if (f.read(id, 4) != 4) break;
    if (f.read((uint8_t*)&sz, 4) != 4) break;
    if (memcmp(id, "data", 4) == 0) return (uint32_t)f.position();
    uint32_t skip = (sz + 1) & ~1u;
    if (!f.seek(f.position() + skip)) break;
  }
  return 44;
}

bool playWavFile(const char* path) {
  File f = SD_MMC.open(path);
  if (!f) return false;
  if (f.size() <= 44) { f.close(); return false; }

  uint32_t dataOffset = wavDataOffset(f);
  f.seek(dataOffset);

  const int monoBytes = 1024;
  uint8_t* monoBuf   = (uint8_t*)heap_caps_malloc(monoBytes,     MALLOC_CAP_8BIT);
  int16_t* stereoBuf = (int16_t*)heap_caps_malloc(monoBytes * 2, MALLOC_CAP_8BIT);

  if (!monoBuf || !stereoBuf) {
    if (monoBuf)   heap_caps_free(monoBuf);
    if (stereoBuf) heap_caps_free(stereoBuf);
    f.close();
    return false;
  }

  audioPlaying = true;
  stopPlayback = false;

  amarSoundSetEnabled(false);

  // Re-open the codec path after recording. The I2S DMA clock can be in a
  // degraded state after record() exits. audio_play_init() re-opens both
  // handles (idempotent) and restores sample-rate/channel config.
  audio_play_init();
  delay(20);
  audio_playback_set_vol(85);

  while (f.available() && !stopPlayback) {
    int readBytes = f.read(monoBuf, monoBytes);
    if (readBytes <= 0) break;
    if (readBytes & 1) readBytes--;

    int samples = readBytes / 2;
    int16_t* mono = (int16_t*)monoBuf;
    for (int i = 0; i < samples; i++) {
      int16_t s = mono[i];
      stereoBuf[i * 2 + 0] = s;
      stereoBuf[i * 2 + 1] = s;
    }
    audio_playback_write((void*)stereoBuf, (uint32_t)(samples * 2 * sizeof(int16_t)));

    if (digitalRead(BTN_REC) == LOW) {
      delay(20);
      if (digitalRead(BTN_REC) == LOW) {
        while (digitalRead(BTN_REC) == LOW) delay(5);
        stopPlayback = true;
      }
    }
  }

  audio_playback_set_vol(0);

  // amarSoundSetEnabled(true) only flips the flag, does NOT restore the volume
  // register. Explicitly set vol so UI sounds work immediately after playback.
  amarSoundSetEnabled(true);
  if (amarSoundIsEnabled()) audio_playback_set_vol(75);

  heap_caps_free(monoBuf);
  heap_caps_free(stereoBuf);
  f.close();

  audioPlaying = false;
  stopPlayback = false;
  return true;
}
