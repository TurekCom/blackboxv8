#!/usr/bin/env python
from __future__ import annotations

import argparse
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from blackbox_rom import BlackboxROM


def parse_input(raw: str) -> bytes:
    # Accepts plain text with \r and \n escapes.
    return raw.encode("utf-8").decode("unicode_escape").encode("latin-1", errors="ignore")


def main() -> None:
    parser = argparse.ArgumentParser(description="Probe command dispatcher in BlackBox V8 ROM.")
    parser.add_argument("--rom", default="BLACKBOXV8/FOR 1541U BB8.BIN")
    parser.add_argument("--key", type=lambda x: int(x, 0), required=True, help="Command key code (e.g. 0x3F).")
    parser.add_argument("--input", default="", help="Input stream for CHRIN/GETIN, e.g. \"ala ma kota\\r\".")
    parser.add_argument("--kbd", default="", help="Keyboard queue payload for $C6/$0277, e.g. \"ala\\r\".")
    parser.add_argument("--irq-period", type=int, default=0, help="Invoke IRQ vector every N steps (0=off).")
    parser.add_argument("--dispatch-pc", type=lambda x: int(x, 0), default=0xAF70)
    parser.add_argument("--use-keyscan", action="store_true", help="Feed command key through $CB keyscan path.")
    parser.add_argument("--profile", default="", help="RAM profile: cmd_core | cmd_plus_0900")
    parser.add_argument("--cold-steps", type=int, default=140000)
    parser.add_argument("--steps", type=int, default=40000)
    args = parser.parse_args()

    rom = BlackboxROM(args.rom)
    result = rom.probe_command_flow(
        command_key=args.key,
        command_input=parse_input(args.input),
        keyboard_buffer=parse_input(args.kbd),
        irq_period=args.irq_period,
        dispatch_pc=args.dispatch_pc,
        use_keyscan=args.use_keyscan,
        ram_profile=(args.profile or None),
        cold_steps=args.cold_steps,
        command_steps=args.steps,
    )
    print(f"key=${result.command_key:02X} final_pc=${result.final_pc:04X} bank={result.active_bank}")
    print(f"sid_delta={len(result.sid_writes_delta)} bank_switch_delta={len(result.bank_switches_delta)}")
    print(f"ram_exec_hits={len(result.ram_exec_hits)} last={f'${result.ram_exec_hits[-1]:04X}' if result.ram_exec_hits else 'n/a'}")
    for i, (addr, val) in enumerate(result.sid_writes_delta[:64], start=1):
        print(f"  sid{i:02d}. ${addr:04X} <= ${val:02X}")
    if result.chout_text:
        preview = result.chout_text[-400:]
        print("chout_tail:")
        print(preview)


if __name__ == "__main__":
    main()
