#!/bin/sh
set -eu
PYTHON="${PYTHON:-.venv/bin/python}"
if [ ! -x "$PYTHON" ]; then PYTHON=python3; fi
mkdir -p build/host-tests
"$PYTHON" scripts/generate_config_header.py --output build/host-tests/generated/default_config.h
cc -std=c11 -Wall -Wextra -Werror -Iinclude -Ibuild/host-tests/generated src/config.c tests/test_config.c -o build/host-tests/test_config
build/host-tests/test_config build/host-tests/default-config.bin
cc -std=c11 -Wall -Wextra -Werror -Iinclude -Ibuild/host-tests/generated src/config.c src/keymap.c tests/test_keymap.c -o build/host-tests/test_keymap
build/host-tests/test_keymap
"$PYTHON" -m unittest discover -s tests -p 'test_*.py'
