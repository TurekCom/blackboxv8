#!/usr/bin/env python
from __future__ import annotations

import argparse
import sys
import wave
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

import blackbox_core


DEFAULT_PHRASES = [
    "Dzień dobry. To jest test syntezatora BlackBox V8.",
    "Zażółć gęślą jaźń, trzysta czterdzieści pięć.",
    "NVDA czyta tekst po polsku: pytajnik, wykrzyknik, przecinek.",
]


def write_wav(path: Path, pcm16: bytes, sample_rate: int) -> None:
    with wave.open(str(path), "wb") as wav:
        wav.setnchannels(1)
        wav.setsampwidth(2)
        wav.setframerate(sample_rate)
        wav.writeframes(pcm16)


def main() -> None:
    parser = argparse.ArgumentParser(description="Render testowych próbek mowy BlackBox V8 do WAV.")
    parser.add_argument("--out-dir", default="test_outputs", help="Katalog wyjściowy na pliki WAV.")
    parser.add_argument("--rate", type=int, default=50)
    parser.add_argument("--pitch", type=int, default=50)
    parser.add_argument("--volume", type=int, default=100)
    parser.add_argument("--text", action="append", help="Tekst do syntezy. Można podać wielokrotnie.")
    args = parser.parse_args()

    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    phrases = args.text if args.text else DEFAULT_PHRASES
    for idx, phrase in enumerate(phrases, start=1):
        pcm = blackbox_core.synthesize_pcm16(
            text=phrase,
            rate=args.rate,
            pitch=args.pitch,
            volume=args.volume,
        )
        out_path = out_dir / f"sample_{idx:02d}.wav"
        write_wav(out_path, pcm, blackbox_core.SAMPLE_RATE)
        duration = len(pcm) / 2 / blackbox_core.SAMPLE_RATE
        print(f"{out_path} | {duration:.2f}s | {len(pcm)} bytes PCM")


if __name__ == "__main__":
    main()
