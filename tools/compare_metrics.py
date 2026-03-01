import wave, numpy as np
from pathlib import Path

def analyze(path):
    with wave.open(str(path), 'rb') as w:
        sr = w.getframerate(); n=w.getnframes(); x=np.frombuffer(w.readframes(n), dtype=np.int16).astype(np.float32)/32768.0
    frame=int(0.03*sr); hop=int(0.01*sr)
    frames=[x[i:i+frame] for i in range(0,len(x)-frame,hop)]
    rms=float(np.sqrt(np.mean(x*x)))
    sil=float(np.mean(np.abs(x)<0.015))
    rms_f=np.array([np.sqrt(np.mean(f*f))+1e-12 for f in frames])
    voiced=rms_f>max(0.02,np.percentile(rms_f,35))
    cent=[]
    for f in frames:
        F=np.fft.rfft(f*np.hanning(len(f))); mag=np.abs(F); freqs=np.fft.rfftfreq(len(f),1/sr); cent.append(float((freqs*mag).sum()/(mag.sum()+1e-12)))
    cent=np.array(cent)
    f0s=[]; min_f,max_f=60,350; min_lag,max_lag=int(sr/max_f),int(sr/min_f)
    for f,v in zip(frames,voiced):
        if not v: continue
        ff=f-np.mean(f)
        if np.max(np.abs(ff))<0.02: continue
        ac=np.correlate(ff,ff,mode='full')[len(ff)-1:]
        ac[:min_lag]=0
        if max_lag < len(ac): ac[max_lag:]=0
        lag=int(np.argmax(ac))
        if lag>0:
            f0=sr/lag
            if min_f<=f0<=max_f: f0s.append(f0)
    out={
      'dur':round(len(x)/sr,2),'rms':round(rms,4),'sil':round(sil,4),
      'voiced':round(float(np.mean(voiced)),4),
      'cent':round(float(np.mean(cent[voiced])) if np.any(voiced) else float(np.mean(cent)),1),
      'f0med':round(float(np.median(f0s)) if f0s else 0,1),
      'f0p10':round(float(np.percentile(f0s,10)) if f0s else 0,1),
      'f0p90':round(float(np.percentile(f0s,90)) if f0s else 0,1),
    }
    return out

for p in [
    Path('test_outputs/yt_ref/ref_60s.wav'),
    Path('test_outputs/sapi5_x64_c64_cluster.wav'),
    Path('test_outputs/sapi5_x64_stmt.wav'),
    Path('test_outputs/sapi5_x64_question.wav')
]:
    if p.exists():
        print(p.name, analyze(p))
