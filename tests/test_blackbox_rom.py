import unittest
from pathlib import Path

import blackbox_rom


class BlackboxRomTests(unittest.TestCase):
    def test_sid_sites_present(self):
        rom_path = Path("BLACKBOXV8") / "FOR 1541U BB8.BIN"
        rom = blackbox_rom.BlackboxROM(rom_path)
        sites = rom.find_sid_write_sites()
        self.assertTrue(len(sites) > 0)

    def test_probe_entry_cold_emits_sid_writes(self):
        rom_path = Path("BLACKBOXV8") / "FOR 1541U BB8.BIN"
        rom = blackbox_rom.BlackboxROM(rom_path)
        probe = rom.probe_entry(bank_idx=1, entry="cold", steps=120000, cia_dc01=0xEF)
        self.assertTrue(len(probe.sid_writes) > 0)
        self.assertTrue(len(probe.bank_switches) > 0)
        self.assertTrue(len(probe.c000_writes) > 0)

    def test_bootstrap_stub_area_is_generated(self):
        rom_path = Path("BLACKBOXV8") / "FOR 1541U BB8.BIN"
        rom = blackbox_rom.BlackboxROM(rom_path)
        analysis = rom.analyze_bootstrap_stubs(bank_idx=1, cold_steps=120000, cia_dc01=0xEF)
        self.assertEqual(len(analysis.ca_bytes), 32)
        # Oczekujemy kodu zaczynającego się od LDA #$01 / STA $DFFF.
        self.assertEqual(analysis.ca_bytes[0], 0xA9)
        self.assertEqual(analysis.ca_bytes[2], 0x8D)

    def test_probe_command_flow_runs(self):
        rom_path = Path("BLACKBOXV8") / "FOR 1541U BB8.BIN"
        rom = blackbox_rom.BlackboxROM(rom_path)
        result = rom.probe_command_flow(
            command_key=0x3C,
            command_input=b"",
            keyboard_buffer=b"TEST\r",
            command_steps=15000,
        )
        self.assertIsInstance(result.final_pc, int)
        self.assertTrue(len(result.sid_writes_delta) >= 0)

    def test_probe_command_sequence_runs(self):
        rom_path = Path("BLACKBOXV8") / "FOR 1541U BB8.BIN"
        rom = blackbox_rom.BlackboxROM(rom_path)
        seq = rom.probe_command_sequence(command_keys=[0x29, 0x3C], command_steps=12000)
        self.assertEqual(len(seq.steps), 2)

    def test_scan_command_keys_runs(self):
        rom_path = Path("BLACKBOXV8") / "FOR 1541U BB8.BIN"
        rom = blackbox_rom.BlackboxROM(rom_path)
        items = rom.scan_command_keys(keys=[0x29, 0x3C, 0x3F], command_steps=10000)
        self.assertEqual(len(items), 3)

    def test_scan_followup_keys_runs(self):
        rom_path = Path("BLACKBOXV8") / "FOR 1541U BB8.BIN"
        rom = blackbox_rom.BlackboxROM(rom_path)
        items = rom.scan_followup_keys(
            initial_key=0x3F,
            keys=[0x20, 0x41],
            use_keyscan=True,
            initial_steps=12000,
            command_steps=8000,
        )
        self.assertEqual(len(items), 2)


if __name__ == "__main__":
    unittest.main()
