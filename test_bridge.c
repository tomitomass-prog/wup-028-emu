#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "tusb.h"
#include "report_ids.h"

static uint8_t last[128];
static uint16_t last_len;
static int reports;
static bool ready = true;

bool tud_hid_ready(void) { return ready; }
bool tud_hid_report(uint8_t id, const void *buffer, uint16_t len) {
    assert(id == 0);
    assert(len <= sizeof(last));
    memcpy(last, buffer, len);
    last_len = len;
    reports++;
    return true;
}
void esp_restart(void) { assert(!"Unexpected device restart"); }

void bridge_submit_wii(const uint8_t *d, bool present);
void bridge_usb_poll(void);
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
     hid_report_type_t type, const uint8_t *buffer, uint16_t size);

static void send_command(uint8_t cmd) {
    uint8_t b[1] = {cmd};
    tud_hid_set_report_cb(0, 0, HID_REPORT_TYPE_OUTPUT, b, 1);
}

static void expect(bool condition, const char *label) {
    if (!condition) { fprintf(stderr, "FAIL: %s\n", label); assert(condition); }
    printf("PASS: %s\n", label);
}

int main(void) {
    const uint8_t neutral[6] = {0x20, 0x20, 0x10, 0x00, 0xFF, 0xFF};
    bridge_submit_wii(NULL, false);
    bridge_usb_poll();
    expect(reports == 0, "No state reports before polling enabled");

    send_command(RPT_ENABLE_POLLING);
    expect(reports == 1 && last_len == 2 && last[0] == RPT_POLLING_ENABLED,
           "Enable polling acknowledgment");
    bridge_usb_poll();
    expect(last_len == RPT_STATE_LEN && last[0] == RPT_STATE,
           "Disconnected state has 37-byte report");
    for (int i = 1; i < RPT_STATE_LEN; i++) expect(last[i] == 0, "Disconnected data is zero");

    bridge_submit_wii(neutral, true);
    bridge_usb_poll();
    expect(last[1] == 0x10, "Port 1 marked connected");
    expect(last[2] == 0 && last[3] == 0, "No buttons pressed");
    expect(last[4] >= 126 && last[4] <= 130, "Left X centered");
    expect(last[5] >= 126 && last[5] <= 130, "Left Y centered");
    for (int i = 10; i < 37; i++) expect(last[i] == 0, "Unused ports zero");

    uint8_t a_pressed[6]; memcpy(a_pressed, neutral, 6);
    a_pressed[5] &= (uint8_t)~(1u << 4);
    bridge_submit_wii(a_pressed, true);
    bridge_usb_poll();
    expect((last[2] & 1) != 0, "Classic A maps to GameCube A");

    send_command(RPT_GET_ORIGIN);
    expect(last_len == RPT_ORIGIN_LEN && last[0] == RPT_ORIGIN,
           "Origin response has 25 bytes");

    send_command(RPT_DISABLE_POLLING);
    int before = reports;
    bridge_usb_poll();
    expect(reports == before, "Polling disabled stops state reports");
    puts("All host-side bridge tests passed.");
    return 0;
}
