# Generates the menu sounds (original, synthesised): small wooden bongo taps, to suit DK64.
#   ui_move.wav   - moving between items: a light, high bongo tap
#   ui_select.wav - selecting / applying: two quick bongo hits, low then high
#   ui_back.wav   - backing out: one lower, softer hit
# Usage: python3 gen_ui_sounds.py <output dir>
import math, random, struct, sys, wave, os
RATE = 22050
def bongo(freq, dur, vol, drop=0.18, noise=0.25):
    out = []
    n = int(RATE * dur)
    phase = 0.0
    random.seed(int(freq))
    for i in range(n):
        t = i / RATE
        f = freq * (1.0 + drop * math.exp(-t * 60))          # pitch falls quickly after the hit
        phase += 2 * math.pi * f / RATE
        body = math.sin(phase) + 0.35 * math.sin(phase * 2.71)   # skin + a wooden overtone
        env = math.exp(-t * (38 + freq / 40))               # short, damped
        click = (random.random() * 2 - 1) * math.exp(-t * 900) * noise
        out.append(vol * (body * env + click))
    return out
def mix(parts, total):
    buf = [0.0] * int(RATE * total)
    for start, samples in parts:
        s = int(RATE * start)
        for i, v in enumerate(samples):
            if s + i < len(buf):
                buf[s + i] += v
    return buf
def write(path, samples):
    peak = max(1e-6, max(abs(v) for v in samples))
    scale = min(1.0, 0.95 / peak)
    with wave.open(path, 'wb') as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(RATE)
        w.writeframes(b''.join(struct.pack('<h', int(max(-1, min(1, v * scale)) * 32767)) for v in samples))
out = sys.argv[1]
os.makedirs(out, exist_ok=True)
write(os.path.join(out, 'ui_move.wav'), mix([(0, bongo(520, 0.09, 0.55))], 0.1))
write(os.path.join(out, 'ui_select.wav'), mix([(0, bongo(300, 0.16, 0.8)), (0.075, bongo(460, 0.16, 0.7))], 0.25))
write(os.path.join(out, 'ui_back.wav'), mix([(0, bongo(230, 0.16, 0.7, drop=0.1))], 0.18))
