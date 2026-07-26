#include "Arduino.h"
#include "SD_MMC.h"
#include "USB.h"
#include "USBMSC.h"
#include "../../config.h"
#include "../../globals.h"
#include "ui.h"
#include "usb_msc.h"

// ---------------------------------------------------------------------------
// RTC_NOINIT survives ESP.restart() and deep-sleep wakeup.
// Cleared only on power-on reset (battery pull / first power-up).
// Magic value guards against uninitialized RAM looking like a valid flag.
// ---------------------------------------------------------------------------
#define MSC_BOOT_MAGIC  0xA5C3E1B2u

RTC_NOINIT_ATTR uint32_t sMscBootMagic;

// ---------------------------------------------------------------------------
// MSC callbacks — SD_MMC is re-init'd in raw mode before these are called.
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
// usb_msc_check_boot_flag
// Call at the very top of setup(), before any other peripheral init.
// If the magic is set, we were rebooted into MSC mode:
//   1. Init SD in raw mode (no filesystem mount needed).
//   2. Register MSC + start USB as a pure mass-storage device.
//   3. Show the USB screen and block until hold-REC.
//   4. Clear the flag and reboot back to normal.
// This function never returns when the flag is set.
// ---------------------------------------------------------------------------
void usb_msc_check_boot_flag() {
  if (sMscBootMagic != MSC_BOOT_MAGIC) return;

  // Flag is set — run MSC-only session.
  // We must NOT call keepBatteryPowerOn() here; the caller (setup) must
  // handle the power latch BEFORE calling this function so we stay on.

  // Init SD in 1-bit mode; we only need raw sector access.
  SD_MMC.setPins(SD_CLK, SD_CMD, SD_D0);
  SD_MMC.begin("/sdcard", true);
  uint32_t sectorCount = (uint32_t)(SD_MMC.cardSize() / 512ULL);
  SD_MMC.end();  // unmount FS — give host exclusive block access
  delay(50);

  // Register MSC and start USB as mass-storage only (no CDC).
  static USBMSC msc;
  msc.vendorID("Amar");
  msc.productID("Note SD Card");
  msc.productRevision("1.0");
  msc.onRead(mscRead);
  msc.onWrite(mscWrite);
  msc.mediaPresent(true);
  msc.begin(sectorCount, 512);
  USB.begin();

  // Show MSC screen (minimal init — display must be set up by caller first).
  showUsbMsc();

  // Block until hold-REC.
  bool     btnWasDown = false;
  uint32_t btnDownAt  = 0;

  while (true) {
    delay(20);
    bool btnDown = (digitalRead(BTN_REC) == LOW);
    if (btnDown && !btnWasDown) {
      btnDownAt  = millis();
      btnWasDown = true;
    } else if (btnWasDown) {
      if (!btnDown) {
        btnWasDown = false;
      } else if ((millis() - btnDownAt) >= (uint32_t)BTN_LONG_MS) {
        break;
      }
    }
  }

  // Clear flag and reboot into normal mode.
  sMscBootMagic = 0;
  delay(200);  // let host see media-gone before reboot
  ESP.restart();
  // never returns
}

// ---------------------------------------------------------------------------
// enterMscMode — called from the menu.
// Sets the RTC flag and reboots; does not return.
// ---------------------------------------------------------------------------
void enterMscMode() {
  sMscBootMagic = MSC_BOOT_MAGIC;
  delay(50);
  ESP.restart();
}
