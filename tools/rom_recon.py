#!/usr/bin/env python
from __future__ import annotations

import argparse
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from blackbox_rom import BlackboxROM


def main() -> None:
    parser = argparse.ArgumentParser(description="ROM reconstruction helper for BlackBox V8.")
    parser.add_argument("--rom", default="BLACKBOXV8/FOR 1541U BB8.BIN")
    parser.add_argument("--bank", type=int, default=1)
    parser.add_argument("--entry", choices=["cold", "warm"], default="warm")
    parser.add_argument("--steps", type=int, default=40000)
    parser.add_argument("--pc", type=lambda x: int(x, 0), help="Direct probe from given PC, e.g. 0x927A")
    parser.add_argument("--dc01", type=lambda x: int(x, 0), default=0xEF, help="Emulated CIA1 port B value.")
    parser.add_argument(
        "--xrefs",
        type=lambda x: int(x, 0),
        action="append",
        help="Show JSR/JMP xrefs to address (can be repeated), e.g. --xrefs 0x927A",
    )
    parser.add_argument("--bootstrap-stubs", action="store_true", help="Run cold boot and execute CAxx stubs.")
    parser.add_argument("--no-probe", action="store_true", help="Only static SID-site scan.")
    args = parser.parse_args()

    rom = BlackboxROM(args.rom)
    print(f"ROM: {args.rom} ({len(rom.raw)} bytes)")
    print(f"Banks: {len(rom.banks)}")
    for b in range(len(rom.banks)):
        cold, warm = rom.entry_points(b)
        print(f"  bank {b}: cold=${cold:04X} warm=${warm:04X}")

    sites = rom.find_sid_write_sites()
    print(f"\nStatic SID write sites: {len(sites)}")
    for s in sites[:120]:
        print(
            f"  bank={s.bank} pc=${s.pc:04X} op=${s.opcode:02X} "
            f"{s.addressing} addr=${s.addr:04X}"
        )

    if args.xrefs:
        for target in args.xrefs:
            refs = rom.find_xrefs_to(target)
            print(f"\nXrefs to ${target:04X}: {len(refs)}")
            for bank, pc, kind in refs[:120]:
                print(f"  bank={bank} pc=${pc:04X} {kind}")

    if args.no_probe:
        return

    if args.bootstrap_stubs:
        boot = rom.analyze_bootstrap_stubs(
            bank_idx=args.bank,
            cold_steps=args.steps,
            cia_dc01=args.dc01,
        )
        print("\nBootstrap CA00 bytes:")
        print("  " + " ".join(f"{b:02X}" for b in boot.ca_bytes))
        for stub in boot.stubs:
            print(
                f"  stub ${stub.entry_pc:04X} -> pc=${stub.final_pc:04X} "
                f"bank={stub.active_bank} sid_delta={len(stub.sid_writes_delta)}"
            )
            for i, (addr, value) in enumerate(stub.sid_writes_delta[:32], start=1):
                print(f"    s{i:02d}. ${addr:04X} <= ${value:02X}")

    if args.pc is not None:
        probe = rom.probe_pc(bank_idx=args.bank, pc=args.pc, steps=args.steps, cia_dc01=args.dc01)
    else:
        probe = rom.probe_entry(
            bank_idx=args.bank,
            entry=args.entry,
            steps=args.steps,
            cia_dc01=args.dc01,
        )
    print(
        f"\nProbe: bank={probe.bank} entry={probe.entry} "
        f"pc=${probe.entry_pc:04X} steps={probe.steps}"
    )
    print(f"Bank switches: {len(probe.bank_switches)}")
    for i, (addr, value) in enumerate(probe.bank_switches[:40], start=1):
        print(f"  sw{i:02d}. ${addr:04X} <= ${value:02X}")
    print(f"Captured SID writes: {len(probe.sid_writes)}")
    for i, (addr, value) in enumerate(probe.sid_writes[:120], start=1):
        print(f"  {i:03d}. ${addr:04X} <= ${value:02X}")
    print(f"C000-CFFF writes: {len(probe.c000_writes)}")
    for i, (addr, value) in enumerate(probe.c000_writes[:80], start=1):
        print(f"  c{i:03d}. ${addr:04X} <= ${value:02X}")


if __name__ == "__main__":
    main()
