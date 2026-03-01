#!/usr/bin/env python
from __future__ import annotations

import argparse
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from blackbox_rom import BlackboxROM


def parse_key(raw: str) -> int:
    return int(raw, 0) & 0xFF


def parse_input(raw: str) -> bytes:
    return raw.encode("utf-8").decode("unicode_escape").encode("latin-1", errors="ignore")


def main() -> None:
    parser = argparse.ArgumentParser(description="Run sequence of command keys in BlackBox V8 ROM.")
    parser.add_argument("--rom", default="BLACKBOXV8/FOR 1541U BB8.BIN")
    parser.add_argument("--key", action="append", required=True, help="Key code, e.g. 0x29")
    parser.add_argument(
        "--input",
        action="append",
        default=[],
        help="Input for corresponding key (optional), e.g. \"test\\r\"",
    )
    parser.add_argument(
        "--kbd",
        action="append",
        default=[],
        help="Keyboard queue for corresponding key (optional), e.g. \"test\\r\"",
    )
    parser.add_argument("--cold-steps", type=int, default=140000)
    parser.add_argument("--steps", type=int, default=35000)
    parser.add_argument("--irq-period", type=int, default=0)
    parser.add_argument("--dispatch-pc", type=lambda x: int(x, 0), default=0xAF70)
    parser.add_argument("--use-keyscan", action="store_true")
    parser.add_argument(
        "--profile",
        default=None,
        help="RAM profile overlay, e.g. cmd_core or cmd_plus_0900",
    )
    args = parser.parse_args()

    keys = [parse_key(k) for k in args.key]
    inputs = [parse_input(x) for x in args.input]
    if len(inputs) < len(keys):
        inputs += [b""] * (len(keys) - len(inputs))
    kbds = [parse_input(x) for x in args.kbd]
    if len(kbds) < len(keys):
        kbds += [b""] * (len(keys) - len(kbds))

    rom = BlackboxROM(args.rom)
    res = rom.probe_command_sequence(
        command_keys=keys,
        command_inputs=inputs,
        keyboard_buffers=kbds,
        irq_period=args.irq_period,
        dispatch_pc=args.dispatch_pc,
        use_keyscan=args.use_keyscan,
        ram_profile=args.profile,
        cold_steps=args.cold_steps,
        command_steps=args.steps,
    )
    for idx, step in enumerate(res.steps, start=1):
        print(
            f"[{idx}] key=${step.command_key:02X} pc=${step.final_pc:04X} bank={step.active_bank} "
            f"sid={len(step.sid_writes_delta)} switches={len(step.bank_switches_delta)} "
            f"ramHits={len(step.ram_exec_hits)}"
        )
        if step.chout_text:
            tail = step.chout_text[-200:]
            print("  chout_tail:")
            print("  " + tail.replace("\r", "\\r").replace("\n", "\\n"))

    print(f"final pc=${res.final_pc:04X} bank={res.active_bank}")


if __name__ == "__main__":
    main()
