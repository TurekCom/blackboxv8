#!/usr/bin/env python
from __future__ import annotations

import argparse
from pathlib import Path

from py65.devices.mpu6502 import MPU


SID_START = 0xD400
SID_END = 0xD418


class TraceMemory(list):
    def __init__(self, init):
        super().__init__(init)
        self.sid_writes: list[tuple[int, int]] = []

    def __setitem__(self, key, value):
        if isinstance(key, int) and SID_START <= key <= SID_END:
            self.sid_writes.append((key, value & 0xFF))
        super().__setitem__(key, value)


def load_bank(rom64: bytes, bank: int) -> bytes:
    if len(rom64) != 65536:
        raise ValueError("This probe expects 64KB ROM image.")
    if bank < 0 or bank > 3:
        raise ValueError("Bank must be in range 0..3.")
    start = bank * 0x4000
    return rom64[start : start + 0x4000]


def main():
    parser = argparse.ArgumentParser(description="Probe zapisów SID z ROM BlackBox V8 przez emulację 6502.")
    parser.add_argument("--rom", default="BLACKBOXV8/FOR 1541U BB8.BIN")
    parser.add_argument("--bank", type=int, default=1, help="Bank 0..3")
    parser.add_argument("--steps", type=int, default=20000)
    parser.add_argument("--entry", choices=["cold", "warm"], default="warm")
    args = parser.parse_args()

    rom_path = Path(args.rom)
    rom = rom_path.read_bytes()
    bank_data = load_bank(rom, args.bank)

    cold = bank_data[0] | (bank_data[1] << 8)
    warm = bank_data[2] | (bank_data[3] << 8)
    pc = cold if args.entry == "cold" else warm

    mem = TraceMemory([0x60] * 65536)  # RTS fallback poza załadowanym bankiem.
    mem[0x8000 : 0xC000] = bank_data
    mem[0x0000] = 0x2F  # minimalna inicjalizacja ZP by uniknąć niektórych pułapek
    mem[0x0001] = 0x37

    mpu = MPU(memory=mem)
    mpu.pc = pc

    for _ in range(args.steps):
        try:
            mpu.step()
        except Exception:
            break

    print(f"ROM: {rom_path}")
    print(f"bank={args.bank} entry={args.entry} pc_start=${pc:04X} steps={args.steps}")
    print(f"SID writes captured: {len(mem.sid_writes)}")
    for idx, (addr, value) in enumerate(mem.sid_writes[:64], start=1):
        print(f"{idx:02d}. ${addr:04X} <= ${value:02X}")


if __name__ == "__main__":
    main()
