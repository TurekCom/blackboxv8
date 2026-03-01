#!/usr/bin/env python
from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
ROM_DIR = ROOT / "BLACKBOXV8"


@dataclass
class Chip:
    bank: int
    load_addr: int
    size: int
    chip_type: int
    data: bytes


def parse_crt(path: Path) -> list[Chip]:
    raw = path.read_bytes()
    if raw[:16] != b"C64 CARTRIDGE   ":
        raise ValueError(f"{path} is not a CRT file")

    offset = 0x40
    chips: list[Chip] = []
    while offset + 0x10 <= len(raw):
        if raw[offset : offset + 4] != b"CHIP":
            break
        packet_len = int.from_bytes(raw[offset + 4 : offset + 8], "big")
        chip_type = int.from_bytes(raw[offset + 8 : offset + 10], "big")
        bank = int.from_bytes(raw[offset + 10 : offset + 12], "big")
        load_addr = int.from_bytes(raw[offset + 12 : offset + 14], "big")
        rom_size = int.from_bytes(raw[offset + 14 : offset + 16], "big")
        data_start = offset + 0x10
        data_end = data_start + rom_size
        chips.append(
            Chip(
                bank=bank,
                load_addr=load_addr,
                size=rom_size,
                chip_type=chip_type,
                data=raw[data_start:data_end],
            )
        )
        offset += packet_len
    return chips


def bytes_equal_count(a: bytes, b: bytes) -> int:
    return sum(1 for x, y in zip(a, b) if x == y)


def main() -> None:
    crt = ROM_DIR / "BLACKBOXV8.CRT"
    bin32 = ROM_DIR / "BBV8 ORYGINAL.BIN"
    bin64 = ROM_DIR / "FOR 1541U BB8.BIN"

    chips = parse_crt(crt)
    crt_concat = b"".join(ch.data for ch in chips)
    b32 = bin32.read_bytes()
    b64 = bin64.read_bytes()

    print("=== BLACKBOX V8 ROM INVENTORY ===")
    print(f"CRT chips: {len(chips)}")
    for ch in chips:
        print(
            f"  bank={ch.bank} load=${ch.load_addr:04X} size={ch.size} type={ch.chip_type}"
        )
    print(f"CRT concat size: {len(crt_concat)}")
    print(f"64KB BIN equals CRT concat: {b64 == crt_concat}")

    banks = [b64[i * 0x4000 : (i + 1) * 0x4000] for i in range(4)]
    parts = [b32[:0x4000], b32[0x4000:0x8000]]
    for pi, part in enumerate(parts):
        print(f"32KB part {pi} similarity vs 64KB banks:")
        for bi, bank in enumerate(banks):
            eq = bytes_equal_count(part, bank)
            print(f"  bank {bi}: {eq}/16384")


if __name__ == "__main__":
    main()
