#!/usr/bin/env python3
"""Combine the firmware UF2 with the keymap.yml configuration slots."""

from __future__ import annotations

import argparse
import struct
from pathlib import Path

from config_schema import load_config
from config_uf2 import make_uf2

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_CONFIG = ROOT / "config" / "keymap.yml"
BLOCK_SIZE = 512
MAGIC_START = (0x0A324655, 0x9E5D5157)
MAGIC_END = 0x0AB16F30
CONFIG_START = 0x10000000 + 2 * 1024 * 1024 - 2 * 4096
CONFIG_END = 0x10000000 + 2 * 1024 * 1024


def parse_blocks(data: bytes, label: str) -> list[bytearray]:
    if not data or len(data) % BLOCK_SIZE:
        raise ValueError(f"{label} is not a complete UF2 image")
    result = []
    for offset in range(0, len(data), BLOCK_SIZE):
        block = bytearray(data[offset:offset + BLOCK_SIZE])
        fields = struct.unpack_from("<8I", block)
        if fields[:2] != MAGIC_START or struct.unpack_from("<I", block, BLOCK_SIZE - 4)[0] != MAGIC_END:
            raise ValueError(f"{label} contains an invalid UF2 block at offset {offset}")
        if fields[4] > BLOCK_SIZE - 32 - 4:
            raise ValueError(f"{label} contains an invalid payload size at offset {offset}")
        result.append(block)
    return result


def retarget_block_numbers(blocks: list[bytearray]) -> bytes:
    total = len(blocks)
    for number, block in enumerate(blocks):
        struct.pack_into("<I", block, 20, number)
        struct.pack_into("<I", block, 24, total)
    return b"".join(blocks)


def merge(firmware: bytes, config: bytes) -> bytes:
    firmware_blocks = parse_blocks(firmware, "firmware image")
    config_blocks = parse_blocks(config, "configuration image")

    kept_firmware = []
    for block in firmware_blocks:
        fields = struct.unpack_from("<8I", block)
        address, payload_size = fields[3], fields[4]
        if address < CONFIG_END and address + payload_size > CONFIG_START:
            continue
        kept_firmware.append(block)
    if not kept_firmware:
        raise ValueError("firmware image contains no firmware blocks")

    for block in config_blocks:
        fields = struct.unpack_from("<8I", block)
        if not (CONFIG_START <= fields[3] < CONFIG_END):
            raise ValueError("configuration UF2 writes outside the reserved settings area")
    return retarget_block_numbers(kept_firmware + config_blocks)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("firmware", type=Path, help="firmware UF2 produced by CMake")
    parser.add_argument("--config", type=Path, default=DEFAULT_CONFIG)
    parser.add_argument("--output", type=Path, help="combined UF2 path; defaults to the firmware path")
    args = parser.parse_args()

    output = args.output or args.firmware
    combined = merge(args.firmware.read_bytes(), make_uf2(load_config(args.config)))
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_name(output.name + ".tmp")
    temporary.write_bytes(combined)
    temporary.replace(output)
    print(f"Created {output} with firmware and keymap.yml settings.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
