#include "tusb.h"
#include "app.h"

#define USB_VID 0xCafe
#define USB_PID 0x4025
#define USB_BCD 0x0200

enum { ITF_CDC = 0, ITF_CDC_DATA, ITF_HID, ITF_COUNT };
#define EP_CDC_NOTIF 0x81
#define EP_CDC_OUT   0x02
#define EP_CDC_IN    0x82
#define EP_HID_IN    0x83
#define CONFIG_LEN (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN + TUD_HID_DESC_LEN)

tusb_desc_device_t const desc_device = {
  .bLength = sizeof(tusb_desc_device_t), .bDescriptorType = TUSB_DESC_DEVICE,
  .bcdUSB = USB_BCD, .bDeviceClass = TUSB_CLASS_MISC,
  .bDeviceSubClass = MISC_SUBCLASS_COMMON, .bDeviceProtocol = MISC_PROTOCOL_IAD,
  .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE, .idVendor = USB_VID,
  .idProduct = USB_PID, .bcdDevice = 0x0100, .iManufacturer = 1,
  .iProduct = 2, .iSerialNumber = 3, .bNumConfigurations = 1
};

uint8_t const *tud_descriptor_device_cb(void) {
  return (uint8_t const *)&desc_device;
}

uint8_t const desc_hid_report[] = { TUD_HID_REPORT_DESC_KEYBOARD() };

uint8_t const desc_configuration[] = {
  TUD_CONFIG_DESCRIPTOR(1, ITF_COUNT, 0, CONFIG_LEN, 0x00, 100),
  TUD_CDC_DESCRIPTOR(ITF_CDC, 4, EP_CDC_NOTIF, 8, EP_CDC_OUT, EP_CDC_IN, 64),
  TUD_HID_DESCRIPTOR(ITF_HID, 5, HID_ITF_PROTOCOL_KEYBOARD,
                     sizeof(desc_hid_report), EP_HID_IN, 16, 10)
};

uint8_t const *tud_descriptor_configuration_cb(uint8_t index) {
  (void)index;
  return desc_configuration;
}

uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance) {
  (void)instance;
  return desc_hid_report;
}

static char const *const strings[] = {
  (const char[]){0x09, 0x04}, "us2jis", "HHKB US to JIS bridge",
  "US2JIS000001", "Configuration CDC", "Keyboard HID"
};
static uint16_t string_desc[32];

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
  (void)langid;
  uint8_t count;
  if (index == 0) {
    string_desc[1] = 0x0409;
    count = 1;
  } else {
    if (index >= sizeof(strings) / sizeof(strings[0])) return NULL;
    char const *s = strings[index];
    count = 0;
    while (s[count] && count < 31) {
      string_desc[1 + count] = (uint8_t)s[count];
      ++count;
    }
  }
  string_desc[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * count + 2));
  return string_desc;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                           hid_report_type_t report_type, uint8_t const *buffer,
                           uint16_t bufsize) {
  (void)instance;
  (void)report_id;
  if (instance == 0 && report_type == HID_REPORT_TYPE_OUTPUT && bufsize >= 1) {
    app_led_state(buffer[0]);
  }
}

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
                              hid_report_type_t report_type, uint8_t *buffer,
                              uint16_t reqlen) {
  (void)instance;
  (void)report_id;
  (void)report_type;
  (void)buffer;
  (void)reqlen;
  return 0;
}
