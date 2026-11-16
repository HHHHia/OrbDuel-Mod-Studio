"""Generate OrbDuel's built-in 8-bit SFX (CC0, self-made, stdlib only).

    python tools/make_sfx.py [--out content/audio/orbduel]

Writes mono 16-bit 44.1 kHz PCM WAV, which SDL_LoadWAV can load as-is.
Every sound is a deterministic function of its name, so re-runs are stable.
"""
from __future__ import annotations

import argparse
import math
import os
import random
import wave

RATE = 44100


def _env(i: int, n: int, decay: float, attack: int = 60) -> float:
    """Linear attack, exponential decay."""
    if i < attack:
        return i / attack
    return math.exp(-decay * (i - attack) / max(n - attack, 1))


def _square(phase: float) -> float:
    return 1.0 if (phase % 1.0) < 0.5 else -1.0


def _saw(phase: float) -> float:
    return 2.0 * (phase % 1.0) - 1.0


def _sweep(f0: float, f1: float, n: int, decay: float, shape: str = "square") -> list[float]:
    out, phase = [], 0.0
    for i in range(n):
        t = i / max(n - 1, 1)
        freq = f0 + (f1 - f0) * t
        phase += freq / RATE
        tone = _square(phase) if shape == "square" else _saw(phase)
        out.append(tone * _env(i, n, decay))
    return out


def _noise(n: int, decay: float, seed: int, lowpass: float = 0.35) -> list[float]:
    rnd = random.Random(seed)
    out, prev = [], 0.0
    for i in range(n):
        white = rnd.uniform(-1.0, 1.0)
        prev = prev + lowpass * (white - prev)
        out.append(prev * _env(i, n, decay))
    return out


def _mix(*tracks: list[float]) -> list[float]:
    n = max(len(t) for t in tracks)
    return [sum(t[i] if i < len(t) else 0.0 for t in tracks) for i in range(n)]


def _seq(notes: list[tuple[float, float]], shape: str = "square", decay: float = 6.0) -> list[float]:
    """notes = [(freq_hz, seconds), ...] played back to back."""
    out: list[float] = []
    for freq, secs in notes:
        out.extend(_sweep(freq, freq, int(RATE * secs), decay, shape))
    return out


def _normalize(samples: list[float], peak: float = 0.85) -> list[float]:
    top = max((abs(s) for s in samples), default=0.0)
    if top < 1e-6:
        return samples
    gain = peak / top
    return [s * gain for s in samples]


def build() -> dict[str, list[float]]:
    snd: dict[str, list[float]] = {}

    # combat
    snd["hit_light"] = _normalize(_mix(
        _noise(int(RATE * 0.07), 42, 11, 0.55),
        _sweep(420, 180, int(RATE * 0.07), 30),
    ))
    snd["hit_heavy"] = _normalize(_mix(
        _noise(int(RATE * 0.16), 24, 23, 0.45),
        _sweep(240, 70, int(RATE * 0.16), 16),
        _sweep(90, 45, int(RATE * 0.16), 8, "saw"),
    ))
    snd["wall_bounce"] = _normalize(_mix(
        _noise(int(RATE * 0.05), 60, 31, 0.7),
        _sweep(900, 520, int(RATE * 0.05), 45),
    ))
    snd["ko"] = _normalize(_mix(
        _noise(int(RATE * 0.35), 12, 47, 0.3),
        _sweep(300, 40, int(RATE * 0.35), 7, "saw"),
    ), 0.9)

    # round flow
    snd["round_start"] = _normalize(_seq([(392, 0.09), (523, 0.09), (659, 0.16)], "square", 5))
    snd["round_win"] = _normalize(_seq([(523, 0.10), (659, 0.10), (784, 0.10), (1047, 0.26)], "square", 4))
    snd["round_lose"] = _normalize(_seq([(392, 0.12), (330, 0.12), (262, 0.30)], "saw", 5))
    snd["sudden_death"] = _normalize(_mix(
        _seq([(880, 0.08), (660, 0.08), (880, 0.08), (660, 0.08)], "square", 6),
        _noise(int(RATE * 0.32), 10, 59, 0.2),
    ))

    # UI
    snd["ui_click"] = _normalize(_sweep(1200, 900, int(RATE * 0.035), 55))
    snd["ui_hover"] = _normalize(_sweep(700, 700, int(RATE * 0.02), 70))
    snd["ui_deny"] = _normalize(_seq([(180, 0.07), (140, 0.09)], "square", 30))

    # match feedback
    snd["heal"] = _normalize(_seq([(523, 0.06), (784, 0.06), (1047, 0.12)], "saw", 6))
    snd["shield"] = _normalize(_mix(
        _sweep(300, 1100, int(RATE * 0.18), 12),
        _noise(int(RATE * 0.18), 18, 71, 0.2),
    ))
    snd["charge"] = _normalize(_sweep(160, 900, int(RATE * 0.30), 4, "saw"), 0.75)

    return snd


def write_wav(path: str, samples: list[float]) -> None:
    payload = b"".join(
        int(max(-1.0, min(1.0, s)) * 32767).to_bytes(2, "little", signed=True) for s in samples
    )
    with wave.open(path, "wb") as fh:
        fh.setnchannels(1)
        fh.setsampwidth(2)
        fh.setframerate(RATE)
        fh.writeframes(payload)


def main() -> None:
    here = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(here, "content", "audio", "orbduel"))
    args = ap.parse_args()

    os.makedirs(args.out, exist_ok=True)
    for name, samples in build().items():
        path = os.path.join(args.out, name + ".wav")
        write_wav(path, samples)
        print(f"{name:14s} {len(samples)/RATE:5.3f}s  {os.path.getsize(path)//1024:3d} KB")


if __name__ == "__main__":
    main()
