#!/usr/bin/env python3
from __future__ import annotations

import argparse
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parent
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

import blackbox_core  # type: ignore


def clamp(value: int, low: int, high: int) -> int:
    return max(low, min(high, int(value)))


def main() -> int:
    parser = argparse.ArgumentParser(description="BlackBox SAPI5 bridge")
    parser.add_argument("--rate", type=int, default=50)
    parser.add_argument("--pitch", type=int, default=50)
    parser.add_argument("--volume", type=int, default=100)
    args = parser.parse_args()

    text = sys.stdin.buffer.read().decode("utf-8", errors="replace")
    if not text.strip():
        return 0

    pcm = blackbox_core.synthesize_pcm16(
        text=text,
        rate=clamp(args.rate, 0, 100),
        pitch=clamp(args.pitch, 0, 100),
        volume=clamp(args.volume, 0, 100),
    )
    if pcm:
        sys.stdout.buffer.write(pcm)
        sys.stdout.buffer.flush()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
