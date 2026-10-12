#!/usr/bin/env python3
"""Send validated YAML configuration to the us2jis USB CDC interface."""

from __future__ import annotations

import argparse
import sys
import time
import zlib
from pathlib import Path

import serial

from config_schema import WIRE_SIZE, WIRE_VERSION, load_config


def receive_exact(port: serial.Serial, length: int) -> bytes:
    chunks = bytearray()
    while len(chunks) < length:
        chunk = port.read(length - len(chunks))
        if not chunk:
            raise TimeoutError(f"timed out after {len(chunks)} of {length} bytes")
        chunks.extend(chunk)
    return bytes(chunks)


def open_device(name: str, baud: int) -> serial.Serial:
    port = serial.Serial(name, baudrate=baud, timeout=2, write_timeout=5)
    time.sleep(0.25)
    port.reset_input_buffer()
    return port


def apply(port: serial.Serial, payload: bytes) -> None:
    crc = zlib.crc32(payload) & 0xFFFFFFFF
    port.write(f"SET {WIRE_VERSION} {len(payload)} {crc:08x}\n".encode("ascii"))
    port.write(payload)
    port.flush()
    result = port.readline().decode("ascii", errors="replace").strip()
    if result != "OK":
        raise RuntimeError(f"device rejected update: {result or 'no response'}")
    print("Configuration saved and applied.")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True, help="USB CDC port, for example COM5")
    parser.add_argument("--baud", type=int, default=115200, help="CDC line rate (ignored by USB CDC devices)")
    commands = parser.add_subparsers(dest="command", required=True)
    set_cmd = commands.add_parser("apply", help="validate YAML and apply it")
    set_cmd.add_argument("file", type=Path)
    get_cmd = commands.add_parser("get", help="download and back up the active binary configuration")
    get_cmd.add_argument("--output", type=Path, default=Path("device-config.bin"))
    restore_cmd = commands.add_parser("restore", help="restore a binary backup")
    restore_cmd.add_argument("file", type=Path)
    commands.add_parser("reset", help="restore firmware defaults")
    commands.add_parser("info", help="identify the device and protocol version")
    args = parser.parse_args()

    try:
        with open_device(args.port, args.baud) as port:
            port.write(b"INFO\n")
            identity = port.readline().decode("ascii", errors="replace").strip().split()
            if len(identity) != 3 or identity[0] != "US2JIS" or identity[1] != str(WIRE_VERSION) or int(identity[2]) != WIRE_SIZE:
                raise RuntimeError(f"unsupported device or protocol: {' '.join(identity) or 'no response'}")
            if args.command == "info":
                print(f"Device us2jis, protocol v{WIRE_VERSION}, configuration size {WIRE_SIZE} bytes.")
            elif args.command == "apply":
                apply(port, load_config(args.file))
            elif args.command == "restore":
                payload = args.file.read_bytes()
                if len(payload) != WIRE_SIZE or payload[0] != WIRE_VERSION:
                    raise ValueError(f"backup must be a v{WIRE_VERSION} {WIRE_SIZE}-byte configuration")
                apply(port, payload)
            elif args.command == "get":
                port.write(b"GET\n")
                line = port.readline().decode("ascii", errors="replace").strip().split()
                if len(line) != 4 or line[0] != "DATA" or int(line[1]) != WIRE_VERSION:
                    raise RuntimeError(f"invalid device response: {' '.join(line)}")
                length, expected_crc = int(line[2]), int(line[3], 16)
                payload = receive_exact(port, length)
                if length != WIRE_SIZE or zlib.crc32(payload) & 0xFFFFFFFF != expected_crc:
                    raise RuntimeError("device configuration length or checksum is invalid")
                args.output.write_bytes(payload)
                print(f"Saved active configuration to {args.output}.")
            else:
                port.write(b"RESET\n")
                result = port.readline().decode("ascii", errors="replace").strip()
                if result != "OK":
                    raise RuntimeError(f"device reset failed: {result or 'no response'}")
                print("Firmware defaults restored.")
    except (OSError, ValueError, RuntimeError, TimeoutError, serial.SerialException) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
