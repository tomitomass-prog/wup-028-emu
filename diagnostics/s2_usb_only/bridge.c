#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "esp_log.h"
#include "esp_system.h"
#include "tusb.h"
#include "report_ids.h"

/* WUP-028 diagnostic only. A wired GC pad is always present on port 1.
 * Preserve upstream report format: one leading report byte plus 4 x 9-byte ports.
 * No I2C, no external controller, no rumble.
 */
static volatile bool polling_enabled;
static uint8_t report_buf[37];
static uint32_t report_count;
static const char *TAG = "wup028_diag";

static void send_ack(uint8_t report_id, uint8_t success) {
    if (!tud_hid_ready()) return;
    uint8_t ack[2] = { report_id, success };
    (void)tud_hid_report(0, ack, sizeof(ack));
}

static void send_origin(void) {
    if (!tud_hid_ready()) return;
    uint8_t origin[25] = {0};
    origin[0] = RPT_ORIGIN;
    for (int port = 0; port < 4; ++port) {
        origin[1 + port * 6 + 0] = 128;
        origin[1 + port * 6 + 1] = 128;
        origin[1 + port * 6 + 2] = 128;
        origin[1 + port * 6 + 3] = 128;
    }
    (void)tud_hid_report(0, origin, sizeof(origin));
}

void bridge_usb_poll(void) {
    if (!polling_enabled || !tud_hid_ready()) return;
    memset(report_buf, 0, sizeof(report_buf));
    report_buf[0] = RPT_STATE;
    report_buf[1] = 0x10; /* wired controller on port 1 */
    report_buf[4] = 128;  /* left X */
    report_buf[5] = 128;  /* left Y */
    report_buf[6] = 128;  /* C X */
    report_buf[7] = 128;  /* C Y */
    /* Brief A press every 4 seconds; useful for testing without GameCube pad.
     * Never use this diagnostic build as a regular controller.
     */
    ++report_count;
    if ((report_count % 4000U) < 200U) report_buf[2] |= 0x01;
    (void)tud_hid_report(0, report_buf, sizeof(report_buf));
}

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
                               hid_report_type_t type, uint8_t *buffer, uint16_t reqlen) {
    (void)instance; (void)report_id; (void)type;
    (void)buffer; (void)reqlen;
    return 0; /* match upstream implementation */
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                           hid_report_type_t type, uint8_t const *buffer, uint16_t size) {
    (void)instance; (void)report_id;
    if (type != HID_REPORT_TYPE_OUTPUT || !buffer || size == 0) return;
    /* Same parsing as upstream loopj gcca.c: OUT payload starts with command. */
    const uint8_t command = buffer[0];
    switch (command) {
        case RPT_ENABLE_POLLING:
            ESP_LOGI(TAG, "USB command 0x13: start reports");
            send_ack(RPT_POLLING_ENABLED, !polling_enabled);
            polling_enabled = true;
            break;
        case RPT_DISABLE_POLLING:
            ESP_LOGI(TAG, "USB command 0x14: stop reports");
            send_ack(RPT_POLLING_DISABLED, polling_enabled);
            polling_enabled = false;
            break;
        case RPT_GET_ORIGIN:
            ESP_LOGI(TAG, "USB command 0x12: origin");
            send_origin();
            break;
        case RPT_SET_MOTOR:
            break;
        case RPT_RESET:
            ESP_LOGW(TAG, "USB command 0x15: reset requested");
            esp_restart();
            break;
        default:
            ESP_LOGW(TAG, "Unknown USB command 0x%02x", command);
            break;
    }
}
