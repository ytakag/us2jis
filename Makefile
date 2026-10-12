.DEFAULT_GOAL := all

BUILD_DIR ?= build
GENERATOR ?= Ninja
PYTHON ?= python3
VENV_DIR ?= .venv
VENV_PYTHON ?= $(VENV_DIR)/bin/python
VENV_REQUIREMENTS_STAMP := $(VENV_DIR)/.requirements-installed
PICO_BOARD ?= waveshare_rp2350_usb_a
PIO_USB_PATH := $(CURDIR)/third_party/Pico-PIO-USB
PICO_TOOLCHAIN_PATH ?= $(lastword $(sort $(wildcard $(HOME)/.pico-sdk/toolchain/*)))
PICOTOOL_CONFIG := $(lastword $(sort $(wildcard $(HOME)/.pico-sdk/picotool/*/picotool/picotoolConfig.cmake)))
PICOTOOL_FETCH_FROM_GIT_PATH ?= $(patsubst %/picotool/picotoolConfig.cmake,%,$(PICOTOOL_CONFIG))
NINJA ?= $(shell command -v ninja 2>/dev/null)
ifeq ($(strip $(NINJA)),)
NINJA := $(lastword $(sort $(wildcard $(HOME)/.pico-sdk/ninja/*/ninja)))
endif

TOOLCHAIN_ARG = $(if $(strip $(PICO_TOOLCHAIN_PATH)),-DPICO_TOOLCHAIN_PATH="$(PICO_TOOLCHAIN_PATH)")
PICOTOOL_ARG = $(if $(strip $(PICOTOOL_FETCH_FROM_GIT_PATH)),-DPICOTOOL_FETCH_FROM_GIT_PATH="$(PICOTOOL_FETCH_FROM_GIT_PATH)")
NINJA_ARG = $(if $(and $(filter Ninja,$(GENERATOR)),$(strip $(NINJA))),-DCMAKE_MAKE_PROGRAM="$(NINJA)")

.PHONY: all configure build firmware deps keymap-uf2 config-uf2 clean help

all: build

configure: deps
	cmake -S . -B "$(BUILD_DIR)" -G "$(GENERATOR)" \
		-DPICO_BOARD="$(PICO_BOARD)" \
		-DPICO_PIO_USB_PATH="$(PIO_USB_PATH)" \
		-DPython3_EXECUTABLE="$(abspath $(VENV_PYTHON))" \
		$(TOOLCHAIN_ARG) $(PICOTOOL_ARG) $(NINJA_ARG)

build: configure
	cmake --build "$(BUILD_DIR)"
	"$(VENV_PYTHON)" scripts/merge_firmware_uf2.py "$(BUILD_DIR)/us2jis.uf2"

firmware: build

deps: $(VENV_REQUIREMENTS_STAMP)

$(VENV_REQUIREMENTS_STAMP): scripts/requirements.txt
	$(PYTHON) -m venv "$(VENV_DIR)"
	"$(VENV_PYTHON)" -m pip install --disable-pip-version-check -r scripts/requirements.txt
	@touch "$@"

clean:
	cmake -E rm -rf "$(BUILD_DIR)"

keymap-uf2 config-uf2: deps
	"$(VENV_PYTHON)" scripts/config_uf2.py --output "$(BUILD_DIR)/keymap.uf2"

help:
	@printf '%s\n' \
	  'make                 Build build/us2jis.uf2 with firmware and keymap settings' \
	  'make build           Build the combined firmware and keymap UF2' \
	  'make firmware        Alias for make build' \
	  'make deps            Create .venv and install Python requirements' \
	  'make keymap-uf2      Create a settings-only UF2 (no firmware rebuild)' \
	  'make configure       Configure the CMake/Ninja build directory' \
	  'make clean           Remove the build directory and generated artifacts'
