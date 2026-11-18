"""Synthesized soundtrack for the neon ride (20 s + black tail), timed to build_ride.py.

  /Applications/Blender.app/Contents/Resources/5.3/python/bin/python3.13 scripts/make_audio.py
Writes audio/soundtrack.wav (48 kHz, 16-bit stereo). Everything is generated; no samples or downloads.
"""
import math, os, wave
import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SR = 48000
PICTURE = 20.0                      # cut to black here
TAIL = 1.0
DUR = PICTURE + TAIL
N = int(SR * DUR)
t = np.arange(N) / SR
rng = np.random.default_rng(5)
SPEED = 36.0
GLASS_T = 372.0 / SPEED             # glass section begins (~10.3 s)
OVERTAKE_T = 4.0
JELLY_T0, JELLY_T1 = 12.0, 19.3     # jellyfish rises, then engulfs


def smoothstep(x):
    x = np.clip(x, 0, 1)
    return x * x * (3 - 2 * x)


def spectrum_filter(x, shape):
    X = np.fft.rfft(x)
    return np.fft.irfft(X * shape(np.fft.rfftfreq(len(x), 1 / SR)), len(x))


def lowpass(x, fc, order=2):
    return spectrum_filter(x, lambda f: 1 / (1 + (f / fc) ** (2 * order)) ** 0.5)


def highpass(x, fc, order=2):
    return spectrum_filter(x, lambda f: 1 - 1 / (1 + (f / max(fc, 1)) ** (2 * order)) ** 0.5)


def bandpass(x, lo, hi):
    return spectrum_filter(x, lambda f: ((f > lo) & (f < hi)).astype(float))


def noise(n=N):
    return rng.standard_normal(n)


live = (t < PICTURE).astype(float)
glass = smoothstep((t - GLASS_T) / 0.6)              # inside the glass section the rain drums and the wind softens
out = np.zeros((2, N))

# --- wind roar around the helmet: broadband, gusting
for ch in (0, 1):
    w = lowpass(noise(), 900) + 0.35 * bandpass(noise(), 900, 3500)
    gust = 0.8 + 0.2 * np.sin(2 * np.pi * 0.37 * t + ch) + 0.1 * np.sin(2 * np.pi * 1.3 * t + 2 * ch)
    out[ch] += w / np.abs(w).max() * 0.22 * gust * (1 - 0.45 * glass)

# --- rain: fine hiss everywhere, plus drumming on the glass walls in the second half
for ch in (0, 1):
    hiss = highpass(noise(), 3500)
    out[ch] += hiss / np.abs(hiss).max() * 0.07
    drops = np.zeros(N)
    idx = rng.integers(0, N, int(DUR * 900))
    drops[idx] = rng.uniform(0.2, 1.0, len(idx))
    tick = np.exp(-np.arange(int(0.004 * SR)) / (0.0008 * SR))
    patter = np.convolve(drops, tick, "same")
    patter = bandpass(patter, 1200, 7000)
    out[ch] += patter / np.abs(patter).max() * 0.16 * glass

# --- tyres on the wet deck: a low hiss with a slight rhythmic seam thump
tyre = lowpass(noise(), 1800) * (1 + 0.15 * np.sin(2 * np.pi * (SPEED / 6.0) * t))
out += tyre / np.abs(tyre).max() * 0.08

# --- electric drive: two clean tones and a soft harmonic, gently rising, wavering with the lean
lean = smoothstep((t - 8.5) / 2.0)
f0 = 182 + 6 * np.sin(2 * np.pi * 0.2 * t) + 10 * lean
ph = 2 * np.pi * np.cumsum(f0) / SR
motor = 0.55 * np.sin(ph) + 0.3 * np.sin(2.003 * ph) + 0.12 * np.sin(3.01 * ph + 0.4)
motor *= 0.9 + 0.1 * np.sin(2 * np.pi * 23 * t)
out[0] += motor * 0.05
out[1] += motor * 0.05

# --- the overtake: a doppler whoosh sweeping from left-behind to right-ahead
d = t - OVERTAKE_T
r = np.sqrt((60.0 - SPEED) ** 2 * d ** 2 + 9.2 ** 2 + 1.6 ** 2)      # distance to the car
closing = -(60.0 - SPEED) ** 2 * d / r
pitch = 340 / (340 - closing)                                       # doppler factor
cf = 210 * pitch
car = np.sin(2 * np.pi * np.cumsum(cf) / SR) + 0.5 * np.sin(2 * np.pi * np.cumsum(cf * 2.51) / SR)
car += 1.5 * lowpass(noise(), 1500) * (1 / (1 + np.exp(-(d) * 3)))  # air blast after it passes
amp = 1.0 / (1 + (r / 10.0) ** 2)
pan = np.clip(d * 1.2, -1, 1) * 0.5 + 0.5                          # behind-left to ahead-right
out[0] += car * amp * 0.9 * (1 - pan)
out[1] += car * amp * 0.9 * pan

# --- distant flying traffic: occasional faint passes
for at in (1.6, 6.8, 9.4, 14.1, 16.9):
    dd = t - at
    rr = np.sqrt((30 * dd) ** 2 + 30 ** 2)
    sig = np.sin(2 * np.pi * np.cumsum(140 * 340 / (340 + 30 ** 2 * dd / rr)) / SR)
    out += sig * (1 / (1 + (rr / 25) ** 2)) * 0.25

# --- the city far below: a deep, slow drone
city = sum(a * np.sin(2 * np.pi * f * t + p) for f, a, p in ((41.2, 1.0, 0), (61.7, 0.5, 1), (82.4, 0.3, 2)))
out += city * 0.05 * (0.8 + 0.2 * np.sin(2 * np.pi * 0.05 * t))

# --- the jellyfish: an airy, choir-like chord that swells as it rises, then floods everything
rise = smoothstep((t - JELLY_T0) / (JELLY_T1 - JELLY_T0))
engulf = smoothstep((t - (JELLY_T1 - 0.6)) / 0.7)
chord = np.zeros(N)
for f, a in ((220.0, 1.0), (277.2, 0.8), (329.6, 0.7), (440.0, 0.5), (554.4, 0.35), (659.3, 0.25)):
    for det in (-0.6, 0.0, 0.7):
        chord += a * np.sin(2 * np.pi * (f + det) * t + rng.uniform(0, 6.28)) * (0.6 + 0.4 * np.sin(2 * np.pi * 0.23 * t + f))
breath = bandpass(noise(), 600, 4000) * 0.25
choir = lowpass(chord / 6.0 + breath, 3000)
pulse = 0.75 + 0.25 * np.sin(2 * np.pi * t / 3.2)
g = (0.02 + 0.12 * rise ** 1.6 + 0.25 * engulf) * pulse
out[0] += choir * g
out[1] += np.roll(choir, 480) * g
sub = np.sin(2 * np.pi * 36.7 * t) * (0.15 * rise + 0.35 * engulf)
out += sub

# --- the world falls away as it engulfs you: wind, rain and motor duck under the swell
duck = 1 - 0.75 * engulf

# --- master
for ch in (0, 1):
    out[ch] = out[ch] * (0.25 + 0.75 * duck) * live
# a soft breath of reverb tail into the black
tail = np.exp(-np.clip(t - PICTURE, 0, None) / 0.35) * (t >= PICTURE)
ring = choir * 0.25 * tail
out[0] += ring
out[1] += np.roll(ring, 480)
out *= np.minimum(t / 0.8, 1)
out = np.tanh(out / np.abs(out).max() * 1.5) / math.tanh(1.5) * 0.89

os.makedirs(os.path.join(ROOT, "audio"), exist_ok=True)
path = os.path.join(ROOT, "audio", "soundtrack.wav")
with wave.open(path, "wb") as w:
    w.setnchannels(2)
    w.setsampwidth(2)
    w.setframerate(SR)
    w.writeframes((out.T * 32767).astype("<i2").tobytes())
print("WROTE", path, f"{DUR:.1f}s peak={np.abs(out).max():.2f} rms={np.sqrt((out ** 2).mean()):.3f}")
