#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tinyusb.h"
#include "tinyusb_default_config.h"
#include "usb_descriptors.h"

#define WII_I2C   I2C_NUM_0
#define WII_SDA   GPIO_NUM_8
#define WII_SCL   GPIO_NUM_9
#define WII_ADDR  0x52

extern void bridge_usb_poll(void);
extern void bridge_submit_wii(const uint8_t *data, bool valid);
static const char *TAG = "wup028_wii_ext";

/* Keep the exact USB scheduling scheme of the working USB-only diagnostic. */
static void usb_report_timer(void *arg) {
    (void)arg;
    bridge_usb_poll();
}

static esp_err_t write_extension(uint8_t reg, uint8_t value) {
    uint8_t data[2] = {reg, value};
    return i2c_master_write_to_device(WII_I2C, WII_ADDR,
                                     data, sizeof(data), pdMS_TO_TICKS(20));
}

static esp_err_t read_extension(uint8_t reg, uint8_t *data, size_t length) {
    return i2c_master_write_read_device(WII_I2C, WII_ADDR,
                                        &reg, 1, data, length, pdMS_TO_TICKS(20));
}

static bool extension_init(void) {
    uint8_t id[6] = {0};
    if (write_extension(0xF0, 0x55) != ESP_OK) return false;
    vTaskDelay(pdMS_TO_TICKS(10));
    if (write_extension(0xFB, 0x00) != ESP_OK) return false;
    vTaskDelay(pdMS_TO_TICKS(10));
    if (read_extension(0xFA, id, sizeof(id)) != ESP_OK) return false;
    ESP_LOGI(TAG, "Wii-Ext ID %02X %02X %02X %02X %02X %02X",
             id[0], id[1], id[2], id[3], id[4], id[5]);
    /* Default format 0x01 of the Wii Classic Controller. */
    if (id[5] != 0x01) {
        ESP_LOGW(TAG, "Unknown extension format; expected ID ending 01");
        return false;
    }
    return true;
}

static void i2c_reader_task(void *arg) {
    (void)arg;
    bool online = false;
    unsigned failures = 0;
    uint8_t data[6];
    TickType_t next_poll = xTaskGetTickCount();
    bridge_submit_wii(NULL, false);
    while (1) {
        if (!online) {
            bridge_submit_wii(NULL, false);
            online = extension_init();
            if (!online) {
                vTaskDelay(pdMS_TO_TICKS(500));
                next_poll = xTaskGetTickCount();
                continue;
            }
            failures = 0;
            ESP_LOGI(TAG, "BlueRetro Wii-Ext connected");
            next_poll = xTaskGetTickCount();
        }
        if (read_extension(0x00, data, sizeof(data)) == ESP_OK) {
            failures = 0;
            bridge_submit_wii(data, true);
        } else if (++failures >= 3) {
            ESP_LOGW(TAG, "BlueRetro I2C lost: neutral state, reconnecting");
            online = false;
            bridge_submit_wii(NULL, false);
        }
        vTaskDelayUntil(&next_poll, pdMS_TO_TICKS(4)); /* about 250 Hz */
    }
}

void app_main(void) {
    /* Do USB before I2C so the adapter appears even if BlueRetro is offline. */
    tinyusb_config_t cfg = TINYUSB_DEFAULT_CONFIG();
    cfg.descriptor.device = &gcca_device_desc;
    cfg.descriptor.full_speed_config = gcca_configuration_desc;
    cfg.descriptor.string = gcca_string_desc;
    cfg.descriptor.string_count = gcca_string_desc_count;
    ESP_ERROR_CHECK(tinyusb_driver_install(&cfg));

    const esp_timer_create_args_t timer_cfg = {
        .callback = usb_report_timer,
        .name = "wup028_reports",
    };
    esp_timer_handle_t timer;
    ESP_ERROR_CHECK(esp_timer_create(&timer_cfg, &timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(timer, 1000));

    i2c_config_t i2c_cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = WII_SDA,
        .scl_io_num = WII_SCL,
        .sda_pullup_en = GPIO_PULLUP_DISABLE,
        .scl_pullup_en = GPIO_PULLUP_DISABLE,
        .master.clk_speed = 100000,
    };
    ESP_ERROR_CHECK(i2c_param_config(WII_I2C, &i2c_cfg));
    ESP_ERROR_CHECK(i2c_driver_install(WII_I2C, i2c_cfg.mode, 0, 0, 0));
    BaseType_t ok = xTaskCreate(i2c_reader_task, "wii_ext_i2c", 4096,
                                NULL, 4, NULL);
    if (ok != pdPASS) ESP_LOGE(TAG, "Could not create I2C task");
    else ESP_LOGI(TAG, "USB ready, I2C BlueRetro on SDA=8 SCL=9");
}
