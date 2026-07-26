#include "Arduino.h"
#include "SD_MMC.h"
#include "USB.h"
#include "USBMSC.h"
#include "../../config.h"
#include "../../globals.h"
#include "ui.h"
#include "usb_msc.h"

static USBMSC msc;
static uint32_t sMscSectorCount = 0;

// ---------------------------------------------------------------------------
// MSC callbacks — called by TinyUSB on the USB task.
// SD_MMC is unmounted before mediaPresent(true) is set.
// ---------------------------------------------------------------------------
static int32_t mscRead(uint32_t lba, uint32_t offset, void* buf, uint32_t bufSize) {
  uint32_t sector = lba + (offset / 512);
  if (SD_MMC.readRAW((uint8_t*)buf, sector)) return (int32_t)bufSize;
  return -1;
}

static int32_t mscWrite(uint32_t lba, uint32_t offset, uint8_t* buf, uint32_t bufSize) {
  uint32_t sector = lba + (offset / 512);
  if (SD_MMC.writeRAW(buf, sector)) return (int32_t)bufSize;
  return -1;
}

// ---------------------------------------------------------------------------
// usb_msc_init — call once from setup(), before USB.begin().
// Registers the MSC class. mediaPresent starts false so the host sees
// no media until the user explicitly enters USB Drive mode.
// ---------------------------------------------------------------------------
void usb_msc_init() {
  // Query card geometry while SD is still mounted.
  sMscSectorCount = (uint32_t)(SD_MMC.cardSize() / 512ULL);

  msc.vendorID("Amar");
  msc.productID("Note SD Card");
  msc.productRevision("1.0");
  msc.onRead(mscRead);
  msc.onWrite(mscWrite);
  msc.mediaPresent(false);   // not connected until user requests it
  msc.begin(sMscSectorCount, 512);
  // USB.begin() is called by the caller (setup()) after this returns.
}

// ---------------------------------------------------------------------------
// enterMscMode — called when user selects USB Drive from the menu.
// Blocks until hold-REC; on return SD is re-mounted.
// ---------------------------------------------------------------------------
void enterMscMode() {
  // Unmount filesystem so the host gets exclusive block-level access.
  SD_MMC.end();
  delay(50);

  // Connect MSC to the host.
  msc.mediaPresent(true);

  // Show the full-screen MSC UI.
  showUsbMsc();

  // Block until hold-REC.
  bool     btnWasDown = false;
  uint32_t btnDownAt  = 0;
  bool     done       = false;

  while (!done) {
    delay(20);
    bool btnDown = (digitalRead(BTN_REC) == LOW);
    if (btnDown && !btnWasDown) {
      btnDownAt  = millis();
      btnWasDown = true;
    } else if (btnWasDown) {
      uint32_t held = millis() - btnDownAt;
      if (!btnDown) {
        btnWasDown = false;  // released before threshold — ignore
      } else if (held >= (uint32_t)BTN_LONG_MS) {
        done = true;         // long-held — exit MSC mode
      }
    }
  }

  // Disconnect MSC from host.
  msc.mediaPresent(false);
  delay(200);  // give host a moment to notice media gone

  // Re-mount SD filesystem.
  SD_MMC.begin("/sdcard", true);
  delay(50);
  // Caller is responsible for calling loadIndex() after this returns.
}
