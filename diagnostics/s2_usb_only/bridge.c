#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "tusb.h"
#include "report_ids.h"
#include "classic_decode.h"

/* USB deliberately follows the previously working s2_usb_only diagnostic.
 * It always advertises port 1 as wired, even when the I2C source is offline,
 * to keep Nintendont's USB detection independent of BlueRetro availability.
 */
static const char *TAG = "wup028_wii_ext";
static volatile bool polling_enabled = false;
static portMUX_TYPE state_lock = portMUX_INITIALIZER_UNLOCKED;
static uint8_t latest_port[9] = {0x10, 0, 0, 128, 128, 128, 128, 0, 0};

void bridge_submit_wii(const uint8_t *data, bool valid) {
    uint8_t next[9];
    if (valid && data) classic_decode(data, next);
    else classic_neutral(next);
    portENTER_CRITICAL(&state_lock);
    memcpy(latest_port, next, sizeof(next));
    portEXIT_CRITICAL(&state_lock);
}

static void send_ack(uint8_t report_id, uint8_t success) {
    if (!tud_hid_ready()) return;
    uint8_t ack[2] = {report_id, success};
    (void)tud_hid_report(0, ack, sizeof(ack));
}

static void send_origin(void) {
    if (!tud_hid_ready()) return;
    uint8_t origin[25] = {0};
    origin[0] = RPT_ORIGIN;
    for (int p = 0; p < 4; ++p) {
        origin[1 + p * 6 + 0] = 128;
        origin[1 + p * 6 + 1] = 128;
        origin[1 + p * 6 + 2] = 128;
        origin[1 + p * 6 + 3] = 128;
    }
    (void)tud_hid_report(0, origin, sizeof(origin));
}

void bridge_usb_poll(void) {
    if (!polling_enabled || !tud_hid_ready()) return;
    uint8_t report[37] = {0};
    report[0] = RPT_STATE;
    portENTER_CRITICAL(&state_lock);
    memcpy(&report[1], latest_port, 9);
    portEXIT_CRITICAL(&state_lock);
    (void)tud_hid_report(0, report, sizeof(report));
}

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
                              hid_report_type_t type, uint8_t *buffer,
                              uint16_t reqlen) {
    (void)instance; (void)report_id; (void)type;
    (void)buffer; (void)reqlen;
    return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                           hid_report_type_t type, uint8_t const *buffer,
                           uint16_t size) {
    (void)instance; (void)report_id;
    if (type != HID_REPORT_TYPE_OUTPUT || !buffer || size == 0) return;
    /* Same command handling as the successful diagnostic firmware. */
    switch (buffer[0]) {
        case RPT_ENABLE_POLLING:
            ESP_LOGI(TAG, "USB: enable polling 0x13");
            send_ack(RPT_POLLING_ENABLED, !polling_enabled);
            polling_enabled = true;
            break;
        case RPT_DISABLE_POLLING:
            ESP_LOGI(TAG, "USB: disable polling 0x14");
            send_ack(RPT_POLLING_DISABLED, polling_enabled);
            polling_enabled = false;
            break;
        case RPT_GET_ORIGIN:
            send_origin();
            break;
        case RPT_SET_MOTOR:
            /* Rumble transport back to BlueRetro not implemented. */
            break;
        case RPT_RESET:
            ESP_LOGW(TAG, "USB reset command 0x15");
            esp_restart();
            break;
        default:
            ESP_LOGW(TAG, "Unknown USB command: 0x%02x", buffer[0]);
            break;
    }
}
