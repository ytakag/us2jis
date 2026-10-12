#!/usr/bin/env python3
"""Package keymap.yml as an RP2350 UF2 that writes the reserved config slots."""

from __future__ import annotations

import argparse
import struct
import zlib
from pathlib import Path

from config_schema import WIRE_SIZE, load_config

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_SOURCE = ROOT / "config" / "keymap.yml"
DEFAULT_OUTPUT = ROOT / "build" / "keymap.uf2"

FLASH_BASE = 0x10000000
FLASH_BYTES = 2 * 1024 * 1024
SLOT_BYTES = 4096
SLOT_OFFSETS = (FLASH_BYTES - 2 * SLOT_BYTES, FLASH_BYTES - SLOT_BYTES)
SLOT_MAGIC = 0x55324A43

UF2_MAGIC_START0 = 0x0A324655
UF2_MAGIC_START1 = 0x9E5D5157
UF2_MAGIC_END = 0x0AB16F30
UF2_FLAG_FAMILY_ID_PRESENT = 0x00002000
RP2350_ARM_S_FAMILY_ID = 0xE48BFF59
UF2_PAYLOAD_BYTES = 256
UF2_BLOCK_BYTES = 512

# Keep this generation above ordinary device updates so this imported config
# wins over the active slot. Both slots are written with the same generation.
IMPORT_GENERATION = 0x7FFFFFFF


def make_slot(payload: bytes) -> bytes:
    if len(payload) != WIRE_SIZE:
        raise ValueError(f"configuration must be {WIRE_SIZE} bytes")
    header = struct.pack(
        "<IIII", SLOT_MAGIC, IMPORT_GENERATION, len(payload), zlib.crc32(payload) & 0xFFFFFFFF
    )
    record = header + payload
    return record + bytes([0xFF]) * (SLOT_BYTES - len(record))


def make_uf2(payload: bytes) -> bytes:
    slot = make_slot(payload)
    blocks: list[bytes] = []
    block_count = len(SLOT_OFFSETS) * SLOT_BYTES // UF2_PAYLOAD_BYTES
    block_no = 0
    for slot_offset in SLOT_OFFSETS:
        for chunk_offset in range(0, SLOT_BYTES, UF2_PAYLOAD_BYTES):
            chunk = slot[chunk_offset : chunk_offset + UF2_PAYLOAD_BYTES]
            address = FLASH_BASE + slot_offset + chunk_offset
            header = struct.pack(
                "<8I",
                UF2_MAGIC_START0,
                UF2_MAGIC_START1,
                UF2_FLAG_FAMILY_ID_PRESENT,
                address,
                len(chunk),
                block_no,
                block_count,
                RP2350_ARM_S_FAMILY_ID,
            )
            block = header + chunk + bytes(UF2_BLOCK_BYTES - 32 - len(chunk) - 4)
            block += struct.pack("<I", UF2_MAGIC_END)
            blocks.append(block)
            block_no += 1
    return b"".join(blocks)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", nargs="?", type=Path, default=DEFAULT_SOURCE)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    args = parser.parse_args()

    payload = load_config(args.source)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(make_uf2(payload))
    print(f"Created {args.output} ({len(SLOT_OFFSETS) * SLOT_BYTES} config bytes).")
    print("Copy this UF2 to the RP2350 BOOTSEL drive to install the configuration.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
