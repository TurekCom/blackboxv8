#!/usr/bin/env python
from __future__ import annotations

import argparse
from pathlib import Path


def printable_runs(data: bytes, min_len: int = 6):
    start = None
    for i, b in enumerate(data):
        if 32 <= b <= 126:
            if start is None:
                start = i
        else:
            if start is not None and (i - start) >= min_len:
                yield start, data[start:i].decode("ascii", errors="replace")
            start = None
    if start is not None and (len(data) - start) >= min_len:
        yield start, data[start:].decode("ascii", errors="replace")


def map64(rom: bytes):
    banks = [rom[i * 0x4000 : (i + 1) * 0x4000] for i in range(4)]
    for idx, bank in enumerate(banks):
        cold = bank[0] | (bank[1] << 8)
        warm = bank[2] | (bank[3] << 8)
        sig = bank[4:9]
        print(f"[bank {idx}] cold=${cold:04X} warm=${warm:04X} sig={sig!r}")
        print(f"  likely entry points: ${cold:04X}, ${warm:04X}")


def map32(rom: bytes):
    nmi = rom[0x7FFA] | (rom[0x7FFB] << 8)
    reset = rom[0x7FFC] | (rom[0x7FFD] << 8)
    irq = rom[0x7FFE] | (rom[0x7FFF] << 8)
    print(f"[32KB] vectors NMI=${nmi:04X} RESET=${reset:04X} IRQ=${irq:04X}")


def main():
    parser = argparse.ArgumentParser(description="Prosta mapa ROM BlackBox V8.")
    parser.add_argument(
        "--rom",
        default="BLACKBOXV8/FOR 1541U BB8.BIN",
        help="Ścieżka do ROM 32KB lub 64KB.",
    )
    parser.add_argument("--min-text", type=int, default=18, help="Minimalna długość stringu ASCII.")
    args = parser.parse_args()

    rom_path = Path(args.rom)
    rom = rom_path.read_bytes()
    print(f"ROM: {rom_path} ({len(rom)} bytes)")

    if len(rom) == 65536:
        map64(rom)
    elif len(rom) == 32768:
        map32(rom)
    else:
        print("Nieznany rozmiar ROM; oczekiwano 32768 lub 65536 bajtów.")

    print("\nASCII runs:")
    for off, text in printable_runs(rom, min_len=args.min_text):
        if "BLACK BOX" in text or "_MO" in text or "POMOC" in text or "SYNTEZ" in text:
            print(f"  ${off:04X}: {text}")


if __name__ == "__main__":
    main()
