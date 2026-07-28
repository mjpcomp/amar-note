#include "Arduino.h"
#include "SD_MMC.h"
#include "USB.h"
#include "USBMSC.h"
#include "esp_task_wdt.h"
#include "driver/sdmmc_host.h"
#include "sdmmc_cmd.h"
#include "../../config.h"
#include "../../globals.h"
#include "ui.h"
#include "usb_msc.h"

// ---------------------------------------------------------------------------
// SDMMCAccessor: thin subclass to expose the protected _card pointer.
// SD_MMC is a global SDMMCFS instance; we reinterpret-cast its address to
// SDMMCAccessor* (same memory layout) to read _card without patching the lib.
// ---------------------------------------------------------------------------
class SDMMCAccessor : public fs::SDMMCFS {
public:
  sdmmc_card_t* getCard() { return _card; }
};

static sdmmc_card_t* getSDCard() {
  return reinterpret_cast<SDMMCAccessor*>(&SD_MMC)->getCard();
}

// ---------------------------------------------------------------------------
// RTC_NOINIT survives ESP.restart() and deep-sleep wakeup.
// Magic value guards against uninitialized RAM looking like a valid flag.
// ---------------------------------------------------------------------------
#define MSC_BOOT_MAGIC  0xA5C3E1B2u

RTC_NOINIT_ATTR uint32_t sMscBootMagic;

static sdmmc_card_t* sCard = nullptr;

// ---------------------------------------------------------------------------
// MSC callbacks — ESP-IDF raw sector API.
// ---------------------------------------------------------------------------
static int32_t mscRead(uint32_t lba, uint32_t offset, void* buf, uint32_t bufSize) {
  (void)offset;
  if (!sCard) return -1;
  uint32_t sectors = bufSize / 512;
  if (sectors == 0) sectors = 1;
  return (sdmmc_read_sectors(sCard, buf, lba, sectors) == ESP_OK)
    ? (int32_t)bufSize : -1;
}

static int32_t mscWrite(uint32_t lba, uint32_t offset, uint8_t* buf, uint32_t bufSize) {
  (void)offset;
  if (!sCard) return -1;
  uint32_t sectors = bufSize / 512;
  if (sectors == 0) sectors = 1;
  return (sdmmc_write_sectors(sCard, buf, lba, sectors) == ESP_OK)
    ? (int32_t)bufSize : -1;
}

// ---------------------------------------------------------------------------
// usb_msc_check_boot_flag — call at top of setup(), before other peripherals.
// Never returns when the boot flag is set.
// ---------------------------------------------------------------------------
void usb_msc_check_boot_flag() {
  if (sMscBootMagic != MSC_BOOT_MAGIC) return;

  // Mount SD so we can read cardSize() and the card handle.
  // Do NOT call SD_MMC.end() afterwards — that deinits the SDMMC host
  // and invalidates sCard. The FAT layer stays idle while USB owns blocks.
  SD_MMC.setPins(SD_CLK, SD_CMD, SD_D0);
  if (!SD_MMC.begin("/sdcard", true /*mode1bit*/)) {
    sMscBootMagic = 0;  // bail cleanly — don't loop
    return;
  }

  uint32_t sectorCount = (uint32_t)(SD_MMC.cardSize() / 512ULL);
  sCard = getSDCard();

  if (!sCard || sectorCount == 0) {
    sMscBootMagic = 0;
    SD_MMC.end();
    return;
  }

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

  // Show USB screen (display already initialised by setup() before this call).
  showUsbMsc();

  // Spin until hold-REC, feeding the watchdog so it never fires.
  bool     btnWasDown = false;
  uint32_t btnDownAt  = 0;

  while (true) {
    esp_task_wdt_reset();
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

  // Signal media gone, clear flag, reboot to normal mode.
  msc.mediaPresent(false);
  sMscBootMagic = 0;
  delay(300);
  ESP.restart();
}

// ---------------------------------------------------------------------------
// enterMscMode — called from menu. Sets RTC flag and reboots.
// ---------------------------------------------------------------------------
void enterMscMode() {
  sMscBootMagic = MSC_BOOT_MAGIC;
  delay(50);
  ESP.restart();
}
