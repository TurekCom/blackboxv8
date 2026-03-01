# -*- coding: UTF-8 -*-
import queue
import threading

import buildVersion
import config
import logHandler
import nvwave
from synthDriverHandler import SynthDriver, synthDoneSpeaking

try:
    from . import blackbox_core, blackbox_runtime
except Exception:
    import blackbox_core  # type: ignore
    import blackbox_runtime  # type: ignore


class SynthDriver(SynthDriver):
    name = "blackbox_v8"
    description = "BlackBox v8 Polish Synthesizer (SAM-Core)"
    supportedSettings = (
        SynthDriver.RateSetting(),
        SynthDriver.PitchSetting(),
        SynthDriver.VolumeSetting(),
    )
    _rate = 50
    _pitch = 50
    _volume = 100

    @classmethod
    def check(cls):
        return True

    def __init__(self):
        super(SynthDriver, self).__init__()
        self._queue = queue.Queue()
        self._stop = threading.Event()
        self.runtime = blackbox_runtime.BlackboxRuntime()
        device = (
            config.conf["audio"]["outputDevice"]
            if buildVersion.version_year >= 2025
            else config.conf["speech"]["outputDevice"]
        )
        self.player = nvwave.WavePlayer(
            channels=1,
            samplesPerSec=blackbox_core.SAMPLE_RATE,
            bitsPerSample=16,
            outputDevice=device,
        )
        self._thread = threading.Thread(target=self._worker, daemon=True)
        self._thread.start()

    def _worker(self):
        while True:
            payload = self._queue.get()
            if payload is None:
                break
            if self._stop.is_set():
                self._queue.task_done()
                continue
            if isinstance(payload, dict):
                text = str(payload.get("text", ""))
                character_mode = bool(payload.get("character_mode", False))
                symbol_level = payload.get("symbol_level", None)
            else:
                text = str(payload)
                character_mode = False
                symbol_level = None
            try:
                data = self.runtime.synthesize(
                    text=text,
                    rate=self._rate,
                    pitch=self._pitch,
                    volume=self._volume,
                    symbol_level=symbol_level,
                    character_mode=character_mode,
                )
                if data and not self._stop.is_set():
                    self.player.feed(data)
                    synthDoneSpeaking.notify(synth=self)
            except Exception:
                logHandler.log.error("BlackBox SAM-Core error", exc_info=True)
            self._queue.task_done()

    @staticmethod
    def _extract_character_mode(item):
        try:
            if item.__class__.__name__ != "CharacterModeCommand":
                return None
            for attr in ("state", "isOn", "enabled", "enable"):
                if hasattr(item, attr):
                    return bool(getattr(item, attr))
        except Exception:
            return None
        return None

    @staticmethod
    def _symbol_level_from_config():
        try:
            return config.conf["speech"].get("symbolLevel", None)
        except Exception:
            return None

    def speak(self, seq):
        symbol_level = self._symbol_level_from_config()
        chunks = []
        cur_text = []
        character_mode = False
        for item in seq:
            if isinstance(item, str):
                cur_text.append(item)
                continue
            mode = self._extract_character_mode(item)
            if mode is None:
                continue
            if cur_text:
                chunks.append(("".join(cur_text), character_mode))
                cur_text = []
            character_mode = mode
        if cur_text:
            chunks.append(("".join(cur_text), character_mode))

        if not chunks:
            text = "".join([item for item in seq if isinstance(item, str)])
            if text.strip():
                chunks = [(text, False)]
        if not chunks:
            return

        self._stop.clear()
        for text, char_mode in chunks:
            if not text.strip():
                continue
            self._queue.put(
                {
                    "text": text,
                    "character_mode": char_mode,
                    "symbol_level": symbol_level,
                }
            )

    def cancel(self):
        self._stop.set()
        self.player.stop()
        try:
            while not self._queue.empty():
                self._queue.get_nowait()
                self._queue.task_done()
        except Exception:
            pass

    def pause(self, switch):
        self.player.pause(switch)

    def terminate(self):
        self._queue.put(None)
        self._thread.join()
        self.player.close()

    def _get_rate(self):
        return self._rate

    def _set_rate(self, value):
        self._rate = value

    def _get_pitch(self):
        return self._pitch

    def _set_pitch(self, value):
        self._pitch = value

    def _get_volume(self):
        return self._volume

    def _set_volume(self, value):
        self._volume = value

    def _get_language(self):
        return "pl"
