#ifndef US2JIS_TUSB_CONFIG_H
#define US2JIS_TUSB_CONFIG_H

#define CFG_TUSB_OS                 OPT_OS_PICO
#define CFG_TUD_ENABLED             1
#define CFG_TUH_ENABLED             1
#define CFG_TUD_CDC                 1
#define CFG_TUD_HID                 1
#define CFG_TUH_HID                 1
#define CFG_TUH_RPI_PIO_USB         1
#define CFG_TUH_ENUMERATION_BUFSIZE 256
#define CFG_TUH_DEVICE_MAX          1
#define CFG_TUH_HID_EPIN_BUFSIZE    64
#define CFG_TUH_HID_EPOUT_BUFSIZE   64
#define CFG_TUD_ENDPOINT0_SIZE      64
#define CFG_TUD_CDC_RX_BUFSIZE      2048
#define CFG_TUD_CDC_TX_BUFSIZE      2048
#define CFG_TUD_CDC_EP_BUFSIZE      64
#define CFG_TUD_HID_EP_BUFSIZE      16
#define CFG_TUSB_MEM_ALIGN          __attribute__((aligned(4)))

#endif
