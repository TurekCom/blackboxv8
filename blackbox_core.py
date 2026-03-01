# -*- coding: UTF-8 -*-
from __future__ import annotations

import math
import random
import re
import struct
from typing import List, Optional


SAMPLE_RATE = 22050

# NVDA punctuation/symbol levels: none, some, most, all.
SYMBOL_LEVEL_NONE = 0
SYMBOL_LEVEL_SOME = 1
SYMBOL_LEVEL_MOST = 2
SYMBOL_LEVEL_ALL = 3

LETTER_NAMES = {
    "a": "a",
    "ą": "oł",
    "b": "be",
    "c": "ce",
    "ć": "cie",
    "d": "de",
    "e": "e",
    "ę": "eł",
    "f": "ef",
    "g": "gie",
    "h": "ha",
    "i": "i",
    "j": "jot",
    "k": "ka",
    "l": "el",
    "ł": "eu",
    "m": "em",
    "n": "en",
    "ń": "eń",
    "o": "o",
    "ó": "u",
    "p": "pe",
    "q": "ku",
    "r": "er",
    "s": "es",
    "ś": "eś",
    "t": "te",
    "u": "u",
    "v": "fau",
    "w": "wu",
    "x": "iks",
    "y": "igrek",
    "z": "zet",
    "ź": "ziet",
    "ż": "żet",
}

# symbol -> (spoken form, minimum symbol level)
SYMBOLS = {
    ".": ("kropka", SYMBOL_LEVEL_MOST),
    ",": ("przecinek", SYMBOL_LEVEL_MOST),
    "?": ("pytajnik", SYMBOL_LEVEL_MOST),
    "!": ("wykrzyknik", SYMBOL_LEVEL_MOST),
    ":": ("dwukropek", SYMBOL_LEVEL_MOST),
    ";": ("średnik", SYMBOL_LEVEL_MOST),
    "-": ("myślnik", SYMBOL_LEVEL_SOME),
    "(": ("nawias otwierający", SYMBOL_LEVEL_MOST),
    ")": ("nawias zamykający", SYMBOL_LEVEL_MOST),
    '"': ("cudzysłów", SYMBOL_LEVEL_MOST),
    "'": ("apostrof", SYMBOL_LEVEL_ALL),
    "@": ("małpa", SYMBOL_LEVEL_SOME),
    "#": ("hasz", SYMBOL_LEVEL_SOME),
    "$": ("dolar", SYMBOL_LEVEL_SOME),
    "%": ("procent", SYMBOL_LEVEL_SOME),
    "*": ("gwiazdka", SYMBOL_LEVEL_SOME),
    "+": ("plus", SYMBOL_LEVEL_SOME),
    "=": ("równa się", SYMBOL_LEVEL_SOME),
    "/": ("ukośnik", SYMBOL_LEVEL_SOME),
    "\\": ("ukośnik wsteczny", SYMBOL_LEVEL_ALL),
    "_": ("podkreślenie", SYMBOL_LEVEL_SOME),
    "<": ("mniejsze niż", SYMBOL_LEVEL_SOME),
    ">": ("większe niż", SYMBOL_LEVEL_SOME),
    "^": ("daszek", SYMBOL_LEVEL_ALL),
    "&": ("ampersand", SYMBOL_LEVEL_SOME),
    "~": ("tylda", SYMBOL_LEVEL_ALL),
    "|": ("kreska pionowa", SYMBOL_LEVEL_ALL),
    "{": ("klamra otwierająca", SYMBOL_LEVEL_ALL),
    "}": ("klamra zamykająca", SYMBOL_LEVEL_ALL),
    "[": ("nawias kwadratowy otwierający", SYMBOL_LEVEL_MOST),
    "]": ("nawias kwadratowy zamykający", SYMBOL_LEVEL_MOST),
    "`": ("backtick", SYMBOL_LEVEL_ALL),
}

PHONEMES = {
    "a": (750, 1200, 2400),
    "e": (500, 1700, 2500),
    "i": (280, 2300, 2900),
    "o": (550, 1050, 2400),
    "u": (350, 800, 2200),
    "y": (350, 1450, 2350),
    "m": (250, 1000, 2200),
    "n": (250, 1500, 2500),
    "ń": (250, 2100, 2800),
    "l": (400, 1500, 2500),
    "ł": (300, 900, 2200),
    "j": (280, 2250, 2890),
    "w": (320, 1050, 2300),
    "z": (420, 1650, 2500),
    "ż": (380, 1500, 2350),
    "ź": (330, 2000, 2750),
    "ę": (500, 1600, 2500),
    "ą": (550, 1000, 2400),
}

_UNITS = [
    "zero",
    "jeden",
    "dwa",
    "trzy",
    "cztery",
    "pięć",
    "sześć",
    "siedem",
    "osiem",
    "dziewięć",
]
_TEENS = [
    "dziesięć",
    "jedenaście",
    "dwanaście",
    "trzynaście",
    "czternaście",
    "piętnaście",
    "szesnaście",
    "siedemnaście",
    "osiemnaście",
    "dziewiętnaście",
]
_TENS = [
    "",
    "dziesięć",
    "dwadzieścia",
    "trzydzieści",
    "czterdzieści",
    "pięćdziesiąt",
    "sześćdziesiąt",
    "siedemdziesiąt",
    "osiemdziesiąt",
    "dziewięćdziesiąt",
]
_HUNDREDS = [
    "",
    "sto",
    "dwieście",
    "trzysta",
    "czterysta",
    "pięćset",
    "sześćset",
    "siedemset",
    "osiemset",
    "dziewięćset",
]


def _parse_symbol_level(raw_level: object) -> int:
    if raw_level is None:
        return SYMBOL_LEVEL_MOST
    if isinstance(raw_level, str):
        key = raw_level.strip().lower()
        if key in ("none", "brak"):
            return SYMBOL_LEVEL_NONE
        if key in ("some", "część", "czesc"):
            return SYMBOL_LEVEL_SOME
        if key in ("most", "większość", "wiekszosc"):
            return SYMBOL_LEVEL_MOST
        if key in ("all", "wszystkie", "character"):
            return SYMBOL_LEVEL_ALL
        try:
            raw_level = int(key)
        except Exception:
            return SYMBOL_LEVEL_MOST
    try:
        lv = int(raw_level)
    except Exception:
        return SYMBOL_LEVEL_MOST
    # NVDA often stores enum values as larger ints (e.g. 0/100/200/300...).
    if lv <= 0:
        return SYMBOL_LEVEL_NONE
    if lv <= 3:
        return lv
    if lv < 150:
        return SYMBOL_LEVEL_SOME
    if lv < 250:
        return SYMBOL_LEVEL_MOST
    return SYMBOL_LEVEL_ALL


def _plural_form(n: int, forms: tuple[str, str, str]) -> str:
    if n == 1:
        return forms[0]
    last2 = n % 100
    last = n % 10
    if 10 < last2 < 20:
        return forms[2]
    if 2 <= last <= 4:
        return forms[1]
    return forms[2]


def _triplet_to_words(n: int) -> str:
    parts: List[str] = []
    h = n // 100
    t = (n // 10) % 10
    u = n % 10
    if h:
        parts.append(_HUNDREDS[h])
    if t == 1:
        parts.append(_TEENS[u])
    else:
        if t:
            parts.append(_TENS[t])
        if u:
            parts.append(_UNITS[u])
    return " ".join(parts).strip()


def int_to_polish_words(n: int) -> str:
    if n == 0:
        return "zero"
    if n < 0:
        return "minus " + int_to_polish_words(-n)

    groups = [
        (("tysiąc", "tysiące", "tysięcy")),
        (("milion", "miliony", "milionów")),
        (("miliard", "miliardy", "miliardów")),
        (("bilion", "biliony", "bilionów")),
    ]
    out: List[str] = []
    chunk0 = n % 1000
    if chunk0:
        out.append(_triplet_to_words(chunk0))
    n //= 1000
    gi = 0
    while n > 0:
        chunk = n % 1000
        if chunk:
            forms = groups[gi] if gi < len(groups) else ("", "", "")
            if chunk == 1 and gi == 0:
                out.append(forms[0])
            else:
                w = _triplet_to_words(chunk)
                label = _plural_form(chunk, forms)
                out.append((w + " " + label).strip())
        n //= 1000
        gi += 1
    return " ".join(reversed([x for x in out if x])).strip()


def _expand_numbers(text: str) -> str:
    def repl(match: re.Match[str]) -> str:
        token = match.group(0)
        try:
            return " " + int_to_polish_words(int(token)) + " "
        except Exception:
            return " " + token + " "

    return re.sub(r"(?<![\w/.-])-?\d+(?![\w/.-])", repl, text)


def _spell_character(ch: str, symbol_level: int) -> str:
    lo = ch.lower()
    if lo in LETTER_NAMES:
        return LETTER_NAMES[lo]
    if ch.isdigit():
        return int_to_polish_words(int(ch))
    sym = SYMBOLS.get(ch)
    if sym:
        return sym[0]
    if ch.isspace():
        return "przerwa"
    return ch


def _expand_symbols(text: str, symbol_level: int, force_all: bool) -> str:
    out: List[str] = []
    for ch in text:
        item = SYMBOLS.get(ch)
        if not item:
            out.append(ch)
            continue
        spoken, min_level = item
        if force_all or symbol_level >= min_level:
            out.append(f" {spoken} ")
        else:
            out.append(ch)
    return "".join(out)


def _normalize_text_for_tokens(text: str) -> str:
    normalized = text.lower().strip()
    normalized = normalized.replace("ó", "u").replace("ch", "h").replace("rz", "ż")
    for pattern, softened in {"ci": "ć", "si": "ś", "zi": "ź", "ni": "ń", "dzi": "dź"}.items():
        for vowel in "aeiouyąę":
            normalized = normalized.replace(pattern + vowel, softened + vowel)
        normalized = normalized.replace(pattern, softened + "i")

    nasal: List[str] = []
    for idx, ch in enumerate(normalized):
        nxt = normalized[idx + 1] if idx + 1 < len(normalized) else " "
        if ch == "ę":
            if nxt == " ":
                nasal.append("e")
            elif nxt in "pb":
                nasal.append("em")
            elif nxt in "tdcz":
                nasal.append("en")
            else:
                nasal.append("ę")
        elif ch == "ą":
            if nxt == " ":
                nasal.append("oł")
            elif nxt in "pb":
                nasal.append("om")
            elif nxt in "tdcz":
                nasal.append("on")
            else:
                nasal.append("ą")
        else:
            nasal.append(ch)
    return "".join(nasal)


def process_text(
    text: str,
    symbol_level: object = None,
    character_mode: bool = False,
) -> List[str]:
    lv = _parse_symbol_level(symbol_level)
    src = (text or "").strip()
    if not src:
        return []

    if character_mode:
        spoken = " ".join(_spell_character(ch, lv) for ch in src)
    else:
        expanded = _expand_numbers(src)
        spoken = _expand_symbols(expanded, lv, force_all=False)

    # Keep punctuation markers as pauses for rhythm.
    spoken = re.sub(r"[,:;]", " | ", spoken)
    spoken = re.sub(r"[.!?]", " || ", spoken)
    normalized = _normalize_text_for_tokens(spoken)

    tokens: List[str] = []
    idx = 0
    while idx < len(normalized):
        ch = normalized[idx]
        if ch in ("|",):
            if idx + 1 < len(normalized) and normalized[idx + 1] == "|":
                tokens.append("||")
                idx += 2
            else:
                tokens.append("|")
                idx += 1
            continue
        found = False
        for digraph in ("sz", "cz", "dz", "dź", "dż", "ch", "rz"):
            if normalized[idx : idx + len(digraph)] == digraph:
                tokens.append("h" if digraph == "ch" else ("ż" if digraph == "rz" else digraph))
                idx += len(digraph)
                found = True
                break
        if not found:
            tokens.append(ch)
            idx += 1

    devoice = {
        "b": "p",
        "d": "t",
        "g": "k",
        "z": "s",
        "ż": "sz",
        "ź": "ś",
        "w": "f",
        "dz": "c",
        "dź": "ć",
        "dż": "cz",
    }
    voiceless = {"p", "t", "k", "s", "c", "ś", "f", "h", "sz", "cz"}
    for i, token in enumerate(tokens):
        if token in ("|", "||", " "):
            continue
        is_last = i == len(tokens) - 1
        nxt = tokens[i + 1] if not is_last else ""
        if token in devoice and (is_last or nxt in voiceless or nxt in ("|", "||", " ")):
            tokens[i] = devoice[token]

    return tokens


class SAMCore:
    def __init__(self, sample_rate: int = SAMPLE_RATE):
        self.sample_rate = sample_rate
        self.cur_f = [500.0, 1000.0, 2000.0]

    def _pitch_factor(self, rel_pos: float, intonation: str) -> float:
        if intonation == "question":
            if rel_pos < 0.65:
                return 0.98 + 0.03 * rel_pos
            rise = (rel_pos - 0.65) / 0.35
            return 1.0 + 0.22 * max(0.0, min(1.0, rise))
        if intonation == "exclamation":
            return 1.08 - 0.06 * rel_pos
        return 1.03 - 0.11 * rel_pos

    def render(
        self,
        tokens: List[str],
        rate_mult: float,
        pitch_hz: float,
        volume_mult: float,
        rng: random.Random,
        intonation: str = "statement",
    ) -> bytes:
        output: List[float] = []
        speech_tokens = [t for t in tokens if t not in (" ", "|", "||")]
        speech_count = max(1, len(speech_tokens))
        spoken_idx = 0

        for token in tokens:
            if token == " ":
                output.extend([0.0] * int(self.sample_rate * 0.035 * rate_mult))
                continue
            if token == "|":
                output.extend([0.0] * int(self.sample_rate * 0.075 * rate_mult))
                continue
            if token == "||":
                output.extend([0.0] * int(self.sample_rate * 0.14 * rate_mult))
                continue

            rel_pos = min(1.0, spoken_idx / float(max(1, speech_count - 1)))
            spoken_idx += 1
            local_pitch = max(45.0, pitch_hz * self._pitch_factor(rel_pos, intonation))
            dur = 0.1 * rate_mult
            if token in ("m", "n", "ń", "l", "ł", "r", "j", "w"):
                dur = 0.08 * rate_mult
            if token in ("p", "b", "t", "d", "k", "g", "c", "ć", "cz", "dz", "dź", "dż"):
                dur = 0.055 * rate_mult

            if token == "r":
                for _ in range(3):
                    output.extend(
                        self._gen_rect_pulse(
                            430.0,
                            1180.0,
                            2350.0,
                            int(self.sample_rate / (local_pitch * 3.2)),
                        )
                    )
                    output.extend([0.0] * int(self.sample_rate * 0.01))
                continue

            if token in PHONEMES:
                target_f = PHONEMES[token]
                cycles = int((self.sample_rate * dur) / (self.sample_rate / local_pitch))
                for _ in range(max(1, cycles)):
                    for i in range(3):
                        self.cur_f[i] += (target_f[i] - self.cur_f[i]) * 0.24
                    output.extend(
                        self._gen_rect_pulse(
                            self.cur_f[0],
                            self.cur_f[1],
                            self.cur_f[2],
                            int(self.sample_rate / local_pitch),
                        )
                    )
            elif token in ("s", "sz", "ś", "f", "ch", "h"):
                f_noise = 2300.0 if token in ("sz",) else (6200.0 if token in ("ś",) else 4300.0)
                amp = 0.26 if token in ("sz", "ś") else 0.2
                length = int(self.sample_rate * dur * (1.15 if token in ("sz", "ś") else 1.0))
                for _ in range(length):
                    v = rng.uniform(-1.0, 1.0)
                    if rng.random() > (f_noise / self.sample_rate):
                        v = 0.0
                    output.append(v * amp)
            elif token in ("z", "ż", "ź", "w"):
                base = self._gen_rect_pulse(380.0, 1400.0, 2300.0, int(self.sample_rate / local_pitch))
                f_noise = 2300.0 if token == "ż" else (5600.0 if token == "ź" else 4200.0)
                cycles = int((self.sample_rate * dur) / len(base)) if base else 1
                for _ in range(max(1, cycles)):
                    for v in base:
                        n = rng.uniform(-1.0, 1.0)
                        if rng.random() > (f_noise / self.sample_rate):
                            n = 0.0
                        output.append(v * 0.7 + n * 0.18)
            elif token in ("p", "b", "t", "d", "k", "g"):
                output.extend([0.0] * int(self.sample_rate * 0.016))
                amp = 0.34 if token in ("p", "t", "k") else 0.2
                for _ in range(int(self.sample_rate * 0.01)):
                    output.append(rng.uniform(-1.0, 1.0) * amp)
            elif token in ("cz", "c", "ć", "dz", "dź", "dż"):
                output.extend([0.0] * int(self.sample_rate * 0.012))
                f_noise = 2300.0 if token in ("cz", "dż") else (6000.0 if token in ("ć", "dź") else 4300.0)
                amp = 0.28
                for _ in range(int(self.sample_rate * 0.045 * rate_mult)):
                    v = rng.uniform(-1.0, 1.0)
                    if rng.random() > (f_noise / self.sample_rate):
                        v = 0.0
                    output.append(v * amp)

            output.extend([0.0] * int(self.sample_rate * 0.004))

        if not output:
            return b""

        mx = max(abs(x) for x in output) or 1.0
        data = bytearray()
        for x in output:
            s = (x / mx) * volume_mult
            s_q = round(s * 7.5) / 7.5
            v = int(s_q * 32767.0)
            data.extend(struct.pack("<h", max(-32768, min(32767, v))))
        return bytes(data)

    def _gen_rect_pulse(self, f1: float, f2: float, f3: float, length: int) -> List[float]:
        c: List[float] = []
        for n in range(max(1, length)):
            t = n / self.sample_rate
            v = (
                math.sin(2 * math.pi * f1 * t) * 0.82
                + math.sin(2 * math.pi * f2 * t) * 0.5
                + math.sin(2 * math.pi * f3 * t) * 0.28
            )
            v *= math.exp(-t * 360.0)
            c.append(v)
        return c


def _default_seed(text: str, rate: int, pitch: int, volume: int) -> int:
    seed_data = f"{text}|{rate}|{pitch}|{volume}"
    return sum(ord(ch) * (idx + 1) for idx, ch in enumerate(seed_data)) & 0xFFFFFFFF


def synthesize_pcm16(
    text: str,
    rate: int = 50,
    pitch: int = 50,
    volume: int = 100,
    seed: Optional[int] = None,
    symbol_level: object = None,
    character_mode: bool = False,
) -> bytes:
    cleaned = (text or "").strip()
    if not cleaned:
        return b""

    seed_value = _default_seed(cleaned, rate, pitch, volume) if seed is None else seed
    rng = random.Random(seed_value)
    rate_mult = 1.0 / (0.4 + (float(rate) / 100.0) * 2.0)
    pitch_hz = 60.0 + (float(pitch) / 100.0) * 100.0
    volume_mult = max(0.0, min(2.0, float(volume) / 100.0))

    parts = re.findall(r"[^.!?]+[.!?]?", cleaned) or [cleaned]
    chunks: List[bytes] = []
    core = SAMCore(sample_rate=SAMPLE_RATE)
    for raw_part in parts:
        part = raw_part.strip()
        if not part:
            continue
        tail = part[-1] if part and part[-1] in ".!?" else ""
        body = part[:-1].strip() if tail else part
        tokens = process_text(body, symbol_level=symbol_level, character_mode=character_mode)
        if not tokens:
            continue
        intonation = "statement"
        if tail == "?":
            intonation = "question"
        elif tail == "!":
            intonation = "exclamation"
        chunks.append(
            core.render(
                tokens=tokens,
                rate_mult=rate_mult,
                pitch_hz=pitch_hz,
                volume_mult=volume_mult,
                rng=rng,
                intonation=intonation,
            )
        )
        if tail == "?":
            chunks.append(b"\x00\x00" * int(SAMPLE_RATE * 0.09))
        elif tail == "!":
            chunks.append(b"\x00\x00" * int(SAMPLE_RATE * 0.06))
        else:
            chunks.append(b"\x00\x00" * int(SAMPLE_RATE * 0.11))
    return b"".join(chunks)
