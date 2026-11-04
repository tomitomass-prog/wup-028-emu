#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "tinyusb.h"
#include "tinyusb_default_config.h"
#include "usb_descriptors.h"

#define WII_I2C I2C_NUM_0
#define SDA_PIN GPIO_NUM_8
#define SCL_PIN GPIO_NUM_9
#define WII_ADDR 0x52

extern void bridge_usb_poll(void);
extern void bridge_submit_wii(const uint8_t data[6], bool connected);

static const char *TAG = "wii_to_wup028";

static esp_err_t wr(const uint8_t *data, size_t len) {
  return i2c_master_write_to_device(WII_I2C, WII_ADDR, data, len, pdMS_TO_TICKS(30));
}
static esp_err_t read_reg(uint8_t reg, uint8_t *out, size_t n) {
  return i2c_master_write_read_device(WII_I2C, WII_ADDR, &reg, 1, out, n, pdMS_TO_TICKS(30));
}
static bool extension_init(void) {
  const uint8_t a[] = {0xF0, 0x55};
  const uint8_t b[] = {0xFB, 0x00};
  uint8_t id[6];
  if (wr(a, 2) != ESP_OK) return false;
  vTaskDelay(pdMS_TO_TICKS(10));
  if (wr(b, 2) != ESP_OK) return false;
  vTaskDelay(pdMS_TO_TICKS(10));
  if (read_reg(0xFA, id, 6) != ESP_OK) return false;
  ESP_LOGI(TAG, "Wii Extension ID: %02x %02x %02x %02x %02x %02x", id[0],id[1],id[2],id[3],id[4],id[5]);
  // 0x01 identifies the Classic Controller data format.
  return id[5] == 0x01;
}

void app_main(void) {
  i2c_config_t conf = { .mode=I2C_MODE_MASTER, .sda_io_num=SDA_PIN, .scl_io_num=SCL_PIN,
                        .sda_pullup_en=GPIO_PULLUP_DISABLE, .scl_pullup_en=GPIO_PULLUP_DISABLE,
                        .master.clk_speed=100000 };
  ESP_ERROR_CHECK(i2c_param_config(WII_I2C, &conf));
  ESP_ERROR_CHECK(i2c_driver_install(WII_I2C, conf.mode, 0, 0, 0));

  tinyusb_config_t tusb_cfg = TINYUSB_DEFAULT_CONFIG();
  tusb_cfg.descriptor.device = &gcca_device_desc;
  tusb_cfg.descriptor.full_speed_config = gcca_configuration_desc;
  tusb_cfg.descriptor.string = gcca_string_desc;
  tusb_cfg.descriptor.string_count = gcca_string_desc_count;
  ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));

  bool online=false;
  uint8_t data[6];
  int bad=0;
  TickType_t tick=xTaskGetTickCount();
  uint32_t count=0;
  while (1) {
    if (!online) {
      online=extension_init();
      bridge_submit_wii(NULL, false);
      if (!online) {
        vTaskDelay(pdMS_TO_TICKS(250));
        continue;
      }
      ESP_LOGI(TAG, "Classic Controller connection established");
    }
    if (read_reg(0x00, data, sizeof(data)) == ESP_OK) {
      bad=0;
      bridge_submit_wii(data, true);
      if ((++count % 500)==0) ESP_LOGI(TAG,"Classic data: %02x %02x %02x %02x %02x %02x", data[0],data[1],data[2],data[3],data[4],data[5]);
    } else if (++bad>=5) {
      online=false;
      bridge_submit_wii(NULL, false);
      ESP_LOGW(TAG,"extension lost; retrying");
    }
    bridge_usb_poll();
    vTaskDelayUntil(&tick, pdMS_TO_TICKS(4));  // ~250 Hz input
  }
}
