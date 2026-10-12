#include <string.h>

#include "hardware/clocks.h"
#include "pico/flash.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "pico/util/queue.h"
#include "pio_usb.h"
#include "tusb.h"
#include "app.h"
#include "config.h"
#include "config_store.h"
#include "protocol.h"

typedef struct {
  uint8_t disconnected;
  key_report_t report;
} host_event_t;

static queue_t report_queue;
static queue_t led_queue;
static bool queue_overflow;
static app_config_t active_config;
static uint8_t mounted_keyboard_addr;
static uint8_t mounted_keyboard_instance;
static volatile uint8_t last_led_mask;

static void mark_queue_overflow(void) {
  __atomic_store_n(&queue_overflow, true, __ATOMIC_RELEASE);
}

static void process_report_queue(void) {
  host_event_t event;
  if (__atomic_exchange_n(&queue_overflow, false, __ATOMIC_ACQ_REL)) {
    while (queue_try_remove(&report_queue, &event)) {}
    app_host_disconnected();
  }
  while (queue_try_remove(&report_queue, &event)) {
    if (event.disconnected) app_host_disconnected();
    else app_host_report(&event.report);
  }
}

static void host_core(void) {
  flash_safe_execute_core_init();
  pio_usb_configuration_t config = PIO_USB_DEFAULT_CONFIG;
  config.pin_dp = 12; /* Waveshare RP2350-USB-A: USB-A D+ on GP12, D- on GP13. */
  tuh_configure(1, TUH_CFGID_RPI_PIO_USB_CONFIGURATION, &config);
  tuh_init(1);
  for (;;) {
    tuh_task();
    uint8_t led_mask;
    while (queue_try_remove(&led_queue, &led_mask)) {
      if (mounted_keyboard_addr != 0) {
        (void)tuh_hid_set_report(mounted_keyboard_addr, mounted_keyboard_instance,
                                 0, HID_REPORT_TYPE_OUTPUT, &led_mask, sizeof(led_mask));
      }
    }
  }
}

int main(void) {
  set_sys_clock_khz(120000, true);
  queue_init(&report_queue, sizeof(host_event_t), 16);
  queue_init(&led_queue, sizeof(uint8_t), 4);
  config_defaults(&active_config);
  config_store_init(&active_config);
  app_init(&active_config);
  protocol_init(&active_config);
  multicore_launch_core1(host_core);
  tud_init(0);
  for (;;) {
    tud_task();
    process_report_queue();
    protocol_task();
    app_task();
    tud_cdc_write_flush();
  }
}

void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance,
                      uint8_t const *report_desc, uint16_t desc_len) {
  (void)report_desc;
  (void)desc_len;
  if (tuh_hid_interface_protocol(dev_addr, instance) == HID_ITF_PROTOCOL_KEYBOARD) {
    mounted_keyboard_addr = dev_addr;
    mounted_keyboard_instance = instance;
    uint8_t led_mask = last_led_mask;
    (void)queue_try_add(&led_queue, &led_mask);
    tuh_hid_receive_report(dev_addr, instance);
  }
}

void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance) {
  (void)dev_addr;
  if (dev_addr == mounted_keyboard_addr && instance == mounted_keyboard_instance) {
    mounted_keyboard_addr = 0;
  }
  host_event_t event = {.disconnected = 1};
  if (!queue_try_add(&report_queue, &event)) mark_queue_overflow();
}

void app_led_state(uint8_t led_mask) {
  last_led_mask = led_mask;
  if (!queue_try_add(&led_queue, &led_mask)) {
    uint8_t discarded;
    (void)queue_try_remove(&led_queue, &discarded);
    (void)queue_try_add(&led_queue, &led_mask);
  }
}

void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance,
                                uint8_t const *report, uint16_t len) {
  (void)dev_addr;
  if (tuh_hid_interface_protocol(dev_addr, instance) == HID_ITF_PROTOCOL_KEYBOARD &&
      len >= sizeof(key_report_t)) {
    host_event_t event = {.disconnected = 0};
    memcpy(&event.report, report, sizeof(event.report));
    if (!queue_try_add(&report_queue, &event)) {
      mark_queue_overflow();
    }
  }
  tuh_hid_receive_report(dev_addr, instance);
}


