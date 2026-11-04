#ifndef TEST_TUSB_H
#define TEST_TUSB_H
#include <stdbool.h>
#include <stdint.h>
typedef enum { HID_REPORT_TYPE_INVALID = 0, HID_REPORT_TYPE_INPUT = 1,
               HID_REPORT_TYPE_OUTPUT = 2, HID_REPORT_TYPE_FEATURE = 3 } hid_report_type_t;
bool tud_hid_ready(void);
bool tud_hid_report(uint8_t report_id, const void *report, uint16_t len);
#endif
