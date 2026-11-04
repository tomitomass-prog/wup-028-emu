#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "tinyusb.h"
#include "tinyusb_default_config.h"
#include "usb_descriptors.h"

extern void bridge_usb_poll(void);

static void usb_report_timer(void *arg) {
    (void)arg;
    bridge_usb_poll();
}

void app_main(void) {
    static const char *TAG = "wup028_diag";
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
    ESP_LOGI(TAG, "USB-only diagnostic started; port 1 always connected; no I2C");
}
