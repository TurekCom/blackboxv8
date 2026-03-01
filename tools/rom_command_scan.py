#!/usr/bin/env python
from __future__ import annotations

import argparse
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from blackbox_rom import BlackboxROM


def parse_keys(raw: str) -> list[int]:
    # Supports "0x20-0x7e" or "0x3c,0x3f,0x29"
    raw = raw.strip()
    if "-" in raw:
        a, b = raw.split("-", 1)
        lo = int(a, 0)
        hi = int(b, 0)
        if hi < lo:
            lo, hi = hi, lo
        return list(range(lo, hi + 1))
    return [int(x, 0) for x in raw.split(",") if x.strip()]


def main() -> None:
    parser = argparse.ArgumentParser(description="Scan command keys at BBV8 dispatcher ($AF86).")
    parser.add_argument("--rom", default="BLACKBOXV8/FOR 1541U BB8.BIN")
    parser.add_argument("--keys", default="0x20-0x7e", help="Range/list, e.g. 0x20-0x7e or 0x29,0x3c")
    parser.add_argument("--cold-steps", type=int, default=140000)
    parser.add_argument("--steps", type=int, default=18000)
    parser.add_argument("--irq-period", type=int, default=0)
    parser.add_argument("--dispatch-pc", type=lambda x: int(x, 0), default=0xAF70)
    parser.add_argument("--use-keyscan", action="store_true")
    parser.add_argument("--prefix-key", type=lambda x: int(x, 0), default=None, help="Execute this key first, then scan follow-up keys")
    parser.add_argument("--prefix-input", default="", help="Input stream used during prefix key run, e.g. \"test\\r\"")
    parser.add_argument("--prefix-kbd", default="", help="Keyboard buffer used during prefix key run, e.g. \"test\\r\"")
    parser.add_argument("--prefix-steps", type=int, default=40000)
    parser.add_argument(
        "--profile",
        default=None,
        help="RAM profile overlay, e.g. cmd_core or cmd_plus_0900",
    )
    parser.add_argument("--show-all", action="store_true")
    args = parser.parse_args()

    keys = parse_keys(args.keys)
    rom = BlackboxROM(args.rom)
    if args.prefix_key is None:
        items = rom.scan_command_keys(
            keys=keys,
            irq_period=args.irq_period,
            dispatch_pc=args.dispatch_pc,
            use_keyscan=args.use_keyscan,
            ram_profile=args.profile,
            cold_steps=args.cold_steps,
            command_steps=args.steps,
        )
    else:
        items = rom.scan_followup_keys(
            initial_key=args.prefix_key & 0xFF,
            keys=keys,
            initial_input=args.prefix_input.encode("utf-8").decode("unicode_escape").encode("latin-1", errors="ignore"),
            initial_keyboard_buffer=args.prefix_kbd.encode("utf-8").decode("unicode_escape").encode("latin-1", errors="ignore"),
            irq_period=args.irq_period,
            dispatch_pc=args.dispatch_pc,
            use_keyscan=args.use_keyscan,
            ram_profile=args.profile,
            cold_steps=args.cold_steps,
            initial_steps=args.prefix_steps,
            command_steps=args.steps,
        )

    for it in items:
        interesting = (
            it.sid_delta_count > 0
            or it.bank_switch_delta_count > 0
            or it.chout_len > 0
            or it.active_bank != 3
        )
        if not args.show_all and not interesting:
            continue
        print(
            f"key=${it.command_key:02X} pc=${it.final_pc:04X} bank={it.active_bank} "
            f"sid={it.sid_delta_count} sidNonZero={it.sid_nonzero_count} "
            f"sw={it.bank_switch_delta_count} chout={it.chout_len} "
            f"ramHits={it.ram_exec_hits_count} ramLast={f'${it.ram_exec_last:04X}' if it.ram_exec_last>=0 else 'n/a'}"
        )
        if it.chout_tail:
            print("  tail:", it.chout_tail.replace("\r", "\\r").replace("\n", "\\n"))


if __name__ == "__main__":
    main()
