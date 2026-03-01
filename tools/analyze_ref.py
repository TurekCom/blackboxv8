import wave, numpy as np, sys
from pathlib import Path

path = Path('test_outputs/yt_ref/ref_60s.wav')
with wave.open(str(path), 'rb') as w:
    sr = w.getframerate()
    n = w.getnframes()
    x = np.frombuffer(w.readframes(n), dtype=np.int16).astype(np.float32)/32768.0

# Basic stats
rms = float(np.sqrt(np.mean(x*x)))

# silence ratio
sil_thr = 0.015
silence_ratio = float(np.mean(np.abs(x) < sil_thr))

# frame analysis
frame = int(0.03*sr)
hop = int(0.01*sr)
frames = []
for i in range(0, len(x)-frame, hop):
    f = x[i:i+frame]
    frames.append(f)

rms_f = np.array([np.sqrt(np.mean(f*f))+1e-12 for f in frames])
voiced = rms_f > max(0.02, np.percentile(rms_f, 35))

# zcr
zcr = np.array([np.mean(np.abs(np.diff(np.signbit(f)))) for f in frames])
zcr_v = float(np.mean(zcr[voiced])) if np.any(voiced) else float(np.mean(zcr))

# centroid
cent = []
for f in frames:
    F = np.fft.rfft(f*np.hanning(len(f)))
    mag = np.abs(F)
    freqs = np.fft.rfftfreq(len(f), 1/sr)
    s = mag.sum()+1e-12
    cent.append(float((freqs*mag).sum()/s))
cent = np.array(cent)
cent_v = float(np.mean(cent[voiced])) if np.any(voiced) else float(np.mean(cent))

# crude F0 autocorr
f0s=[]
min_f,max_f=60,350
min_lag,max_lag=int(sr/max_f), int(sr/min_f)
for f,v in zip(frames, voiced):
    if not v:
        continue
    ff = f - np.mean(f)
    if np.max(np.abs(ff)) < 0.02:
        continue
    ac = np.correlate(ff, ff, mode='full')[len(ff)-1:]
    ac[:min_lag]=0
    if max_lag < len(ac):
        ac[max_lag:]=0
    lag = int(np.argmax(ac))
    if lag>0:
        f0 = sr/lag
        if min_f<=f0<=max_f:
            f0s.append(f0)

if f0s:
    f0_med = float(np.median(f0s))
    f0_p10 = float(np.percentile(f0s,10))
    f0_p90 = float(np.percentile(f0s,90))
else:
    f0_med=f0_p10=f0_p90=0.0

print('sr',sr)
print('dur_s',round(len(x)/sr,2))
print('rms',round(rms,4))
print('silence_ratio',round(silence_ratio,4))
print('voiced_frames_ratio',round(float(np.mean(voiced)),4))
print('zcr_voiced',round(zcr_v,4))
print('centroid_voiced_hz',round(cent_v,1))
print('f0_med_hz',round(f0_med,1))
print('f0_p10_p90',round(f0_p10,1),round(f0_p90,1))
