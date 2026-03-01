import synthDriverHandler
try:
    from . import blackbox_engine as blackbox
except ImportError:
    import blackbox_engine as blackbox
import queue
import threading
import logHandler
import nvwave
import config
from synthDriverHandler import SynthDriver, synthDoneSpeaking

class SynthDriver(SynthDriver):
    name = "blackbox_v8"
    description = "BlackBox v8 Polish Synthesizer"

    @classmethod
    def check(cls):
        return True

    def __init__(self):
        super(SynthDriver, self).__init__()
        self._queue = queue.Queue()
        self._stop_event = threading.Event()
        try:
            device = config.conf["audio"]["outputDevice"]
        except:
            device = config.conf["speech"]["outputDevice"]
        self.player = nvwave.WavePlayer(channels=1, samplesPerSec=22050, bitsPerSample=16, outputDevice=device)
        self._thread = threading.Thread(target=self._worker)
        self._thread.daemon = True
        self._thread.start()

    def _worker(self):
        while True:
            text = self._queue.get()
            if text is None: break
            if self._stop_event.is_set():
                self._queue.task_done(); continue
            try:
                data = blackbox.get_audio_data(text)
                if data and not self._stop_event.is_set():
                    self.player.feed(data)
                    synthDoneSpeaking.notify(synth=self)
            except Exception:
                logHandler.log.error("BlackBox v8 error", exc_info=True)
            self._queue.task_done()

    def speak(self, speechSequence):
        text = ""
        for item in speechSequence:
            if isinstance(item, str): text += item
        if text.strip():
            self._stop_event.clear()
            self._queue.put(text)

    def cancel(self):
        self._stop_event.set()
        self.player.stop()
        try:
            while not self._queue.empty():
                self._queue.get_nowait()
                self._queue.task_done()
        except: pass

    def pause(self, switch):
        self.player.pause(switch)

    def terminate(self):
        self._queue.put(None)
        self._thread.join()
        self.player.close()
