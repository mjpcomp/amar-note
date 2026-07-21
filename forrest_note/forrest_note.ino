#include "Arduino.h"
#include "SD_MMC.h"
#include "WiFi.h"
#include "HTTPClient.h"
#include "esp_task_wdt.h"
#include "esp_sleep.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "ArduinoJson.h"
#include "config.h"
#include "globals.h"
#include "src/app/buttons.h"
#include "src/app/record.h"
#include "src/app/network.h"
#include "src/app/ui.h"
#include "src/app/config_store.h"
#include "src/app/battery.h"
#include "logo_bitmap.h"

// ============================================================
// Amar Note — firmware entry point
// ============================================================

void setup() {
    Serial.begin(115200);
    Serial.println("\n=== Amar Note " FIRMWARE_VERSION " ===");

    configLoad();
    batteryInit();
    displayInit();
    buttonsInit();
    sdInit();

    bool apOk = WiFi.softAP("AmarNote-Setup");
    IPAddress apIP = WiFi.softAPIP();
    Serial.printf("[setup] AP %s IP %s\n",
                  "AmarNote-Setup", apIP.toString().c_str());

    networkInit();
    uiInit();

    Serial.println("[setup] done");
}

void loop() {
    buttonsService();
    networkService();
    uiService();
    batteryService();
    delay(LOOP_DELAY_MS);
}
