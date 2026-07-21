#include "Arduino.h"
#include "../../config.h"
#include "../../globals.h"
#include "record.h"
#include "ui.h"
#include "network.h"
#include "driver/i2s_std.h"

// ============================================================
// Record — Amar Note audio capture
// ============================================================

static i2s_chan_handle_t rxHandle = nullptr;
static bool             recording = false;
static File             recFile;
static uint32_t         recStartMs = 0;
static uint32_t         recBytes   = 0;

static const uint32_t BUF_BYTES = (SAMPLE_RATE / 1000) * RECORD_BUF_MS * 2;

void recordInit() {
    i2s_chan_config_t chanCfg = I2S_CHANNEL_DEFAULT_CONFIG(
        I2S_NUM_0, I2S_ROLE_MASTER);
    i2s_new_channel(&chanCfg, nullptr, &rxHandle);

    i2s_std_config_t stdCfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(
                        I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = (gpio_num_t)I2S_BCK,
            .ws   = (gpio_num_t)I2S_WS,
            .dout = I2S_GPIO_UNUSED,
            .din  = (gpio_num_t)I2S_DI,
            .invert_flags = { .mclk_inv=false, .bclk_inv=false, .ws_inv=false },
        },
    };
    i2s_channel_init_std_mode(rxHandle, &stdCfg);
    i2s_channel_enable(rxHandle);
}

bool recordIsActive() { return recording; }

void recordStart() {
    if (recording) return;
    char path[64];
    snprintf(path, sizeof(path), "/rec_%lu.wav", (unsigned long)millis());
    recFile = SD_MMC.open(path, FILE_WRITE);
    if (!recFile) {
        Serial.println("[record] failed to open file");
        return;
    }
    // Reserve WAV header space
    uint8_t hdr[44] = {};
    recFile.write(hdr, 44);
    recBytes   = 0;
    recStartMs = millis();
    recording  = true;
    uiOnRecordStart();
    Serial.println("[record] started");
}

void recordStop() {
    if (!recording) return;
    recording = false;

    // Patch WAV header
    uint32_t dataSize   = recBytes;
    uint32_t riffSize   = dataSize + 36;
    uint32_t sampleRate = SAMPLE_RATE;
    uint32_t byteRate   = SAMPLE_RATE * 2;
    uint16_t blockAlign = 2;
    uint16_t bitsPerSample = 16;
    recFile.seek(0);
    recFile.write((uint8_t*)"RIFF", 4);
    recFile.write((uint8_t*)&riffSize, 4);
    recFile.write((uint8_t*)"WAVEfmt ", 8);
    uint32_t fmtSize = 16; recFile.write((uint8_t*)&fmtSize, 4);
    uint16_t audioFmt = 1; recFile.write((uint8_t*)&audioFmt, 2);
    uint16_t numCh = 1;    recFile.write((uint8_t*)&numCh, 2);
    recFile.write((uint8_t*)&sampleRate, 4);
    recFile.write((uint8_t*)&byteRate, 4);
    recFile.write((uint8_t*)&blockAlign, 2);
    recFile.write((uint8_t*)&bitsPerSample, 2);
    recFile.write((uint8_t*)"data", 4);
    recFile.write((uint8_t*)&dataSize, 4);
    recFile.close();

    uiOnRecordStop();
    networkEnqueueTranscribe(recFile.name());
    Serial.printf("[record] stopped, %lu bytes, queued for transcription\n",
                  (unsigned long)recBytes);
}

void recordService() {
    if (!recording) return;
    if ((millis() - recStartMs) >= (uint32_t)RECORD_MAX_S * 1000) {
        Serial.println("[record] max duration reached, stopping");
        recordStop();
        return;
    }
    static uint8_t buf[4096];
    size_t bytesRead = 0;
    i2s_channel_read(rxHandle, buf, BUF_BYTES, &bytesRead, pdMS_TO_TICKS(10));
    if (bytesRead > 0) {
        recFile.write(buf, bytesRead);
        recBytes += bytesRead;
    }
}
