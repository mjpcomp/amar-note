#ifndef CONFIG_H
#define CONFIG_H

#define EPD_SPI_NUM        SPI2_HOST
#define EPD_PIN_CS         5
#define EPD_PIN_DC         6
#define EPD_PIN_RST        7
#define EPD_PIN_BUSY       8
#define EPD_PIN_CLK        3
#define EPD_PIN_MOSI       2
#define EPD_PIN_MISO       -1

#define I2S_BCK            9
#define I2S_WS             10
#define I2S_DO             11
#define I2S_DI             12
#define I2C_SDA            17
#define I2C_SCL            18  // also BTN_PWR pull-up rail — keep OUTPUT-safe

#define SD_CMD             38
#define SD_CLK             39
#define SD_D0              40

#define BTN_REC            0   // BOOT button
#define BTN_PWR            18

#define BTN_DEBOUNCE_MS    12
#define BTN_LONG_MS        450
#define BTN_VLONG_MS       1200

#define LOOP_DELAY_MS      4

#define FIRMWARE_VERSION   "1.1.0-amar"
#define DEVICE_NAME        "Amar Note"

#define SAMPLE_RATE        16000
#define RECORD_BUF_MS      40
#define RECORD_MAX_S       120

#define BATTERY_PIN        1
#define BATTERY_ADC_SCALE  2.0f

#define NVS_NAMESPACE      "forrest"

#endif // CONFIG_H
