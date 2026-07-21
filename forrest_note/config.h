#ifndef CONFIG_H
#define CONFIG_H

// ---------------------------------------------------------------------------
// EPD (e-Paper Display) — Waveshare ESP32-S3-ePaper-1.54 official pins
// Source: 02_Example/Arduino/09_LVGL_V8_Test/user_config.h
// ---------------------------------------------------------------------------
#define EPD_SPI_NUM          SPI2_HOST
#define EPD_WIDTH            200
#define EPD_HEIGHT           200
#define LVGL_SPIRAM_BUFF_LEN (EPD_WIDTH * EPD_HEIGHT * 2)

#define EPD_DC_PIN    GPIO_NUM_10
#define EPD_CS_PIN    GPIO_NUM_11
#define EPD_SCK_PIN   GPIO_NUM_12
#define EPD_MOSI_PIN  GPIO_NUM_13
#define EPD_RST_PIN   GPIO_NUM_9
#define EPD_BUSY_PIN  GPIO_NUM_8

// ---------------------------------------------------------------------------
// Power control pins
// ---------------------------------------------------------------------------
#define EPD_PWR_PIN     GPIO_NUM_6
#define AUDIO_PWR_PIN   GPIO_NUM_42
#define VBAT_PWR_PIN    GPIO_NUM_17

// ---------------------------------------------------------------------------
// Buttons
// ---------------------------------------------------------------------------
#define BOOT_BUTTON_PIN  GPIO_NUM_0
#define PWR_BUTTON_PIN   GPIO_NUM_18

// Aliases for firmware code
#define BTN_REC  BOOT_BUTTON_PIN
#define BTN_PWR  PWR_BUTTON_PIN

#define BTN_DEBOUNCE_MS  12
#define BTN_LONG_MS      450
#define BTN_VLONG_MS     1200

// ---------------------------------------------------------------------------
// SD Card — SDMMC 1-bit mode
// Source: 02_Example/Arduino/04_SD_Card/sdcard_bsp.cpp
// ---------------------------------------------------------------------------
#define SD_CLK  GPIO_NUM_39
#define SD_CMD  GPIO_NUM_41
#define SD_D0   GPIO_NUM_40

// ---------------------------------------------------------------------------
// I2C — official Waveshare pins
// Source: 02_Example/Arduino/09_LVGL_V8_Test/user_config.h
// ---------------------------------------------------------------------------
#define ESP32_I2C_SDA_PIN  GPIO_NUM_47
#define ESP32_I2C_SCL_PIN  GPIO_NUM_48

// Aliases
#define I2C_SDA  ESP32_I2C_SDA_PIN
#define I2C_SCL  ESP32_I2C_SCL_PIN

// I2C device addresses
#define I2C_RTC_DEV_ADDRESS    0x51
#define I2C_SHTC3_DEV_ADDRESS  0x70

// ---------------------------------------------------------------------------
// I2S (audio) — retained from prior, align with codec_board BSP as needed
// ---------------------------------------------------------------------------
#define I2S_BCK  GPIO_NUM_15
#define I2S_WS   GPIO_NUM_16
#define I2S_DI   GPIO_NUM_14
#define I2S_DO   GPIO_NUM_7

// ---------------------------------------------------------------------------
// LVGL timing
// ---------------------------------------------------------------------------
#define EXAMPLE_LVGL_TICK_PERIOD_MS     5
#define EXAMPLE_LVGL_TASK_MAX_DELAY_MS  500
#define EXAMPLE_LVGL_TASK_MIN_DELAY_MS  100

// ---------------------------------------------------------------------------
// Wake-up pin (RTC deep-sleep)
// ---------------------------------------------------------------------------
#define EXT_WAKEUP_PIN  GPIO_NUM_0

// ---------------------------------------------------------------------------
// Firmware / app config
// ---------------------------------------------------------------------------
#define LOOP_DELAY_MS     4
#define FIRMWARE_VERSION  "1.1.0-amar"
#define DEVICE_NAME       "Amar Note"

#define SAMPLE_RATE       16000
#define RECORD_BUF_MS     40
#define RECORD_MAX_S      120

#define BATTERY_PIN        GPIO_NUM_1
#define BATTERY_ADC_SCALE  2.0f

#define NVS_NAMESPACE  "forrest"

#endif // CONFIG_H
