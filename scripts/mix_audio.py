"""Soundtrack for the neon ride, built from real CC0 recordings (OpenGameArt) in audio/wav/.

Sources (all CC0): Rain (loopable); Rain on Window Loop; wind1; wind whoosh loop; Space Winds;
Ghostly Humming; Rain + Long Thunder; Kenney Sci-Fi Sounds (spaceEngine*, engineCircular).

  /Applications/Blender.app/Contents/Resources/5.3/python/bin/python3.13 scripts/mix_audio.py
Writes audio/soundtrack.wav (48 kHz stereo): 20 s of picture plus 1 s into black.
"""
import math, os, wave
import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WAV = os.path.join(ROOT, "audio", "wav")
SR = 48000
PICTURE, TAIL = 20.0, 1.0
N = int(SR * (PICTURE + TAIL))
t = np.arange(N) / SR
SPEED_KEYS = [(0.0, 34.0), (1.2, 34.0), (4.4, 50.0), (6.2, 44.0), (8.4, 52.0), (10.4, 46.0), (12.5, 38.0),
              (15.4, 38.0), (18.6, 55.0), (21.0, 56.0)]          # keep in step with build_ride.py


def _speed_track(tt):
    v = np.full(tt.shape, SPEED_KEYS[0][1])
    for (t0, v0), (t1, v1) in zip(SPEED_KEYS, SPEED_KEYS[1:]):
        u = np.clip((tt - t0) / (t1 - t0), 0, 1)
        seg = (tt > t0) & (tt <= t1)
        v = np.where(seg, v0 + (v1 - v0) * u * u * (3 - 2 * u), v)
    return np.where(tt > SPEED_KEYS[-1][0], SPEED_KEYS[-1][1], v)
OVERTAKE_T = 4.0
JELLY_T0, JELLY_T1 = 12.0, 19.3


def load(name):
    with wave.open(os.path.join(WAV, name + ".wav")) as w:
        a = np.frombuffer(w.readframes(w.getnframes()), dtype="<i2").astype(np.float64) / 32768.0
    a = a.reshape(-1, 2).T
    return a / max(1e-9, np.abs(a).max())


def smoothstep(x):
    x = np.clip(x, 0, 1)
    return x * x * (3 - 2 * x)


def spectrum_filter(x, shape):
    X = np.fft.rfft(x, axis=-1)
    return np.fft.irfft(X * shape(np.fft.rfftfreq(x.shape[-1], 1 / SR)), x.shape[-1], axis=-1)


def lowpass(x, fc, order=2):
    return spectrum_filter(x, lambda f: 1 / (1 + (f / fc) ** (2 * order)) ** 0.5)


def highpass(x, fc, order=2):
    return spectrum_filter(x, lambda f: 1 - 1 / (1 + (f / max(fc, 1)) ** (2 * order)) ** 0.5)


def loop(x, n, xfade=0.25):
    """Repeat x with crossfades until it is n samples long."""
    xf = int(xfade * SR)
    out = np.zeros((2, n))
    pos, seg = 0, x.shape[1] - xf
    ramp = np.linspace(0, 1, xf)
    while pos < n:
        piece = x.copy()
        if pos > 0:
            piece[:, :xf] *= ramp
        piece[:, -xf:] *= ramp[::-1]
        end = min(n, pos + piece.shape[1])
        out[:, pos:end] += piece[:, :end - pos]
        pos += seg
    return out


def place(buf, sig, at, gain=1.0):
    i = int(at * SR)
    j = max(0, -i)
    i = max(0, i)
    m = min(N - i, sig.shape[1] - j)
    if m > 0:
        buf[:, i:i + m] += sig[:, j:j + m] * gain


def varispeed(x, rate):
    """Play x with a time-varying playback rate (array per output sample): pitch + doppler."""
    pos = np.cumsum(rate)
    pos = pos - pos[0]
    pos = np.clip(pos, 0, x.shape[1] - 2)
    i = pos.astype(int)
    f = pos - i
    return x[:, i] * (1 - f) + x[:, i + 1] * f


def pan(sig, p):
    """p in -1..1 (array or scalar), equal power."""
    a = (np.asarray(p) + 1) * math.pi / 4
    return np.vstack([sig.mean(0) * np.cos(a), sig.mean(0) * np.sin(a)])


def reverb(x, secs=2.4, wet=0.45, seed=3):
    rng = np.random.default_rng(seed)
    n = int(secs * SR)
    out = x * (1 - wet)
    L = 1 << int(np.ceil(np.log2(x.shape[1] + n)))
    for ch in (0, 1):
        ir = rng.standard_normal(n) * np.exp(-np.arange(n) / (secs / 5 * SR))
        ir = lowpass(ir[None, :], 4000)[0]
        ir /= np.sqrt((ir ** 2).sum())
        out[ch] += np.fft.irfft(np.fft.rfft(x[ch], L) * np.fft.rfft(ir, L), L)[:x.shape[1]] * wet
    return out


speed = _speed_track(t)                                  # the rider's speed, m/s
dist = np.cumsum(speed) / SR
GLASS_T = float(t[np.searchsorted(dist, 372.0)])
throttle = np.clip(np.gradient(speed, 1.0 / SR) / 6.0, 0, 1)     # 0..1 while accelerating
live = (t < PICTURE).astype(float)
glass = smoothstep((t - GLASS_T) / 0.6)
duck = np.ones(N)
mix = np.zeros((2, N))

# rain bed: the real rain, a little brighter on the open skyway, softened inside the glass
rain = loop(load("rain"), N)
mix += rain * 0.42 * (1 - 0.35 * glass) * duck

# rain drumming on the glass walls once the skyway closes in
win = loop(load("rain_on_window_loop"), N)
mix += highpass(win, 300) * 0.5 * glass * duck

# wind roar at speed: highpassed, with slow gusts, quieter behind the glass
wind = load("wind1")[:, int(8 * SR):int(8 * SR) + N]
wind = highpass(wind, 160)
gust = 0.75 + 0.25 * np.sin(2 * np.pi * 0.31 * t) + 0.1 * np.sin(2 * np.pi * 1.1 * t + 1)
mix += wind * 0.55 * gust * (1 - 0.55 * glass) * (speed / 44.0) ** 1.6 * duck

# the bike's electric drive follows the throttle: pitch climbs with speed, and it howls while accelerating
rev = (speed - 34.0) / 26.0                                # 0 at cruise .. 1 flat out
low = loop(load("spaceEngineLow_000"), 3 * N)
mix += varispeed(low, 0.75 + 0.55 * rev)[:, :N] * (0.26 + 0.2 * throttle) * duck
small = loop(load("spaceEngineSmall_000"), 4 * N)
mix += highpass(varispeed(small, 1.05 + 1.15 * rev)[:, :N], 500) * (0.07 + 0.2 * throttle + 0.06 * rev) * duck

# the rider's passes: each vehicle swells and drops in pitch as we blast past it (side: -1 left, +1 right)
whoosh = load("wind_woosh_loop")
truck = loop(load("engine_sound"), 2 * N)
cabsrc = loop(load("spaceEngineSmall_000"), 2 * N)
for kind, side, t_pass, vel in (("truck", 1, 3.2, 24.0), ("cab", 1, 5.0, 28.0), ("truck", -1, 7.9, 24.0),
                                ("cab", 1, 17.0, 28.0), ("truck", -1, 19.1, 24.0)):
    close = float(np.interp(t_pass, t, speed)) - vel            # closing speed, m/s
    dp = t - t_pass
    rp = np.sqrt((close * dp) ** 2 + 4.4 ** 2)
    dop = 340.0 / (340.0 + close * close * dp / rp)
    if kind == "truck":
        sig = lowpass(varispeed(truck, 0.55 * dop)[:, :N], 900) * 2.4
    else:
        sig = varispeed(cabsrc, 1.1 * dop)[:, :N] * 1.3
    mix += pan(sig, 0.85 * side) * (1.0 / (1.0 + (rp / 6.5) ** 2)) * duck
    wl = int(1.2 * SR)
    place(mix, pan(whoosh[:, :wl] * np.hanning(wl), 0.7 * side), t_pass - 0.45, 0.8)

# distant flying traffic: faint doppler passes of the medium engine
eng = loop(load("spaceEngine_000"), 2 * N)
for k, at in enumerate((1.6, 6.8, 9.4, 14.1, 16.9)):
    dd = t - at
    rr = np.sqrt((12 * dd) ** 2 + 30 ** 2)
    sig = varispeed(eng, 0.8 * 340 / (340 + 144 * dd / rr))[:, :N]
    mix += pan(sig, np.clip(dd, -1, 1) * (0.6 if k % 2 else -0.6)) * (1 / (1 + (rr / 28) ** 2)) * 0.5 * duck

# the city far below: the low space wind, heavily lowpassed into a drone
city = load("space-wind_0")[:, int(4 * SR):int(4 * SR) + N]
mix += lowpass(city, 380) * 0.45 * duck

# one distant roll of thunder over the skyline
th = load("rain-thunder")
lo = lowpass(th, 220)
win_e = np.convolve((lo[0] ** 2), np.ones(SR) / SR, "valid")
peak = int(np.argmax(win_e[:-6 * SR]))
clip = th[:, max(0, peak - SR):peak + 5 * SR]
fade = np.minimum(np.arange(clip.shape[1]) / (SR // 2), 1.0) * np.minimum((clip.shape[1] - np.arange(clip.shape[1])) / SR, 1.0)
roll = lowpass(clip, 700) * fade
place(mix, roll, 6.2, 0.5)

# master: cut the world at the picture cut, let only the jellyfish ring into the black
for ch in (0, 1):
    mix[ch] *= np.where(t < PICTURE, 1.0, 0.0)
mix *= np.clip((PICTURE - t) / 0.35, 0, 1)                   # a quick fade into the cut, then silence
mix *= np.minimum(t / 0.8, 1)
mix = np.tanh(mix / np.abs(mix).max() * 1.4) / math.tanh(1.4) * 0.89

path = os.path.join(ROOT, "audio", "soundtrack.wav")
with wave.open(path, "wb") as w:
    w.setnchannels(2)
    w.setsampwidth(2)
    w.setframerate(SR)
    w.writeframes((mix.T * 32767).astype("<i2").tobytes())
print("WROTE", path, f"peak={np.abs(mix).max():.2f} rms={np.sqrt((mix ** 2).mean()):.3f}")
