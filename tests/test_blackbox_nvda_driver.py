import importlib.util
import os
import sys
import types
import unittest


ROOT = os.path.dirname(os.path.dirname(__file__))
DRIVER_PATH = os.path.join(ROOT, "blackbox_v8_driver.py")


class _NotifyAction:
    def __init__(self):
        self.calls = []

    def notify(self, **kwargs):
        self.calls.append(kwargs)


class _FakeBaseSynthDriver:
    def __init__(self):
        pass

    @classmethod
    def RateSetting(cls):
        return ("rate",)

    @classmethod
    def PitchSetting(cls):
        return ("pitch",)

    @classmethod
    def VolumeSetting(cls):
        return ("volume",)


class _FakeWavePlayer:
    def __init__(self, **kwargs):
        self.kwargs = kwargs
        self.feeds = []
        self.paused = []
        self.stopped = 0
        self.closed = 0

    def feed(self, data, onDone=None):
        self.feeds.append(bytes(data))
        if onDone:
            onDone()

    def idle(self):
        return

    def stop(self):
        self.stopped += 1

    def pause(self, switch):
        self.paused.append(bool(switch))

    def close(self):
        self.closed += 1


class _FakeIndexCommand:
    def __init__(self, index):
        self.index = index


class _FakeCharacterModeCommand:
    def __init__(self, state):
        self.state = state


class _FakeBreakCommand:
    def __init__(self, time):
        self.time = time


class _FakePitchCommand:
    def __init__(self, multiplier):
        self.multiplier = multiplier


class _FakeRateCommand:
    def __init__(self, multiplier):
        self.multiplier = multiplier


class _FakeVolumeCommand:
    def __init__(self, multiplier):
        self.multiplier = multiplier


class _FakeNativeBlackBox:
    sample_rate = 22050

    def __init__(self):
        self.calls = []

    @classmethod
    def can_load(cls):
        return True

    def synthesize(self, text, rate, pitch, volume, modulation):
        self.calls.append(
            {
                "text": text,
                "rate": rate,
                "pitch": pitch,
                "volume": volume,
                "modulation": modulation,
            },
        )
        return f"{text}|{rate}|{pitch}|{volume}|{modulation}".encode("utf-8")


def _load_driver_module():
    for key in list(sys.modules):
        if key in {
            "blackbox_v8_driver_under_test",
            "buildVersion",
            "config",
            "logHandler",
            "nvwave",
            "speech",
            "speech.commands",
            "synthDriverHandler",
        }:
            del sys.modules[key]

    build_version = types.ModuleType("buildVersion")
    build_version.version_year = 2025
    sys.modules["buildVersion"] = build_version

    config = types.ModuleType("config")
    config.conf = {
        "audio": {"outputDevice": None},
        "speech": {"outputDevice": None},
    }
    sys.modules["config"] = config

    log_handler = types.ModuleType("logHandler")
    log_handler.log = types.SimpleNamespace(
        debug=lambda *a, **k: None,
        error=lambda *a, **k: None,
    )
    sys.modules["logHandler"] = log_handler

    nvwave = types.ModuleType("nvwave")
    nvwave.WavePlayer = _FakeWavePlayer
    sys.modules["nvwave"] = nvwave

    speech_pkg = types.ModuleType("speech")
    speech_commands = types.ModuleType("speech.commands")
    speech_commands.IndexCommand = _FakeIndexCommand
    speech_commands.CharacterModeCommand = _FakeCharacterModeCommand
    speech_commands.BreakCommand = _FakeBreakCommand
    speech_commands.PitchCommand = _FakePitchCommand
    speech_commands.RateCommand = _FakeRateCommand
    speech_commands.VolumeCommand = _FakeVolumeCommand
    sys.modules["speech"] = speech_pkg
    sys.modules["speech.commands"] = speech_commands

    synth_driver_handler = types.ModuleType("synthDriverHandler")
    synth_driver_handler.SynthDriver = _FakeBaseSynthDriver
    synth_driver_handler.synthDoneSpeaking = _NotifyAction()
    synth_driver_handler.synthIndexReached = _NotifyAction()
    sys.modules["synthDriverHandler"] = synth_driver_handler

    spec = importlib.util.spec_from_file_location("blackbox_v8_driver_under_test", DRIVER_PATH)
    module = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    module.NativeBlackBox = _FakeNativeBlackBox
    module._read_user_percent = lambda _name, default: default
    return module


class BlackBoxNvdaDriverTests(unittest.TestCase):
    def test_check_uses_native_backend(self):
        module = _load_driver_module()
        self.assertTrue(module.SynthDriver.check())

    def test_driver_speaks_with_native_backend_and_notifies_index(self):
        module = _load_driver_module()
        synth = module.SynthDriver()
        try:
            seq = ["Czesc", _FakeIndexCommand(7), " Grzegorz"]
            synth.speak(seq)
            synth._queue.join()
            self.assertEqual(len(synth.player.feeds), 2)
            self.assertEqual(
                module.synthIndexReached.calls[0]["index"],
                7,
            )
            self.assertEqual(len(module.synthDoneSpeaking.calls), 1)
        finally:
            synth.terminate()

    def test_character_mode_and_break_create_separate_chunks(self):
        module = _load_driver_module()
        synth = module.SynthDriver()
        try:
            seq = [
                _FakeCharacterModeCommand(True),
                "ab.@",
                _FakeCharacterModeCommand(False),
                _FakeBreakCommand(50),
                "kot",
            ]
            synth.speak(seq)
            synth._queue.join()
            self.assertEqual(len(synth.player.feeds), 3)
            self.assertIn("a be kropka małpa".encode("utf-8"), synth.player.feeds[0])
            self.assertEqual(len(synth.player.feeds[1]), 2204)
            self.assertIn(b"kot", synth.player.feeds[2])
        finally:
            synth.terminate()

    def test_emoji_normalization_can_be_toggled(self):
        module = _load_driver_module()
        module._load_emoji_map = lambda: (
            {"😀": "uśmiechnięta buźka", "👋🏻": "machająca dłoń: karnacja jasna"},
            {
                "😀": [("😀", "uśmiechnięta buźka")],
                "👋": [("👋🏻", "machająca dłoń: karnacja jasna")],
            },
        )
        synth = module.SynthDriver()
        try:
            synth.speak(["Hej 😀 👋🏻"])
            synth._queue.join()
            self.assertIn("Hej uśmiechnięta buźka machająca dłoń: karnacja jasna".encode("utf-8"), synth.player.feeds[0])
            synth.cancel()
            synth._speakEmojis = False
            synth.speak(["Hej 😀"])
            synth._queue.join()
            self.assertIn("Hej 😀".encode("utf-8"), synth.player.feeds[-1])
        finally:
            synth.terminate()


if __name__ == "__main__":
    unittest.main()
