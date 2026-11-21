#!/usr/bin/env python3
"""Makes the mod's own sounds (sounds/*.wav), all synthesised here: no recordings, nobody to credit.

  python tools/gen_sounds.py      -> sounds/speed_warn.wav, speed_alarm.wav, bus_bell.wav, bus_gong.wav,
                                     ride_request.wav, payout.wav (44.1 kHz, mono, 16 bit)

The door sounds (door_open.wav, door_close.wav) are recordings (Sounds From The Lab) and are not made here.
Every sound is a sum of decaying sine partials (a struck bar, a bell, a chime), so it stays clean and short,
like the game's own menu sounds. Same input, same bytes: no randomness.
"""
import math, os, struct

RATE = 44100
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def tone(buf, at, freq, amp, decay, partials=((1.0, 1.0),), attack=0.004):
    """a struck tone at 'at' seconds: partials (ratio, relative amp), each decays exp(-t/(decay/ratio^0.5))"""
    start = int(at * RATE)
    length = int(min(len(buf) - start, decay * 7 * RATE))
    for ratio, rel in partials:
        f = freq * ratio
        if f >= RATE / 2 - 500:
            continue
        tau = decay / math.sqrt(ratio)
        w = 2 * math.pi * f / RATE
        for i in range(max(0, length)):
            t = i / RATE
            env = (t / attack if t < attack else 1.0) * math.exp(-t / tau)
            buf[start + i] += amp * rel * env * math.sin(w * i)


def finish(buf, peak_db, fade=0.03):
    """normalise to peak_db (dBFS), fade the tail out, 16-bit samples"""
    peak = max(abs(s) for s in buf) or 1.0
    gain = 10 ** (peak_db / 20) / peak
    n = int(fade * RATE)
    out = []
    for i, s in enumerate(buf):
        if i >= len(buf) - n:
            s *= (len(buf) - i) / n
        out.append(max(-32767, min(32767, int(round(s * gain * 32767)))))
    return out


def write(name, samples):
    data = struct.pack("<%dh" % len(samples), *samples)
    head = b"RIFF" + struct.pack("<I", 36 + len(data)) + b"WAVE"
    head += b"fmt " + struct.pack("<IHHIIHH", 16, 1, 1, RATE, RATE * 2, 2, 16)
    head += b"data" + struct.pack("<I", len(data))
    path = os.path.join(ROOT, "sounds", name + ".wav")
    with open(path, "wb") as f:
        f.write(head + data)
    print("%-18s %.2f s" % (name + ".wav", len(samples) / RATE))


def empty(seconds):
    return [0.0] * int(seconds * RATE)


# a soft electronic chime (a car's dashboard): the note, a weak octave, a hint of the twelfth
CHIME = ((1.0, 1.0), (2.0, 0.18), (3.0, 0.05))
# a small struck bell (a bus's stop bell): inharmonic partials, as a real bell has
BELL = ((1.0, 1.0), (2.32, 0.45), (3.89, 0.22), (5.38, 0.10), (7.02, 0.04))
# a vibraphone-like bar (the announcement gong)
BAR = ((1.0, 1.0), (3.98, 0.20), (9.0, 0.03))
# a marimba-like bar (the phone's ping)
MARIMBA = ((1.0, 1.0), (3.93, 0.30), (10.2, 0.05))


def speed_warn():
    """yellow: one soft "bong", a little too fast (plays once)"""
    b = empty(0.75)
    tone(b, 0.0, 1046.5, 1.0, 0.16, CHIME, attack=0.006)   # C6
    tone(b, 0.0, 523.25, 0.35, 0.20, CHIME, attack=0.010)  # C5 underneath: rounder
    return finish(b, -9.0)


def speed_alarm():
    """red: "ding-ding", repeated by the mod every second while too fast"""
    b = empty(0.95)
    for at in (0.0, 0.30):
        tone(b, at, 1568.0, 1.0, 0.10, CHIME, attack=0.003)  # G6
        tone(b, at, 1975.5, 0.30, 0.07, CHIME, attack=0.003) # B6 on top: brighter, more urgent
    return finish(b, -4.0)


def bus_bell():
    """the stop button: a short bright bell strike"""
    b = empty(1.0)
    tone(b, 0.0, 1318.5, 1.0, 0.22, BELL, attack=0.002)  # E6
    return finish(b, -6.0)


def bus_gong():
    """before the stop announcement: three falling notes, like a station's PA (G5, E5, C5)"""
    b = empty(1.9)
    for at, f in ((0.0, 783.99), (0.32, 659.25), (0.64, 523.25)):
        tone(b, at, f, 1.0, 0.42, BAR, attack=0.008)
    return finish(b, -6.0)


def ride_request():
    """a ride request on the phone: two quick rising notes (E6, B6)"""
    b = empty(0.65)
    tone(b, 0.0, 1318.5, 0.9, 0.09, MARIMBA, attack=0.003)
    tone(b, 0.11, 1975.5, 1.0, 0.13, MARIMBA, attack=0.003)
    return finish(b, -6.0)


def payout():
    """paid: a quick rising arpeggio with a little shimmer (C6 E6 G6 C7)"""
    b = empty(1.1)
    for k, (at, f) in enumerate(((0.0, 1046.5), (0.06, 1318.5), (0.12, 1568.0), (0.18, 2093.0))):
        tone(b, at, f, 0.8 + 0.1 * k, 0.12 if k < 3 else 0.26, BELL[:3], attack=0.002)
    tone(b, 0.18, 4186.0, 0.12, 0.30, ((1.0, 1.0),), attack=0.004)  # the shimmer
    return finish(b, -6.0)


def main():
    os.makedirs(os.path.join(ROOT, "sounds"), exist_ok=True)
    for fn in (speed_warn, speed_alarm, bus_bell, bus_gong, ride_request, payout):
        write(fn.__name__, fn())


if __name__ == "__main__":
    main()
