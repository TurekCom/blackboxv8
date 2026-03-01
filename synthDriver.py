import math
import struct
import random
import queue
import threading
import nvwave
import config
import logHandler
import synthDriverHandler
from synthDriverHandler import SynthDriver, synthDoneSpeaking

# Próba importu num2words
try:
    from num2words import num2words
except ImportError:
    def num2words(n, lang='pl'): return str(n)

SAMPLE_RATE = 22050

# --- SILNIK SYNTEZY ---
VOWEL_FORMANTS = {
    'a': [(750, 1.0), (1200, 0.6), (2400, 0.4)],
    'e': [(500, 1.0), (1700, 0.8), (2500, 0.5)],
    'i': [(250, 1.0), (2300, 0.9), (3000, 0.6)],
    'o': [(500, 1.0), (1000, 0.7), (2400, 0.4)],
    'u': [(300, 1.0), (700, 0.6), (2300, 0.4)],
    'y': [(350, 1.0), (1400, 0.7), (2300, 0.5)],
    'ą': [(550, 0.8), (1000, 0.5), (2400, 0.3)],
    'ę': [(500, 0.8), (1600, 0.6), (2500, 0.4)],
}

PUNCTUATION_NAMES = {
    '.': 'kropka', ',': 'przecinek', '?': 'pytajnik', '!': 'wykrzyknik',
    ':': 'dwukropek', ';': 'średnik', '-': 'myślnik', '(': 'nawias',
    ')': 'nawias zamknij', '"': 'cudzysłów', '@': 'małpa', '#': 'hasz',
    '$': 'dolar', '%': 'procent', '*': 'gwiazdka', '+': 'plus', '=': 'równa się'
}

LETTER_NAMES = {
    'a': 'a', 'ą': 'oł', 'b': 'be', 'c': 'ce', 'ć': 'cie', 
    'd': 'de', 'e': 'e', 'ę': 'eł', 'f': 'ef', 'g': 'gie', 
    'h': 'ha', 'i': 'i', 'j': 'jot', 'k': 'ka', 'l': 'el', 
    'ł': 'eu', 'm': 'em', 'n': 'en', 'ń': 'eń', 'o': 'o', 
    'ó': 'u', 'p': 'pe', 'r': 'er', 's': 'es', 
    'ś': 'eś', 't': 'te', 'u': 'u', 'v': 'fał', 'w': 'wu', 
    'y': 'igrek', 'z': 'zet', 'ź': 'ziet', 'ż': 'żet'
}

NOISE_PHONEMES = {
    's': (4000, 8000, 0.5, 0.12), 'sz': (1800, 3500, 0.7, 0.18),
    'ś': (5000, 9000, 0.6, 0.15), 'f': (1500, 7000, 0.2, 0.12),
    'ch': (1200, 3000, 0.4, 0.16), 'z': (4000, 8000, 0.2, 0.12),
    'ż': (1800, 3500, 0.3, 0.18), 'ź': (5000, 9000, 0.2, 0.15),
}

class BiquadBP:
    def __init__(self, freq, q=5.0):
        w0 = 2 * math.pi * freq / SAMPLE_RATE
        alpha = math.sin(w0) / (2 * q)
        self.b0 = alpha; self.b1 = 0; self.b2 = -alpha
        self.a0 = 1 + alpha; self.a1 = -2 * math.cos(w0); self.a2 = 1 - alpha
        self.x1 = 0; self.x2 = 0; self.y1 = 0; self.y2 = 0
    def process(self, x):
        y = (self.b0/self.a0)*x + (self.b1/self.a0)*self.x1 + (self.b2/self.a0)*self.x2 
            - (self.a1/self.a0)*self.y1 - (self.a2/self.a0)*self.y2
        self.x2, self.x1 = self.x1, x
        self.y2, self.y1 = self.y1, y
        return y

def g2p(text):
    words = []
    for w in text.split():
        if w.isdigit(): words.append(num2words(int(w), lang='pl'))
        else:
            cw = ""
            for c in w:
                if c in PUNCTUATION_NAMES:
                    if cw: words.append(cw); cw = ""
                    words.append(PUNCTUATION_NAMES[c])
                else: cw += c
            if cw: words.append(cw)
    text = " ".join(words).lower().strip()
    if len(text) == 1 and text in LETTER_NAMES: text = LETTER_NAMES[text]
    text = text.replace('ó', 'u').replace('ch', 'h').replace('rz', 'ż')
    repl = {'ci': 'ć', 'si': 'ś', 'zi': 'ź', 'ni': 'ń', 'dzi': 'dź'}
    for p, s in repl.items():
        for v in 'aeiouyąę': text = text.replace(p + v, s + v)
        text = text.replace(p, s + 'i')
    res = []
    i = 0
    while i < len(text):
        found = False
        for dg in ['sz', 'cz', 'dz', 'dź', 'dż']:
            if text[i:i+len(dg)] == dg: res.append(dg); i += len(dg); found = True; break
        if not found: res.append(text[i]); i += 1
    devoice = {'b': 'p', 'd': 't', 'g': 'k', 'z': 's', 'ż': 'sz', 'ź': 'ś', 'w': 'f', 'dz': 'c', 'dź': 'ć', 'dż': 'cz'}
    if res and res[-1] in devoice: res[-1] = devoice[res[-1]]
    return res

def get_wav(text):
    tokens = g2p(text)
    full = []
    for idx, p in enumerate(tokens):
        samples = []
        is_last = (idx == len(tokens) - 1)
        if p in VOWEL_FORMANTS:
            filters = [BiquadBP(f, q=8.0) for f, a in VOWEL_FORMANTS[p]]
            amps = [a for f, a in VOWEL_FORMANTS[p]]
            for n in range(int(SAMPLE_RATE * 0.18)):
                src = ((n * 100 / SAMPLE_RATE) % 1.0) * 2 - 1
                samples.append(sum(f.process(src) * amps[i] for i, f in enumerate(filters)))
        elif p in NOISE_PHONEMES:
            l, h, amp, dur = NOISE_PHONEMES[p]
            filt = BiquadBP((l+h)/2, q=2.0)
            for _ in range(int(SAMPLE_RATE * (dur * 1.5 if is_last else dur))):
                samples.append(filt.process(random.uniform(-1, 1)) * amp)
        elif p == ' ': samples = [0] * int(SAMPLE_RATE * 0.1)
        if samples:
            m = max(abs(s) for s in samples)
            if m > 0: samples = [s/m for s in samples]
            if is_last: samples = [s * 1.2 for s in samples]
            full.extend(samples)
            full.extend([0] * int(SAMPLE_RATE * 0.01))
    if not full: return b""
    m = max(abs(s) for s in full)
    if m > 0: full = [s/m for s in full]
    data = b""
    for s in full:
        v = int(s * 32767)
        data += struct.pack("<h", max(-32768, min(32767, v)))
    return data

# --- STEROWNIK NVDA ---
class SynthDriver(SynthDriver):
    name = "blackbox_v8"
    description = "BlackBox v8 Polish Synthesizer"
    
    # Wymagane w NVDA 2025.x
    supportedSettings = (
        SynthDriver.RateSetting(),
        SynthDriver.PitchSetting(),
        SynthDriver.VolumeSetting(),
    )

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
                data = get_wav(text)
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
