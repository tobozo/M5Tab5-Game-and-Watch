/*\
 *
 * M5Stack Tab5 Game&Watch Emulator
 *
 * A speculation brought to you by tobozo, copyleft (c+) 2025
 *
\*/
#pragma once

/*
 * NOTE: Compiler may complain about missing <rom/usb/usb_common.h>
 *       Patching EspUsbHost.h is easy, see https://github.com/tanakamasayuki/EspUsbHost/issues/18
 *
 * // #include <rom/usb/usb_common.h>
 * #if __has_include(<rom/usb/usb_common.h>)
 *   #include <rom/usb/usb_common.h>
 * #else
 *   #define USB_DEVICE_DESC              0x01
 *   #define USB_CONFIGURATION_DESC       0x02
 *   #define USB_STRING_DESC              0x03
 *   #define USB_INTERFACE_DESC           0x04
 *   #define USB_ENDPOINT_DESC            0x05
 *   #define USB_DEVICE_QUAL_DESC         0x06
 *   #define USB_INTERFACE_ASSOC_DESC     0x0B
 *   #define USB_DEVICE_CAPABILITY_DESC   0x10
 *   #define USB_HID_DESC                 0x21
 *   #define USB_HID_REPORT_DESC          0x22
 *   #define USB_DFU_FUNCTIONAL_DESC      0x21
 *   #define USB_ASSOCIATION_DESC         0x0B
 *   #define USB_BINARY_OBJECT_STORE_DESC 0x0F
 * #endif
 *
 * */
#include <EspUsbHost.h>


#if 0

void log_printbuf(const uint8_t *b, size_t len, size_t total_len) {
  // for (size_t i = 0; i < len; i++) {
  //   log_printf("%s0x%02x,", i ? " " : "", b[i]);
  // }
  if (total_len > 16) {
    for (size_t i = len; i < 16; i++) {
      log_printf("      ");
    }
    log_printf("  %d bytes -> ", total_len);
  } else {
    log_printf(" // ");
  }
  for (size_t i = 0; i < len; i++) {
    //log_printf("%c", ((b[i] >= 0x20) && (b[i] < 0x80)) ? b[i] : '¿');
    log_printf("%c", isprint(b[i]) ? b[i] : '.');
  }
  log_printf("\n");
}

#endif

extern void drawBuffer(uint8_t* data, uint32_t len);

typedef struct _SNES_gamepad_t
{
  union {
    struct {
      uint8_t x:1, y:1, a:1, b:1;
      uint8_t up:1, down:1, left:1, right:1;
      uint8_t start:1, select:1;
      uint8_t l1:1, r1:1;
      uint8_t reserved: 4;
    };
    uint16_t val{0};
  } btns;

  bool operator==(_SNES_gamepad_t& l)
  {
    return l.btns.val == btns.val;
  }
} SNES_gamepad_report_t;



SNES_gamepad_report_t parse_SNES_report(const uint8_t* data, int length)
{
  SNES_gamepad_report_t report;
  if(length<9)
    return report;
  report.btns.x = bitRead(data[5], 4);
  report.btns.y = bitRead(data[5], 5);
  report.btns.a = bitRead(data[5], 6);
  report.btns.b = bitRead(data[5], 7);

  report.btns.left  = data[3] == 0;
  report.btns.right = data[3] == 0xff;
  report.btns.up    = data[4] == 0;
  report.btns.down  = data[4] == 0xff;

  report.btns.select = bitRead(data[6], 5);
  report.btns.start  = bitRead(data[6], 4);

  report.btns.l1 = bitRead(data[6], 0);
  report.btns.r1 = bitRead(data[6], 1);

  return report;
}



#define KEYCODE_POOL_SIZE 8
QueueHandle_t usbHIDQueue;

static int keycode_usb = 0;


class M5UsbHost : public EspUsbHost
{

  void onKeyboard(hid_keyboard_report_t report, hid_keyboard_report_t last_report)
  {
    if( last_report.modifier != report.modifier ) {
      ESP_LOGD("EspUsbHost", "Modifier changed");
    }
    for(int i=0;i<6;i++) {
      if( last_report.keycode[i] != report.keycode[i] ) {
        ESP_LOGD("EspUsbHost", "Keycode #%d changed [0x%02x]->[0x%02x]", i, last_report.keycode[i], report.keycode[i]);
        if( report.keycode[i] == 0 ) { // key released
          xQueueSend(usbHIDQueue, (void*)&report.keycode[i], 0);
          break;
        }
      }
    }
  }


  void onKeyboardKey(uint8_t ascii, uint8_t keycode, uint8_t modifier)
  {
    if (' ' <= ascii && ascii <= '~') {
      Serial.printf("%c", ascii);
    } else if (ascii == '\r') {
      Serial.println();
    } else {
      switch(keycode) {
        case 0x50: /* Arrow left   */
        case 0x51: /* Arrow down   */
        case 0x4f: /* Arrow right  */
        case 0x52: /* Arrow up     */
        case 0x2b: /* TAB: Jump    */
        case 0x29: /* ESC: GAW     */
        case 0x3a: /* F1: Game A   */
        case 0x3b: /* F2: Game B   */
        case 0x3c: /* F3: Time     */
        case 0x3d: /* F4: Alarm    */
        case 0x3e: /* F5: ACL      */
        case 0x3f: /* F6: reserved */
        case 0x40: /* F7: reserved */
        case 0x41: /* F8: reserved */
        case 0x42: /* F9: reserved */
          xQueueSend(usbHIDQueue, (void*)&keycode, 0);
        break;

        default:
          ESP_LOGD("USB", "Unhandled keycode: [0x%02x:%x]\n", keycode, modifier);
        break;
      }
    }
  }


  void onReceive(const usb_transfer_t *transfer)
  {
    EspUsbHost *usbHost = (EspUsbHost *)transfer->context;
    endpoint_data_t *endpoint_data = &usbHost->endpoint_data_list[(transfer->bEndpointAddress & USB_B_ENDPOINT_ADDRESS_EP_NUM_MASK)];
    if (endpoint_data->bInterfaceClass != USB_CLASS_HID) // only catch reports from HID interfaces
      return;
    if (endpoint_data->bInterfaceProtocol != HID_ITF_PROTOCOL_NONE) // only catch unknown HID reports
      return;

    const usb_device_desc_t *dev_desc;
    esp_err_t err = usb_host_get_device_descriptor(usbHost->deviceHandle, &dev_desc);
    if( err != ESP_OK )
      return;

    static SNES_gamepad_report_t last_report;

    uint8_t ret = 0;

    switch(dev_desc->idProduct) {
      case 0xe501: // SNES Gamepad
        const int snes_report_size = 9; // endpoint descriptor length
        if(transfer->data_buffer_size<snes_report_size)
          break;
        // // Debug SNES transfer
        // drawBuffer((uint8_t*)&transfer->data_buffer, snes_report_size);
        auto report = parse_SNES_report(transfer->data_buffer, snes_report_size);
        if( last_report == report ) {
          // ignore repeated report
        } else { // send keyboard ascii codes
          if     ( report.btns.left )
            ret = 0x50; /* left */
          else if( report.btns.down )
            ret = 0x51; /* down */
          else if( report.btns.right )
            ret = 0x4f; /* right*/
          else if( report.btns.up )
            ret = 0x52; /* up   */
          else if( report.btns.y )
            ret = 0x2b; /* TAB: Jump  */
          else if( report.btns.select )
            ret = 0x29; /* ESC: GAW   */
          else if( report.btns.a )
            ret = 0x3a; /* F1: Game A */
          else if( report.btns.b )
            ret = 0x3b; /* F2: Game B */
          else if( report.btns.start )
            ret = 0x3c; /* F3: Time   */
          else if( report.btns.x )
            ret = 0x3d; /* F4: Alarm  */
          else if( report.btns.l1 && report.btns.r1)
            ret = 0x3e; /* F5: ACL    */

          xQueueSend(usbHIDQueue, (void*)&ret, 0);
          // // Debug SNES buffer
          // drawBuffer((uint8_t*)&report, sizeof(SNES_gamepad_report_t));
        }
        last_report = report;
      break;
    }
  }

  // virtual void onGone(const usb_host_client_event_msg_t *eventMsg){};
  //
  // virtual uint8_t getKeycodeToAscii(uint8_t keycode, uint8_t shift);
  //
  // virtual void onMouse(hid_mouse_report_t report, uint8_t last_buttons);
  // virtual void onMouseButtons(hid_mouse_report_t report, uint8_t last_buttons);
  // virtual void onMouseMove(hid_mouse_report_t report);

};

M5UsbHost usbHost;

void usbHostTask(void*param)
{
  usbHIDQueue = xQueueCreate(KEYCODE_POOL_SIZE, sizeof(uint8_t*));

  usbHost.begin();
  usbHost.setHIDLocal( HID_LOCAL_UK );

  while(1) {
    usbHost.task();
    vTaskDelay(1);
  }
}
