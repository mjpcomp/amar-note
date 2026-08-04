#ifndef CONFIG_H
#define CONFIG_H

#define EPD_SPI_NUM        SPI2_HOST
#define ESP32_I2C_DEV_NUM  I2C_NUM_0

#define EPD_WIDTH  200
#define EPD_HEIGHT 200
#define LVGL_SPIRAM_BUFF_LEN (EPD_WIDTH * EPD_HEIGHT * 2)

/* EPD SPI pins */
#define EPD_DC_PIN    GPIO_NUM_10
#define EPD_CS_PIN    GPIO_NUM_11
#define EPD_SCK_PIN   GPIO_NUM_12
#define EPD_MOSI_PIN  GPIO_NUM_13
#define EPD_RST_PIN   GPIO_NUM_9
#define EPD_BUSY_PIN  GPIO_NUM_8

/* Touch controller (FT6336) */
#define EPD_TP_INT_PIN          GPIO_NUM_21   // touch interrupt (active-low, currently polled)
#define EPD_TP_RST_PIN          GPIO_NUM_7    // touch reset
#define I2C_FT6336_DEV_Address  0x38          // FT6336 default I2C address

/* Power control pins */
#define EPD_PWR_PIN     GPIO_NUM_6
#define Audio_PWR_PIN   GPIO_NUM_42
#define VBAT_PWR_PIN    GPIO_NUM_17
#define BAT_ADC_PIN     4   // ADC1_CHANNEL_3 on ESP32-S3

#define BOOT_BUTTON_PIN GPIO_NUM_0
#define PWR_BUTTON_PIN  GPIO_NUM_18

/* Deep-sleep wake-up pin */
#define ext_wakeup_pin_1 GPIO_NUM_0

/* I2C bus */
#define ESP32_I2C_SDA_PIN GPIO_NUM_47
#define ESP32_I2C_SCL_PIN GPIO_NUM_48

/* LVGL tick timing */
#define EXAMPLE_LVGL_TICK_PERIOD_MS    5
#define EXAMPLE_LVGL_TASK_MAX_DELAY_MS 500
#define EXAMPLE_LVGL_TASK_MIN_DELAY_MS 100

/* I2C peripheral addresses */
#define I2C_RTC_DEV_Address        0x51
#define I2C_SHTC3_DEV_Address      0x70

/* Button GPIO aliases */
#define BTN_REC      0
#define BTN_PWR      18
#define PWR_HOLD_PIN 17

/* SD-MMC pins */
#define SD_CLK  39
#define SD_CMD  41
#define SD_D0   40

/* Audio */
#define SAMPLE_RATE      16000
#define REC_BUF          (8 * 1024)
#define MAX_REC_MS       (5UL * 60UL * 1000UL)   // hard cap on a single recording (5 min)
#define REC_RING_LEN     (96 * 1024)              // PSRAM ring to absorb SD write stalls
#define MIC_GAIN_DB      45.0f                    // ES7210 input gain
#define SPK_VOL_MAX      100.0f                   // ES8311 output volume ceiling
#define MIN_NOTE_SECONDS 1                        // recordings shorter than this are discarded

/* Minimum recording threshold (min rec setting) */
// 0 = off (keep all recordings), otherwise discard recordings shorter than this many seconds.
#define MIN_REC_SECS_DEFAULT  0

/* Storage paths */
#define NOTES_DIR  "/notes"
#define INDEX_FILE "/notes/index.csv"
#define TAG_FILE   "/notes/tags.txt"
#define TOMBS_FILE "/notes/tombs.csv"   // pending vault deletes (uid,tag per line)
#define MAX_TAGS   20

/* NVS (Non-Volatile Storage) namespace — all runtime config keys live here */
#define NVS_NAMESPACE  "amar"

/* SoftAP setup portal SSID — shown when device has no Wi-Fi credentials */
#define SETUP_SSID     "AmarNote-Setup"

/* UI timing */
#define REC_HOLD_MS         350
#define BTN_LONG_MS         450     // hold threshold for "back"/secondary (lower = snappier)
#define BTN_DEBOUNCE_MS     12      // press must persist this long to count
#define LOOP_DELAY_MS       4       // main-loop poll period (was 15; lower = more responsive)
#define ULTRA_SLEEP_MS      120000UL
#define TICKER_INTERVAL_MS  950

/* Battery warning */
#define BAT_CHECK_INTERVAL_MS  30000
#define BAT_LOW_THRESHOLD      15
#define BAT_RECOVER_THRESHOLD  20

// ---------------------------------------------------------------------------
// Timezone  —  POSIX TZ string
// ---------------------------------------------------------------------------
#define DEVICE_TZ_POSIX  "PST8PDT,M3.2.0,M11.1.0"   // US Pacific (UTC-8 / UTC-7 DST)

// ---------------------------------------------------------------------------
// Settings menu row count
// ---------------------------------------------------------------------------
// Bump this when adding or removing rows from the showSettings() rendering
// and corresponding input handler in amar_note.ino.
//
// Rows (0-based):
//   0 Sounds | 1 Transfer | 2 Device | 3 Idle Rec | 4 Min Rec | 5 Reset
//
#define SETTINGS_COUNT  6

/* Firmware version — bump here only; FW_VERSION is an alias for back-compat */
#define FIRMWARE_VERSION  "v1.5.5"
#define FW_VERSION        FIRMWARE_VERSION

/* OTA update source — GitHub releases for this repo */
#define OTA_GITHUB_OWNER  "mjpcomp"
#define OTA_GITHUB_REPO   "amar-note"

/* ── Nekogotchi (virtual pet) ──────────────────────────────────────────────── */
/* SD save file */
#define PET_FILE            "/pet.dat"
/* Starting stats (0-100) */
#define PET_START_HUNGER    85
#define PET_START_HAPPY     85
#define PET_START_ENERGY    90
/* Hourly decay / recovery rates */
#define PET_DECAY_HUNGER_PH  3
#define PET_DECAY_HAPPY_PH   2
#define PET_RECOVER_ENERGY_PH 8
/* Action deltas */
#define PET_FEED_HUNGER     50
#define PET_PLAY_HAPPY      20
#define PET_PLAY_ENERGY     15
#define PET_PET_HAPPY       10
/* Max offline hours applied at once (prevents instant death after long sleep) */
#define PET_ELAPSED_CAP_H   48
/* Millis an action-pose sprite is shown before returning to main view */
#define PET_ACTION_MS       1400
/* Mood thresholds */
#define PET_TH_HUNGRY       30
#define PET_TH_SLEEP_ENERGY 20
#define PET_TH_SAD          15
#define PET_TH_HAPPY        70
/* Sprite position on screen */
#define PET_SPRITE_X        25
#define PET_SPRITE_Y        26
/* Action-bar Y position */
#define PET_BAR_Y           178

#endif // CONFIG_H
