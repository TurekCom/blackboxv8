#!/usr/bin/env python
from __future__ import annotations

import argparse
from collections import Counter
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from blackbox_rom import BlackboxROM


def to_ranges(addrs: list[int]) -> list[tuple[int, int]]:
    if not addrs:
        return []
    uniq = sorted(set(addrs))
    ranges: list[tuple[int, int]] = []
    start = prev = uniq[0]
    for a in uniq[1:]:
        if a == prev + 1:
            prev = a
            continue
        ranges.append((start, prev))
        start = prev = a
    ranges.append((start, prev))
    return ranges


def main() -> None:
    parser = argparse.ArgumentParser(description="Report RAM execution ranges for a BBV8 command flow.")
    parser.add_argument("--rom", default="BLACKBOXV8/FOR 1541U BB8.BIN")
    parser.add_argument("--key", type=lambda x: int(x, 0), required=True)
    parser.add_argument("--input", default="")
    parser.add_argument("--kbd", default="")
    parser.add_argument("--irq-period", type=int, default=0)
    parser.add_argument("--steps", type=int, default=120000)
    args = parser.parse_args()

    rom = BlackboxROM(args.rom)
    res = rom.probe_command_flow(
        command_key=args.key,
        command_input=args.input.encode("latin-1", errors="ignore"),
        keyboard_buffer=args.kbd.encode("latin-1", errors="ignore"),
        irq_period=args.irq_period,
        command_steps=args.steps,
    )
    ram = [a for a in res.ram_exec_hits if a < 0x0800]
    rr = to_ranges(ram)
    print(f"key=${res.command_key:02X} final_pc=${res.final_pc:04X} bank={res.active_bank}")
    print(f"ram hits: {len(ram)} unique={len(set(ram))}")
    for s, e in rr[:80]:
        if s == e:
            print(f"  ${s:04X}")
        else:
            print(f"  ${s:04X}-${e:04X}")

    cnt = Counter(ram)
    print("top hotspots:")
    for addr, c in cnt.most_common(20):
        print(f"  ${addr:04X}: {c}")


if __name__ == "__main__":
    main()
