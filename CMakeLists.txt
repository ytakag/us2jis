cmake_minimum_required(VERSION 3.13)

if(NOT DEFINED PICO_SDK_PATH AND DEFINED ENV{PICO_SDK_PATH})
  set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})
endif()
if(NOT PICO_SDK_PATH)
  set(PICO_SDK_PATH "$ENV{HOME}/.pico-sdk/sdk/2.2.0" CACHE PATH "Pico SDK directory")
endif()

set(PICO_BOARD waveshare_rp2350_usb_a CACHE STRING "Target board")
set(PICO_FLASH_SIZE_BYTES 2088960 CACHE STRING "Reserve the final 8 KiB of the 2 MiB flash for settings" FORCE)
set(PICO_PIO_USB_PATH "${CMAKE_CURRENT_LIST_DIR}/third_party/Pico-PIO-USB" CACHE PATH "Pinned PIO USB library")
include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(us2jis C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(GENERATED_CONFIG_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(DEFAULT_CONFIG_HEADER "${GENERATED_CONFIG_DIR}/default_config.h")
add_custom_command(
  OUTPUT "${DEFAULT_CONFIG_HEADER}"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${GENERATED_CONFIG_DIR}"
  COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_LIST_DIR}/scripts/generate_config_header.py"
          --output "${DEFAULT_CONFIG_HEADER}"
  DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/config/keymap.yml"
    "${CMAKE_CURRENT_LIST_DIR}/scripts/config_schema.py"
    "${CMAKE_CURRENT_LIST_DIR}/scripts/generate_config_header.py"
  VERBATIM
)
set_source_files_properties("${DEFAULT_CONFIG_HEADER}" PROPERTIES GENERATED TRUE)

add_executable(us2jis
  "${DEFAULT_CONFIG_HEADER}"
  src/app.c
  src/config.c
  src/config_store.c
  src/keymap.c
  src/main.c
  src/protocol.c
  src/usb_descriptors.c
)
target_include_directories(us2jis PRIVATE include config "${GENERATED_CONFIG_DIR}")
target_link_libraries(us2jis PRIVATE
  pico_stdlib
  pico_multicore
  pico_flash
  hardware_flash
  tinyusb_device
  tinyusb_host
  tinyusb_pico_pio_usb
)
target_compile_definitions(us2jis PRIVATE
  CFG_TUSB_CONFIG_FILE="tusb_config.h"
  PIO_USB_USE_TINYUSB=1
)
target_compile_options(us2jis PRIVATE -Wall -Wextra -Werror)
pico_enable_stdio_usb(us2jis 0)
pico_enable_stdio_uart(us2jis 0)
pico_add_extra_outputs(us2jis)
