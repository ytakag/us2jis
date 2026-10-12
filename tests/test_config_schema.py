import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from config_schema import WIRE_SIZE, load_config


class ConfigSchemaTests(unittest.TestCase):
    def load_text(self, text: str) -> bytes:
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "keymap.yml"
            path.write_text(text, encoding="utf-8")
            return load_config(path)

    def test_default_config_wire_format(self):
        payload = load_config(Path(__file__).resolve().parents[1] / "config/keymap.yml")
        self.assertEqual(len(payload), WIRE_SIZE)
        self.assertEqual(payload[:8], bytes((2, 1, 200, 0, 2, 1, 0x90, 0x91)))
        self.assertEqual(payload[8 + 4 * 0x2E: 12 + 4 * 0x2E], bytes((0x2D, 1, 0x33, 1)))
        self.assertEqual(payload[8 + 4 * 0x34: 12 + 4 * 0x34], bytes((0x24, 1, 0x1F, 1)))

    def test_custom_mapping(self):
        payload = self.load_text('''
schema_version: 1
keyboard:
  alt_tap:
    timeout_ms: 250
    left: {tap: ime_off}
    right: {tap: ime_on}
  ime:
    ime_on: {key: "0x90"}
    ime_off: {key: "0x91"}
keymap:
  enabled: true
  mappings:
    "0x04":
      plain: {key: "0x05", shift: false}
''')
        self.assertEqual(payload[8 + 4 * 4: 12 + 4 * 4], bytes((5, 0, 4, 1)))

    def test_rejects_invalid_action_and_usage(self):
        base = '''
schema_version: 1
keyboard:
  alt_tap:
    timeout_ms: 200
    left: {tap: invalid}
    right: {tap: ime_on}
  ime:
    ime_on: {key: "0x90"}
    ime_off: {key: "0x91"}
keymap: {enabled: true, mappings: {}}
'''
        with self.assertRaises(ValueError):
            self.load_text(base)

        invalid_usage = '''
schema_version: 1
keyboard:
  alt_tap:
    timeout_ms: 200
    left: {tap: ime_off}
    right: {tap: ime_on}
  ime:
    ime_on: {key: "0x90"}
    ime_off: {key: "0x91"}
keymap:
  enabled: true
  mappings:
    "0x04":
      plain: {key: "0xE0", shift: false}
'''
        with self.assertRaises(ValueError):
            self.load_text(invalid_usage)


if __name__ == "__main__":
    unittest.main()
