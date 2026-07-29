#include "touch.h"
#include "../../config.h"
#include "../../src/i2c_bsp/i2c_bsp.h"
#include "Arduino.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"

// ── FT6336 register map (minimal) ─────────────────────────────────────────
#define FT_REG_TOUCH_COUNT  0x02   // number of active touch points (0 or 1)
#define FT_REG_P1_XH        0x03   // P1 XH, XL, YH, YL  (4 bytes)

// Panel native resolution (before coordinate mapping)
#define PANEL_MAX  320

// De-bounce: ignore a second tap within this many milliseconds
#define TOUCH_DEBOUNCE_MS  120

static i2c_master_dev_handle_t ft_dev = NULL;
static uint32_t lastTouchMs = 0;

// ── Reset the FT6336 via its RST line ──────────────────────────────────
static void ft_reset(void) {
    gpio_config_t io = {};
    io.intr_type     = GPIO_INTR_DISABLE;
    io.pin_bit_mask  = (1ULL << EPD_TP_RST_PIN);
    io.mode          = GPIO_MODE_OUTPUT;
    io.pull_up_en    = GPIO_PULLUP_DISABLE;
    io.pull_down_en  = GPIO_PULLDOWN_DISABLE;
    gpio_config(&io);

    gpio_set_level((gpio_num_t)EPD_TP_RST_PIN, 1); delay(10);
    gpio_set_level((gpio_num_t)EPD_TP_RST_PIN, 0); delay(20);
    gpio_set_level((gpio_num_t)EPD_TP_RST_PIN, 1); delay(50);
}

// ── Low-level register read via the shared i2c_master bus ──────────────
static esp_err_t ft_read(uint8_t reg, uint8_t *buf, uint8_t len) {
    return (esp_err_t)i2c_read_buff(ft_dev, reg, buf, len);
}

// ── Public API ────────────────────────────────────────────────────────
void touchInit(void) {
    ft_reset();

    // Register the FT6336 on the already-initialised I2C master bus.
    // i2c_bsp exposes the bus handle indirectly through its add-device
    // helpers; we replicate the same pattern used for the RTC.
    extern i2c_master_bus_handle_t _i2c_get_bus_handle(void);

    // The bus handle is file-scoped in i2c_bsp.c; we add a small accessor
    // declared just below — see i2c_bsp.h for the extern.
    i2c_device_config_t dev_cfg = {};
    dev_cfg.dev_addr_length  = I2C_ADDR_BIT_LEN_7;
    dev_cfg.device_address   = I2C_FT6336_DEV_Address;
    dev_cfg.scl_speed_hz     = 400000;

    // Use the exported bus-registration helper that mirrors how the RTC
    // was added (i2c_master_bus_add_device via the bus stored in i2c_bsp).
    i2c_touch_register_device(&dev_cfg, &ft_dev);

    Serial.println("[touch] FT6336 init OK");
}

// touchPoll — renamed from touchRead to avoid clashing with the ESP32
// Arduino core 3.x declaration: touch_value_t touchRead(uint8_t pin)
// in esp32-hal-touch-ng.h.
bool touchPoll(uint16_t *sx, uint16_t *sy) {
    if (ft_dev == NULL) return false;

    uint8_t count = 0;
    if (ft_read(FT_REG_TOUCH_COUNT, &count, 1) != ESP_OK) return false;
    if (count == 0) return false;

    uint8_t buf[4];
    if (ft_read(FT_REG_P1_XH, buf, 4) != ESP_OK) return false;

    uint16_t raw_x = (uint16_t)((buf[0] & 0x0F) << 8) | buf[1];
    uint16_t raw_y = (uint16_t)((buf[2] & 0x0F) << 8) | buf[3];

    // Panel is portrait-rotated 90° CCW relative to the ePaper framebuffer.
    // Mapping verified against the Tamagotchi build on the same hardware:
    //   screen_x = 199 - scale(raw_y)
    //   screen_y =       scale(raw_x)
    uint16_t screen_x = (uint16_t)(199 - (raw_y * 200 / PANEL_MAX));
    uint16_t screen_y = (uint16_t)(raw_x * 200 / PANEL_MAX);

    if (screen_x > 199) screen_x = 199;
    if (screen_y > 199) screen_y = 199;

    // De-bounce: swallow rapid repeats
    uint32_t now = millis();
    if (now - lastTouchMs < TOUCH_DEBOUNCE_MS) return false;
    lastTouchMs = now;

    *sx = screen_x;
    *sy = screen_y;
    return true;
}

bool touchHitTest(uint16_t sx, uint16_t sy,
                  int rx, int ry, int rw, int rh) {
    return (sx >= (uint16_t)rx && sx < (uint16_t)(rx + rw) &&
            sy >= (uint16_t)ry && sy < (uint16_t)(ry + rh));
}
