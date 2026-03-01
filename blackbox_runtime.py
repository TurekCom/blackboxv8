# -*- coding: UTF-8 -*-
from __future__ import annotations

import os
from dataclasses import dataclass
from pathlib import Path
from typing import Optional

try:
    from . import blackbox_core
except Exception:
    import blackbox_core  # type: ignore


ROOT = Path(__file__).resolve().parent
DEFAULT_ROM_PATH = ROOT / "BLACKBOXV8" / "FOR 1541U BB8.BIN"


@dataclass
class RuntimeConfig:
    mode: str = "heuristic"
    rom_path: Optional[Path] = None

    @classmethod
    def from_env(cls) -> "RuntimeConfig":
        mode = os.environ.get("BLACKBOX_RUNTIME_MODE", "heuristic").strip().lower()
        rom_path_raw = os.environ.get("BLACKBOX_ROM_PATH", "").strip()
        rom_path = Path(rom_path_raw) if rom_path_raw else None
        return cls(mode=mode, rom_path=rom_path)


class BaseBackend:
    name = "base"

    def synthesize(
        self,
        text: str,
        rate: int,
        pitch: int,
        volume: int,
        symbol_level: object = None,
        character_mode: bool = False,
    ) -> bytes:
        raise NotImplementedError


class HeuristicBackend(BaseBackend):
    name = "heuristic"

    def synthesize(
        self,
        text: str,
        rate: int,
        pitch: int,
        volume: int,
        symbol_level: object = None,
        character_mode: bool = False,
    ) -> bytes:
        return blackbox_core.synthesize_pcm16(
            text=text,
            rate=rate,
            pitch=pitch,
            volume=volume,
            symbol_level=symbol_level,
            character_mode=character_mode,
        )


class RomBackend(BaseBackend):
    name = "rom"

    def __init__(self, rom_path: Optional[Path] = None):
        self.rom_path = rom_path or DEFAULT_ROM_PATH
        if not self.rom_path.exists():
            raise FileNotFoundError(f"ROM file not found: {self.rom_path}")
        self._rom = self.rom_path.read_bytes()
        if len(self._rom) not in (32768, 65536):
            raise ValueError(f"Unexpected ROM size: {len(self._rom)} bytes")
        self._analysis = None
        try:
            from .blackbox_rom import BlackboxROM
        except Exception:
            try:
                from blackbox_rom import BlackboxROM  # type: ignore
            except Exception:
                BlackboxROM = None
        if BlackboxROM:
            try:
                analyzer = BlackboxROM(self.rom_path)
                self._analysis = {
                    "banks": len(analyzer.banks),
                    "sid_sites": len(analyzer.find_sid_write_sites()),
                }
            except Exception:
                self._analysis = None

    def synthesize(
        self,
        text: str,
        rate: int,
        pitch: int,
        volume: int,
        symbol_level: object = None,
        character_mode: bool = False,
    ) -> bytes:
        # Placeholder: do czasu pełnej emulacji 6502+SID fallback do rdzenia heurystycznego.
        return blackbox_core.synthesize_pcm16(
            text=text,
            rate=rate,
            pitch=pitch,
            volume=volume,
            symbol_level=symbol_level,
            character_mode=character_mode,
        )


class BlackboxRuntime:
    def __init__(self, config: Optional[RuntimeConfig] = None):
        self.config = config or RuntimeConfig.from_env()
        self.backend = self._create_backend(self.config)

    def _create_backend(self, config: RuntimeConfig) -> BaseBackend:
        if config.mode == "rom":
            try:
                return RomBackend(config.rom_path)
            except Exception:
                return HeuristicBackend()
        return HeuristicBackend()

    def synthesize(
        self,
        text: str,
        rate: int,
        pitch: int,
        volume: int,
        symbol_level: object = None,
        character_mode: bool = False,
    ) -> bytes:
        return self.backend.synthesize(
            text=text,
            rate=rate,
            pitch=pitch,
            volume=volume,
            symbol_level=symbol_level,
            character_mode=character_mode,
        )
