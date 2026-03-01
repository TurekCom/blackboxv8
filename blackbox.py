import math
import struct
import random

# Próba importu num2words, jeśli nie ma - używamy prostego mapowania
try:
    from num2words import num2words
except ImportError:
    def num2words(n, lang='pl'):
        return str(n)

SAMPLE_RATE = 22050

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
    'ś': 'eś', 't': 'te', 'u': 'u', 'w': 'wu', 
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
        y = (self.b0/self.a0)*x + (self.b1/self.a0)*self.x1 + (self.b2/self.a0)*self.x2 \
            - (self.a1/self.a0)*self.y1 - (self.a2/self.a0)*self.y2
        self.x2, self.x1 = self.x1, x
        self.y2, self.y1 = self.y1, y
        return y

def polish_g2p_v8(text):
    words = []
    for w in text.split():
        if w.isdigit(): words.append(num2words(int(w), lang='pl'))
        else:
            clean_w = ""
            for char in w:
                if char in PUNCTUATION_NAMES:
                    if clean_w: words.append(clean_w); clean_w = ""
                    words.append(PUNCTUATION_NAMES[char])
                else: clean_w += char
            if clean_w: words.append(clean_w)
    
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
            if text[i:i+len(dg)] == dg:
                res.append(dg); i += len(dg); found = True; break
        if not found: res.append(text[i]); i += 1
    devoice = {'b': 'p', 'd': 't', 'g': 'k', 'z': 's', 'ż': 'sz', 'ź': 'ś', 'w': 'f', 'dz': 'c', 'dź': 'ć', 'dż': 'cz'}
    if res and res[-1] in devoice: res[-1] = devoice[res[-1]]
    return res

def get_audio_data(text):
    tokens = polish_g2p_v8(text)
    full_samples = []
    for idx, p in enumerate(tokens):
        samples = []
        is_last = (idx == len(tokens) - 1)
        if p in VOWEL_FORMANTS:
            filters = [BiquadBP(f, q=8.0) for f, a in VOWEL_FORMANTS[p]]
            amps = [a for f, a in VOWEL_FORMANTS[p]]
            for n in range(int(SAMPLE_RATE * 0.18)):
                src = ((n * 100 / SAMPLE_RATE) % 1.0) * 2 - 1
                val = sum(f.process(src) * amps[i] for i, f in enumerate(filters))
                samples.append(val)
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
            full_samples.extend(samples)
            full_samples.extend([0] * int(SAMPLE_RATE * 0.01))

    if not full_samples: return b""
    m = max(abs(s) for s in full_samples)
    if m > 0: full_samples = [s/m for s in full_samples]
    
    data = b""
    for s in full_samples:
        v = int(s * 32767)
        data += struct.pack("<h", max(-32768, min(32767, v)))
    return data
