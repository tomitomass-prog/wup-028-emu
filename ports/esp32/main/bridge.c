#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "esp_system.h"
#include "tusb.h"
#include "report_ids.h"

static uint8_t state[9] = {0};
static bool connected=false;
static bool polling=false;
static uint8_t response[37];

static uint8_t axis(int value, int max) {
  if (value<0) value=0;
  if (value>max) value=max;
  return (uint8_t)((value*255 + max/2)/max);
}
static uint16_t decode_buttons(const uint8_t d[6]) {
  // Classic Controller reports active-low digital buttons.
  const uint8_t a=(uint8_t)~d[4], b=(uint8_t)~d[5];
  uint16_t out=0;
  // USB GC mapping: A=bit0 B=bit1 X=bit2 Y=bit3,
  // DLEFT=bit4 DRIGHT=bit5 DDOWN=bit6 DUP=bit7,
  // START=bit8 Z=bit9 R=bit10 L=bit11.
  if (b & (1<<4)) out|=1<<0;  // Classic A
  if (b & (1<<6)) out|=1<<1;  // Classic B
  if (b & (1<<3)) out|=1<<2;  // Classic X
  if (b & (1<<5)) out|=1<<3;  // Classic Y
  if (b & (1<<1)) out|=1<<4;  // left
  if (a & (1<<7)) out|=1<<5;  // right
  if (a & (1<<6)) out|=1<<6;  // down
  if (b & (1<<0)) out|=1<<7;  // up
  if (a & (1<<2)) out|=1<<8;  // + -> start
  if (b & (1<<2)) out|=1<<9;  // ZR -> Z
  if (a & (1<<1)) out|=1<<10; // R click
  if (a & (1<<5)) out|=1<<11; // L click
  // ZL, HOME, and minus intentionally unmapped in this first version.
  return out;
}
void bridge_submit_wii(const uint8_t *d, bool present) {
  connected=present;
  memset(state,0,sizeof(state));
  if (!present || !d) return;
  const int lx=d[0]&0x3f;
  const int ly=d[1]&0x3f;
  const int rx=((d[0]>>6)&3)<<3 | ((d[1]>>6)&3)<<1 | ((d[2]>>7)&1);
  const int ry=d[2]&0x1f;
  const int lt=((d[2]>>5)&3)<<3 | ((d[3]>>5)&7);
  const int rt=d[3]&0x1f;
  const uint16_t buttons=decode_buttons(d);
  state[0]=0x10; // wired controller, no rumble advertised
  state[1]=buttons&255;
  state[2]=buttons>>8;
  state[3]=axis(lx,63);
  state[4]=axis(ly,63);
  state[5]=axis(rx,31);
  state[6]=axis(ry,31);
  state[7]=axis(lt,31);
  state[8]=axis(rt,31);
}
static void send_report(uint8_t rid, const uint8_t *payload, uint16_t length) {
  if (!tud_hid_ready()) return;
  // Report is transferred with report ID as the first byte (id=0 argument).
  uint8_t packet[37]={0};
  packet[0]=rid;
  if (payload && length) memcpy(packet+1,payload,length);
  tud_hid_report(0, packet, length+1);
}
void bridge_usb_poll(void) {
  if (!polling || !tud_hid_ready()) return;
  memset(response,0,sizeof(response));
  response[0]=RPT_STATE;
  if (connected) memcpy(response+1,state,9);
  tud_hid_report(0,response,RPT_STATE_LEN);
}
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t type, uint8_t *buffer, uint16_t reqlen) {
  (void)instance;(void)report_id;(void)type;(void)buffer;(void)reqlen;
  return 0;
}
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t type, const uint8_t *buffer, uint16_t size) {
  (void)instance;
  if (type != HID_REPORT_TYPE_OUTPUT || size==0) return;
  // Follow upstream loopj behavior for OUT endpoint, where buffer[0] holds the report ID.
  const uint8_t cmd = report_id ? report_id : buffer[0];
  switch(cmd) {
    case RPT_ENABLE_POLLING: {
      uint8_t ack=(uint8_t)!polling;
      send_report(RPT_POLLING_ENABLED,&ack,1);
      polling=true;
      break;
    }
    case RPT_DISABLE_POLLING: {
      uint8_t ack=(uint8_t)polling;
      send_report(RPT_POLLING_DISABLED,&ack,1);
      polling=false;
      break;
    }
    case RPT_GET_ORIGIN: {
      uint8_t origins[24]={0};
      for(int p=0;p<4;p++) {origins[p*6+0]=128;origins[p*6+1]=128;origins[p*6+2]=128;origins[p*6+3]=128;}
      send_report(RPT_ORIGIN,origins,sizeof(origins));
      break;
    }
    case RPT_RESET: esp_restart(); break;
    case RPT_SET_MOTOR: break; // rumble TODO, intentionally not advertised
    default: break;
  }
}
