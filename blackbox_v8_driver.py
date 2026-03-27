# -*- coding: UTF-8 -*-
from __future__ import annotations

import ctypes
import os
import queue
import re
import struct
import threading
from dataclasses import dataclass, field
from pathlib import Path
from typing import Callable
import winreg

import buildVersion
import config
import logHandler
import nvwave
from speech.commands import BreakCommand, CharacterModeCommand, IndexCommand, PitchCommand, RateCommand, VolumeCommand
from synthDriverHandler import SynthDriver as BaseSynthDriver, synthDoneSpeaking, synthIndexReached
try:
    from autoSettingsUtils.driverSetting import BooleanDriverSetting
except Exception:  # pragma: no cover - fallback for tests/non-NVDA env
    class BooleanDriverSetting:
        def __init__(self, id, displayNameWithAccelerator, availableInSettingsRing=False, displayName=None, defaultVal=False, useConfig=True):
            self.id = id
            self.displayNameWithAccelerator = displayNameWithAccelerator
            self.availableInSettingsRing = availableInSettingsRing
            self.displayName = displayName or displayNameWithAccelerator.replace("&", "")
            self.defaultVal = defaultVal
            self.useConfig = useConfig


USER_SETTINGS_KEY = r"Software\BlackBox\SAPI5\Settings"
DEFAULT_MODULATION = 50
EMOJI_ASSET_FALLBACK = Path("android") / "app" / "src" / "main" / "assets" / "emoji" / "emoji_pl_cldr.tsv"
ASCII_EMOTICONS = (
    ("<3", "serce"),
    (":-)", "uśmiechnięta buźka"),
    (":)", "uśmiechnięta buźka"),
    (":-(", "smutna buźka"),
    (":(", "smutna buźka"),
    (";-)", "mrugająca buźka"),
    (";)", "mrugająca buźka"),
    (":-D", "roześmiana buźka"),
    (":D", "roześmiana buźka"),
    (":-P", "buźka z językiem"),
    (":P", "buźka z językiem"),
    (":-*", "buziak"),
    (":*", "buziak"),
)
CHAR_MODE_LABELS = {
    "a": "a",
    "ą": "a z ogonkiem",
    "b": "be",
    "c": "ce",
    "ć": "cie z kreską",
    "d": "de",
    "e": "e",
    "ę": "e z ogonkiem",
    "f": "ef",
    "g": "gie",
    "h": "ha",
    "i": "i",
    "j": "jot",
    "k": "ka",
    "l": "el",
    "ł": "eł przekreślone",
    "m": "em",
    "n": "en",
    "ń": "eń z kreską",
    "o": "o",
    "ó": "u z kreską",
    "p": "pe",
    "q": "ku",
    "r": "er",
    "s": "es",
    "ś": "ś z kreską",
    "t": "te",
    "u": "u",
    "v": "fał",
    "w": "wu",
    "x": "iks",
    "y": "igrek",
    "z": "zet",
    "ź": "zie z kreską",
    "ż": "zet z kropką",
    "0": "zero",
    "1": "jeden",
    "2": "dwa",
    "3": "trzy",
    "4": "cztery",
    "5": "pięć",
    "6": "sześć",
    "7": "siedem",
    "8": "osiem",
    "9": "dziewięć",
    " ": "spacja",
    "\t": "tabulator",
    "\n": "enter",
    "\r": "enter",
    ".": "kropka",
    ",": "przecinek",
    ":": "dwukropek",
    ";": "średnik",
    "!": "wykrzyknik",
    "?": "znak zapytania",
    "@": "małpa",
    "#": "kratka",
    "$": "dolar",
    "%": "procent",
    "^": "daszek",
    "&": "ampersand",
    "*": "gwiazdka",
    "(": "nawias otwierający",
    ")": "nawias zamykający",
    "[": "lewy nawias kwadratowy",
    "]": "prawy nawias kwadratowy",
    "{": "lewa klamra",
    "}": "prawa klamra",
    "<": "mniejsze niż",
    ">": "większe niż",
    "/": "ukośnik",
    "\\": "ukośnik wsteczny",
    "|": "pionowa kreska",
    "-": "minus",
    "_": "podkreślnik",
    "=": "równa się",
    "+": "plus",
    "\"": "cudzysłów",
    "'": "apostrof",
    "`": "akcent odwrotny",
    "~": "tylda",
}
FALLBACK_EMOJI_LABELS = {
    "😀": "uśmiechnięta buźka",
    "😂": "buźka ze łzami radości",
    "🤣": "tarza się ze śmiechu",
    "🙂": "lekko uśmiechnięta buźka",
    "😉": "mrugająca buźka",
    "😍": "buźka z sercami w oczach",
    "😘": "buźka wysyłająca buziaka",
    "😭": "głośno płacząca buźka",
    "🤔": "zamyślona buźka",
    "😎": "buźka w okularach",
    "👍": "kciuk w górę",
    "👎": "kciuk w dół",
    "👏": "klaskanie",
    "🙏": "złożone dłonie",
    "❤": "czerwone serce",
    "💔": "złamane serce",
    "🔥": "ogień",
    "✨": "iskry",
    "🎉": "konfetti",
    "💩": "kupka",
}
_EMOJI_MAP: dict[str, str] | None = None
_EMOJI_INDEX: dict[str, list[tuple[str, str]]] | None = None


def _clamp(value: int, lo: int, hi: int) -> int:
    return lo if value < lo else hi if value > hi else value


def _read_user_percent(name: str, default: int) -> int:
    try:
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, USER_SETTINGS_KEY) as key:
            value, reg_type = winreg.QueryValueEx(key, name)
            if reg_type == winreg.REG_DWORD:
                return _clamp(int(value), 0, 100)
    except OSError:
        pass
    return default


def _char_mode_text(text: str) -> str:
    spoken = []
    for ch in text:
        spoken.append(CHAR_MODE_LABELS.get(ch.lower(), ch))
    return " ".join(part for part in spoken if part)


def _emoji_asset_candidates() -> list[Path]:
    here = Path(__file__).resolve().parent
    return [
        here / "data" / "emoji" / "emoji_pl_cldr.tsv",
        Path.cwd() / EMOJI_ASSET_FALLBACK,
        here.parent / EMOJI_ASSET_FALLBACK,
    ]


def _load_emoji_map() -> tuple[dict[str, str], dict[str, list[tuple[str, str]]]]:
    global _EMOJI_MAP, _EMOJI_INDEX
    if _EMOJI_MAP is not None and _EMOJI_INDEX is not None:
        return _EMOJI_MAP, _EMOJI_INDEX

    emoji_map: dict[str, str] = {}
    for candidate in _emoji_asset_candidates():
        if not candidate.exists():
            continue
        with candidate.open("r", encoding="utf-8") as handle:
            for line in handle:
                line = line.rstrip("\n")
                if "\t" not in line:
                    continue
                cp, tts = line.split("\t", 1)
                cp = cp.strip()
                tts = tts.strip()
                if cp and tts:
                    emoji_map[cp] = tts
        if emoji_map:
            break
    if not emoji_map:
        emoji_map = dict(FALLBACK_EMOJI_LABELS)

    emoji_index: dict[str, list[tuple[str, str]]] = {}
    for key, value in emoji_map.items():
        first = key[0]
        emoji_index.setdefault(first, []).append((key, value))
    for first in emoji_index:
        emoji_index[first].sort(key=lambda item: len(item[0]), reverse=True)

    _EMOJI_MAP = emoji_map
    _EMOJI_INDEX = emoji_index
    return emoji_map, emoji_index


def _match_emoji_key(text: str, start: int, key: str) -> int | None:
    ti = start
    ki = 0
    while ki < len(key):
        while ti < len(text) and text[ti] == "\ufe0f":
            ti += 1
        if ti >= len(text) or text[ti] != key[ki]:
            return None
        ti += 1
        ki += 1
    while ti < len(text) and text[ti] == "\ufe0f":
        ti += 1
    return ti


def _replace_ascii_emoticons(text: str) -> str:
    out = text
    for emoticon, spoken in ASCII_EMOTICONS:
        out = out.replace(emoticon, f" {spoken} ")
    return out


def _normalize_emoji_text(text: str, enabled: bool) -> str:
    if not enabled or not text:
        return text
    _emoji_map, emoji_index = _load_emoji_map()
    normalized = _replace_ascii_emoticons(text)
    out: list[str] = []
    i = 0
    while i < len(normalized):
        candidates = emoji_index.get(normalized[i])
        matched = None
        if candidates:
            for key, spoken in candidates:
                end = _match_emoji_key(normalized, i, key)
                if end is not None:
                    matched = (end, spoken)
                    break
        if matched is not None:
            out.append(" ")
            out.append(matched[1])
            out.append(" ")
            i = matched[0]
            continue
        out.append(normalized[i])
        i += 1
    result = "".join(out)
    result = re.sub(r" {2,}", " ", result)
    result = re.sub(r"\s+([,.;:!?])", r"\1", result)
    return result.strip()


def _driver_dir() -> Path:
    return Path(__file__).resolve().parent


def _native_lib_path() -> Path:
    arch = "x64" if struct.calcsize("P") == 8 else "x86"
    candidate = _driver_dir() / "bin" / arch / "blackbox_nvda_native.dll"
    if candidate.exists():
        return candidate
    return _driver_dir() / f"blackbox_nvda_native_{arch}.dll"


class NativeBlackBox:
    def __init__(self):
        dll_path = _native_lib_path()
        if not dll_path.exists():
            raise FileNotFoundError(f"Native DLL not found: {dll_path}")
        if hasattr(os, "add_dll_directory"):
            os.add_dll_directory(str(dll_path.parent))
        self._dll = ctypes.CDLL(str(dll_path))
        self._dll.bbx_get_api_version.restype = ctypes.c_int
        self._dll.bbx_get_sample_rate.restype = ctypes.c_int
        self._dll.bbx_synthesize_utf16.argtypes = [
            ctypes.c_wchar_p,
            ctypes.c_int,
            ctypes.c_int,
            ctypes.c_int,
            ctypes.c_int,
            ctypes.POINTER(ctypes.c_void_p),
            ctypes.POINTER(ctypes.c_uint32),
        ]
        self._dll.bbx_synthesize_utf16.restype = ctypes.c_int
        self._dll.bbx_free_buffer.argtypes = [ctypes.c_void_p]
        self._dll.bbx_free_buffer.restype = None
        self.sample_rate = int(self._dll.bbx_get_sample_rate())

    @classmethod
    def can_load(cls) -> bool:
        try:
            cls()
            return True
        except Exception:
            logHandler.log.debug("BlackBox native NVDA backend unavailable", exc_info=True)
            return False

    def synthesize(self, text: str, rate: int, pitch: int, volume: int, modulation: int) -> bytes:
        out_ptr = ctypes.c_void_p()
        out_size = ctypes.c_uint32()
        rc = self._dll.bbx_synthesize_utf16(
            text,
            int(rate),
            int(pitch),
            int(volume),
            int(modulation),
            ctypes.byref(out_ptr),
            ctypes.byref(out_size),
        )
        if rc != 0 or not out_ptr.value or out_size.value == 0:
            return b""
        try:
            return ctypes.string_at(out_ptr.value, out_size.value)
        finally:
            self._dll.bbx_free_buffer(out_ptr)


@dataclass
class _Chunk:
    kind: str
    text: str = ""
    duration_ms: int = 0
    rate: int = 50
    pitch: int = 50
    volume: int = 100
    modulation: int = DEFAULT_MODULATION
    on_done: list[Callable[[], None]] = field(default_factory=list)


class SynthDriver(BaseSynthDriver):
    name = "blackbox_v8"
    description = "BlackBox V8 (native)"
    supportedSettings = (
        BaseSynthDriver.RateSetting(),
        BaseSynthDriver.PitchSetting(),
        BaseSynthDriver.VolumeSetting(),
        BooleanDriverSetting(
            "speakEmojis",
            "Odczytuj &emotikony i emoji",
            availableInSettingsRing=False,
            defaultVal=True,
        ),
    )
    supportedCommands = {IndexCommand, CharacterModeCommand, BreakCommand, PitchCommand, RateCommand, VolumeCommand}
    supportedNotifications = {synthIndexReached, synthDoneSpeaking}

    _rate = 50
    _pitch = 50
    _volume = 100
    _speakEmojis = True

    @classmethod
    def check(cls):
        return NativeBlackBox.can_load()

    def __init__(self):
        super().__init__()
        self._queue: queue.Queue[list[_Chunk] | None] = queue.Queue()
        self._stop = threading.Event()
        self._native = NativeBlackBox()
        self._modulation = _read_user_percent("ModulationPercent", DEFAULT_MODULATION)
        device = (
            config.conf["audio"]["outputDevice"]
            if buildVersion.version_year >= 2025
            else config.conf["speech"]["outputDevice"]
        )
        self.player = nvwave.WavePlayer(
            channels=1,
            samplesPerSec=self._native.sample_rate,
            bitsPerSample=16,
            outputDevice=device,
        )
        self._thread = threading.Thread(target=self._worker, daemon=True, name="BlackBoxNvdaWorker")
        self._thread.start()

    def _silence_bytes(self, duration_ms: int) -> bytes:
        samples = max(0, int(self._native.sample_rate * duration_ms / 1000))
        return b"\x00\x00" * samples

    def _notify_indices(self, callbacks: list[Callable[[], None]]) -> Callable[[], None]:
        def run():
            for callback in callbacks:
                try:
                    callback()
                except Exception:
                    logHandler.log.error("BlackBox NVDA index callback failed", exc_info=True)

        return run

    def _flush_text(
        self,
        chunks: list[_Chunk],
        text_parts: list[str],
        character_mode: bool,
        rate_mult: float,
        pitch_mult: float,
        volume_mult: float,
    ) -> None:
        if not text_parts:
            return
        text = "".join(text_parts)
        text_parts.clear()
        if not text.strip():
            return
        if character_mode:
            text = _char_mode_text(text)
        text = _normalize_emoji_text(text, self._speakEmojis)
        chunks.append(
            _Chunk(
                kind="speech",
                text=text,
                rate=_clamp(int(round(self._rate * rate_mult)), 0, 100),
                pitch=_clamp(int(round(self._pitch * pitch_mult)), 0, 100),
                volume=_clamp(int(round(self._volume * volume_mult)), 0, 100),
                modulation=self._modulation,
            ),
        )

    def _parse_sequence(self, seq) -> list[_Chunk]:
        chunks: list[_Chunk] = []
        text_parts: list[str] = []
        rate_mult = 1.0
        pitch_mult = 1.0
        volume_mult = 1.0
        character_mode = False
        pending_immediate: list[Callable[[], None]] = []

        def attach_index(index: int) -> None:
            callback = lambda index=index: synthIndexReached.notify(synth=self, index=index)
            if chunks:
                chunks[-1].on_done.append(callback)
            else:
                pending_immediate.append(callback)

        for item in seq:
            if isinstance(item, str):
                text_parts.append(item)
                continue
            if isinstance(item, IndexCommand):
                self._flush_text(chunks, text_parts, character_mode, rate_mult, pitch_mult, volume_mult)
                attach_index(item.index)
                continue
            if isinstance(item, CharacterModeCommand):
                self._flush_text(chunks, text_parts, character_mode, rate_mult, pitch_mult, volume_mult)
                character_mode = bool(item.state)
                continue
            if isinstance(item, BreakCommand):
                self._flush_text(chunks, text_parts, character_mode, rate_mult, pitch_mult, volume_mult)
                chunks.append(_Chunk(kind="silence", duration_ms=max(0, int(item.time))))
                continue
            if isinstance(item, RateCommand):
                self._flush_text(chunks, text_parts, character_mode, rate_mult, pitch_mult, volume_mult)
                rate_mult = max(0.1, float(item.multiplier))
                continue
            if isinstance(item, PitchCommand):
                self._flush_text(chunks, text_parts, character_mode, rate_mult, pitch_mult, volume_mult)
                pitch_mult = max(0.1, float(item.multiplier))
                continue
            if isinstance(item, VolumeCommand):
                self._flush_text(chunks, text_parts, character_mode, rate_mult, pitch_mult, volume_mult)
                volume_mult = max(0.0, float(item.multiplier))
                continue

        self._flush_text(chunks, text_parts, character_mode, rate_mult, pitch_mult, volume_mult)

        if pending_immediate:
            if chunks:
                chunks[0].on_done = pending_immediate + chunks[0].on_done
            else:
                chunks.append(_Chunk(kind="marker", on_done=pending_immediate))
        return chunks

    def _worker(self):
        while True:
            utterance = self._queue.get()
            try:
                if utterance is None:
                    return
                if self._stop.is_set():
                    continue
                had_audio = False
                for chunk in utterance:
                    if self._stop.is_set():
                        break
                    if chunk.kind == "marker":
                        for callback in chunk.on_done:
                            callback()
                        continue
                    if chunk.kind == "silence":
                        data = self._silence_bytes(chunk.duration_ms)
                    else:
                        data = self._native.synthesize(
                            text=chunk.text,
                            rate=chunk.rate,
                            pitch=chunk.pitch,
                            volume=chunk.volume,
                            modulation=chunk.modulation,
                        )
                    if not data:
                        for callback in chunk.on_done:
                            callback()
                        continue
                    had_audio = True
                    on_done = self._notify_indices(chunk.on_done) if chunk.on_done else None
                    self.player.feed(data, onDone=on_done)
                if had_audio:
                    self.player.idle()
                if not self._stop.is_set():
                    synthDoneSpeaking.notify(synth=self)
            except Exception:
                logHandler.log.error("BlackBox native NVDA worker error", exc_info=True)
            finally:
                self._queue.task_done()

    def speak(self, seq):
        utterance = self._parse_sequence(seq)
        if not utterance:
            return
        self._stop.clear()
        self._queue.put(utterance)

    def cancel(self):
        self._stop.set()
        self.player.stop()
        try:
            while True:
                item = self._queue.get_nowait()
                self._queue.task_done()
                if item is None:
                    self._queue.put(None)
                    break
        except queue.Empty:
            pass

    def pause(self, switch):
        self.player.pause(switch)

    def terminate(self):
        self._stop.set()
        self.player.stop()
        self._queue.put(None)
        self._thread.join()
        self.player.close()

    def _get_rate(self):
        return self._rate

    def _set_rate(self, value):
        self._rate = _clamp(int(value), 0, 100)

    def _get_pitch(self):
        return self._pitch

    def _set_pitch(self, value):
        self._pitch = _clamp(int(value), 0, 100)

    def _get_volume(self):
        return self._volume

    def _set_volume(self, value):
        self._volume = _clamp(int(value), 0, 100)

    def _get_language(self):
        return "pl"

    def _get_speakEmojis(self):
        return self._speakEmojis

    def _set_speakEmojis(self, value):
        self._speakEmojis = bool(value)
