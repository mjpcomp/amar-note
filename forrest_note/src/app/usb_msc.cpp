#include "Arduino.h"
#include "SD_MMC.h"
#include "USB.h"
#include "USBMSC.h"
#include "../../config.h"
#include "../../globals.h"
#include "ui.h"
#include "usb_msc.h"

static USBMSC msc;
static bool   mscMounted = false;

// ---------------------------------------------------------------------------
// MSC callbacks — called by TinyUSB on the USB task.
// SD_MMC is already unmounted before these are ever called.
// ---------------------------------------------------------------------------
static int32_t mscRead(uint32_t lba, uint32_t offset, void* buf, uint32_t bufSize) {
  uint32_t sector = lba + offset / 512;
  if (SD_MMC.readRAW((uint8_t*)buf, sector)) return (int32_t)bufSize;
  return -1;
}

static int32_t mscWrite(uint32_t lba, uint32_t offset, uint8_t* buf, uint32_t bufSize) {
  uint32_t sector = lba + offset / 512;
  if (SD_MMC.writeRAW(buf, sector)) return (int32_t)bufSize;
  return -1;
}

// ---------------------------------------------------------------------------
// enterMscMode — public entry point.
// ---------------------------------------------------------------------------
void enterMscMode() {
  // Unmount filesystem so the host gets exclusive block-level access.
  SD_MMC.end();
  delay(50);

  // Briefly re-init in raw mode to query card geometry, then end again.
  SD_MMC.begin("/sdcard", true);  // 1-bit mode is fine for geometry query
  uint32_t sectorCount = (uint32_t)(SD_MMC.cardSize() / 512ULL);
  SD_MMC.end();
  delay(20);

  // Register MSC device with TinyUSB.
  // Note: onFlush() is not part of the USBMSC API in ESP32 Arduino core 3.x.
  // SD_MMC.writeRAW() is synchronous so no explicit flush is needed.
  msc.vendorID("Amar");
  msc.productID("Note SD Card");
  msc.productRevision("1.0");
  msc.onRead(mscRead);
  msc.onWrite(mscWrite);
  msc.mediaPresent(true);
  msc.begin(sectorCount, 512);

  USB.begin();
  mscMounted = true;

  // Show the full-screen MSC UI and block until hold-REC.
  showUsbMsc();

  bool btnWasDown = false;
  uint32_t btnDownAt = 0;
  bool done = false;

  while (!done) {
    delay(20);
    bool btnDown = (digitalRead(BTN_REC) == LOW);
    if (btnDown && !btnWasDown) {
      btnDownAt  = millis();
      btnWasDown = true;
    } else if (btnWasDown) {
      uint32_t held = millis() - btnDownAt;
      if (!btnDown) {
        btnWasDown = false;  // released before long-press threshold — ignore
      } else if (held >= (uint32_t)BTN_LONG_MS) {
        done = true;         // held long enough — exit MSC mode
      }
    }
  }

  // Tear down USB MSC.
  msc.mediaPresent(false);
  delay(100);  // give host a moment to notice media gone
  mscMounted = false;

  // Re-mount SD filesystem.
  delay(50);
  SD_MMC.begin("/sdcard", true);
}
