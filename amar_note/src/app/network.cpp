#include "Arduino.h"
#include "../../config.h"
#include "../../globals.h"
#include "../../types.h"
#include "network.h"
#include "notes.h"
#include "rtc.h"
#include "ui.h"
#include "battery.h"
#include "config_store.h"
#include "ota.h"
#include "WiFi.h"
#include "WiFiClientSecure.h"
#include <WebServer.h>
#include <HTTPUpdate.h>
#include <ArduinoJson.h>
#include "SD_MMC.h"
#include "esp_heap_caps.h"
#include "../../secrets.h"

extern const uint8_t x509_crt_bundle_start[] asm("_binary_x509_crt_bundle_start");
extern const uint8_t x509_crt_bundle_end[]   asm("_binary_x509_crt_bundle_end");

// ─── OTA progress globals (file-scope so lambdas and the API handler share them)
volatile int         g_otaPct   = 0;
volatile const char* g_otaStage = "idle";

// ─── Transcription 