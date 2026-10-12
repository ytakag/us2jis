#!/usr/bin/env python3
"""Generate the firmware's initial configuration from the editable YAML."""

from __future__ import annotations

import argparse
from pathlib import Path

from config_schema import WIRE_SIZE, load_config

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_SOURCE = ROOT / "config" / "keymap.yml"


def render_header(payload: bytes) -> str:
    if len(payload) != WIRE_SIZE:
        raise ValueError(f"configuration must be {WIRE_SIZE} bytes")
    rows = [payload[index:index + 12] for index in range(0, len(payload), 12)]
    values = ",\n  ".join(", ".join(f"0x{value:02x}" for value in row) for row in rows)
    return (
        "#ifndef US2JIS_DEFAULT_CONFIG_H\n"
        "#define US2JIS_DEFAULT_CONFIG_H\n\n"
        "#include <stdint.h>\n"
        "#include \"config.h\"\n\n"
        "static const uint8_t config_default_wire[CONFIG_WIRE_SIZE] = {\n  "
        f"{values}\n"
        "};\n\n"
        "#endif\n"
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=DEFAULT_SOURCE)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    payload = load_config(args.source)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(render_header(payload), encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
