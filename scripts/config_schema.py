"""Validate the editable YAML schema and encode the device wire format."""

from __future__ import annotations

from pathlib import Path
from typing import Any

import yaml

SCHEMA_VERSION = 1
WIRE_VERSION = 2
WIRE_SIZE = 8 + 256 * 4
ACTION = {"none": 0, "ime_on": 1, "ime_off": 2}


def _mapping(value: Any, field: str, expected: set[str]) -> dict:
    if not isinstance(value, dict):
        raise ValueError(f"{field} must be a mapping")
    if set(value) != expected:
        raise ValueError(f"{field} must contain exactly: {', '.join(sorted(expected))}")
    return value


def _usage(value: Any, field: str) -> int:
    if isinstance(value, bool):
        raise ValueError(f"{field} must be a HID usage integer or hex string")
    if isinstance(value, int):
        result = value
    elif isinstance(value, str):
        try:
            result = int(value, 0)
        except ValueError as exc:
            raise ValueError(f"{field} must be an integer such as 0x2F") from exc
    else:
        raise ValueError(f"{field} must be a HID usage integer or hex string")
    if not 0 <= result <= 0xFF or 1 <= result <= 3 or 0xE0 <= result <= 0xE7:
        raise ValueError(f"{field} is not a valid non-modifier keyboard usage")
    return result


def _read_yaml(path: Path) -> dict:
    try:
        document = yaml.safe_load(path.read_text(encoding="utf-8"))
    except yaml.YAMLError as exc:
        raise ValueError(f"invalid YAML: {exc}") from exc
    if not isinstance(document, dict):
        raise ValueError("configuration must be a mapping")
    schema_version = document.get("schema_version")
    if (isinstance(schema_version, bool) or not isinstance(schema_version, int)
            or schema_version != SCHEMA_VERSION):
        raise ValueError(f"schema_version must be {SCHEMA_VERSION}")
    return _mapping(document, "configuration", {"schema_version", "keyboard", "keymap"})


def _keyboard_settings(value: Any) -> tuple[int, int, int, int, int]:
    keyboard = _mapping(value, "keyboard", {"alt_tap", "ime"})
    alt_tap = _mapping(keyboard["alt_tap"], "keyboard.alt_tap", {"timeout_ms", "left", "right"})
    timeout = alt_tap["timeout_ms"]
    if isinstance(timeout, bool) or not isinstance(timeout, int) or not 50 <= timeout <= 2000:
        raise ValueError("keyboard.alt_tap.timeout_ms must be an integer from 50 through 2000")

    actions = []
    for side in ("left", "right"):
        binding = _mapping(alt_tap[side], f"keyboard.alt_tap.{side}", {"tap"})
        action = binding["tap"]
        if not isinstance(action, str) or action not in ACTION:
            raise ValueError(f"keyboard.alt_tap.{side}.tap must be none, ime_on, or ime_off")
        actions.append(ACTION[action])

    ime = _mapping(keyboard["ime"], "keyboard.ime", {"ime_on", "ime_off"})
    usages = []
    for action in ("ime_on", "ime_off"):
        definition = _mapping(ime[action], f"keyboard.ime.{action}", {"key"})
        usage = _usage(definition["key"], f"keyboard.ime.{action}.key")
        if usage < 4:
            raise ValueError(f"keyboard.ime.{action}.key must be a non-modifier keyboard usage")
        usages.append(usage)
    return timeout, actions[0], actions[1], usages[0], usages[1]


def _keymap_entries(value: Any) -> tuple[bool, list[list[int]]]:
    keymap = _mapping(value, "keymap", {"enabled", "mappings"})
    enabled = keymap["enabled"]
    if not isinstance(enabled, bool):
        raise ValueError("keymap.enabled must be true or false")
    mappings = keymap["mappings"]
    if not isinstance(mappings, dict):
        raise ValueError("keymap.mappings must be a mapping")

    entries = [[usage, 0, usage, 1] for usage in range(256)]
    for usage in (1, 2, 3, 0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7):
        entries[usage] = [0, 0, 0, 0]
    for source_value, mapping_value in mappings.items():
        source = _usage(source_value, "keymap source")
        if not isinstance(mapping_value, dict):
            raise ValueError(f"mapping for {source_value} must be a mapping")
        if not set(mapping_value).issubset({"plain", "shifted"}):
            raise ValueError(f"mapping for {source_value} has an unsupported state")
        for state_name, index in (("plain", 0), ("shifted", 2)):
            state = mapping_value.get(state_name)
            if state is None:
                continue
            state = _mapping(state, f"{source_value}.{state_name}", {"key", "shift"})
            target = _usage(state["key"], f"{source_value}.{state_name}.key")
            shift = state["shift"]
            if not isinstance(shift, bool):
                raise ValueError(f"{source_value}.{state_name}.shift must be boolean")
            entries[source][index] = target
            entries[source][index + 1] = int(shift)
    return enabled, entries


def load_config(path: Path) -> bytes:
    document = _read_yaml(path)
    timeout, left_action, right_action, ime_on, ime_off = _keyboard_settings(document["keyboard"])
    enabled, entries = _keymap_entries(document["keymap"])

    payload = bytearray((WIRE_VERSION, int(enabled), timeout & 0xFF, timeout >> 8,
                         left_action, right_action, ime_on, ime_off))
    for entry in entries:
        payload.extend(entry)
    if len(payload) != WIRE_SIZE:
        raise AssertionError("internal wire size mismatch")
    return bytes(payload)
