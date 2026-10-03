#!/usr/bin/env python3
"""Regenerate Phase 6 procedural WAV assets while preserving custom footsteps."""
from pathlib import Path
import math
import random
import struct
import wave

ROOT = Path(__file__).resolve().parents[1] / "sounds"
SAMPLE_RATE = 44100
random.seed(2606)


def click_envelope(time_s: float, attack: float = 0.002, decay: float = 18.0) -> float:
    if time_s < attack:
        return time_s / max(attack, 1e-6)
    return math.exp(-(time_s - attack) * decay)


def write_wav(relative_path: str, duration: float, sample_fn, gain: float = 0.8) -> None:
    path = ROOT / relative_path
    path.parent.mkdir(parents=True, exist_ok=True)
    frame_count = int(SAMPLE_RATE * duration)
    frames = bytearray()
    for index in range(frame_count):
        time_s = index / SAMPLE_RATE
        value = max(-1.0, min(1.0, sample_fn(time_s, index, frame_count) * gain))
        frames.extend(struct.pack("<h", int(value * 32767)))

    with wave.open(str(path), "wb") as output:
        output.setnchannels(1)
        output.setsampwidth(2)
        output.setframerate(SAMPLE_RATE)
        output.writeframes(frames)


def low_noise(seed: int, smoothing: float = 0.92):
    generator = random.Random(seed)
    state = 0.0

    def sample() -> float:
        nonlocal state
        state = smoothing * state + (1.0 - smoothing) * generator.uniform(-1.0, 1.0)
        return state

    return sample


# Three small tile-footstep fallbacks.
#
# The shipped project may contain custom user-supplied footstep clips. Preserve
# those files when present; generate these procedural fallbacks only if a
# runtime footstep file is missing.
for number, (low_frequency, high_frequency, seed) in enumerate(
    [(118, 710, 1), (132, 820, 2), (105, 640, 3)], start=1
):
    relative_path = f"footsteps/footstep_{number:02d}.wav"
    if (ROOT / relative_path).exists():
        continue

    noise = low_noise(seed, 0.75)

    def footstep(time_s, _index, _count,
                 low_frequency=low_frequency,
                 high_frequency=high_frequency,
                 noise=noise):
        return click_envelope(time_s, 0.003, 24.0) * (
            0.62 * math.sin(2.0 * math.pi * low_frequency * time_s)
            + 0.22 * math.sin(2.0 * math.pi * high_frequency * time_s)
            + 0.35 * noise()
        )

    write_wav(relative_path, 0.16, footstep, 0.72)


def short_click(frequency: float, seed: int):
    noise = low_noise(seed, 0.25)

    def sample(time_s, _index, _count):
        return click_envelope(time_s, 0.0015, 38.0) * (
            0.55 * math.sin(2.0 * math.pi * frequency * time_s) + 0.48 * noise()
        )

    return sample


write_wav("switches/light_switch.wav", 0.085, short_click(1250, 10), 0.72)
write_wav("computer/keyboard_click.wav", 0.055, short_click(1650, 11), 0.48)
write_wav("computer/mouse_click.wav", 0.070, short_click(980, 12), 0.58)
write_wav("computer/power_button.wav", 0.095, short_click(720, 13), 0.62)

# Door start cues.
for filename, base_frequency, seed, direction in [
    ("door_open.wav", 92, 20, 1),
    ("door_close.wav", 78, 21, -1),
]:
    noise = low_noise(seed, 0.985)
    duration = 0.65

    def door_sample(time_s, _index, _count,
                    base_frequency=base_frequency,
                    direction=direction,
                    noise=noise,
                    duration=duration):
        progress = time_s / duration
        envelope = math.sin(math.pi * progress) ** 0.8
        sweep = base_frequency + direction * 24.0 * progress
        latch_center = 0.08 if direction == 1 else 0.88
        latch = math.exp(-((progress - latch_center) / 0.035) ** 2) * 0.4
        latch *= math.sin(2.0 * math.pi * 540.0 * time_s)
        return envelope * (
            0.50 * math.sin(2.0 * math.pi * sweep * time_s) + 1.4 * noise()
        ) + latch

    write_wav(f"door/{filename}", duration, door_sample, 0.52)


def computer_chime(startup: bool):
    duration = 0.85
    notes = [330, 440, 660] if startup else [660, 440, 280]

    def sample(time_s, _index, _count):
        value = 0.0
        for note_index, frequency in enumerate(notes):
            note_time = time_s - (0.10 + note_index * 0.20)
            if 0.0 <= note_time < 0.26:
                envelope = math.sin(math.pi * note_time / 0.26) ** 1.5
                value += envelope * (
                    math.sin(2.0 * math.pi * frequency * note_time)
                    + 0.28 * math.sin(2.0 * math.pi * 2.0 * frequency * note_time)
                )
        return value * 0.42

    return duration, sample


for relative_path, startup in [
    ("computer/computer_startup.wav", True),
    ("computer/computer_shutdown.wav", False),
]:
    duration, function = computer_chime(startup)
    write_wav(relative_path, duration, function, 0.72)


def periodic_hum(frequencies, amplitudes, noise_amplitude, seed):
    generator = random.Random(seed)
    phases = [generator.random() * 2.0 * math.pi for _ in frequencies]
    noise_frequencies = [17, 23, 31, 43, 57]
    noise_phases = [generator.random() * 2.0 * math.pi for _ in noise_frequencies]

    def sample(time_s, _index, _count):
        value = sum(
            amplitude * math.sin(2.0 * math.pi * frequency * time_s + phase)
            for frequency, amplitude, phase in zip(frequencies, amplitudes, phases)
        )
        value += noise_amplitude * sum(
            math.sin(2.0 * math.pi * frequency * time_s + phase)
            for frequency, phase in zip(noise_frequencies, noise_phases)
        ) / len(noise_frequencies)
        return value

    return sample


write_wav(
    "computer/computer_hum_loop.wav", 2.0,
    periodic_hum([60, 90, 120], [0.32, 0.18, 0.08], 0.12, 30), 0.24,
)
write_wav(
    "projector/projector_fan_loop.wav", 2.0,
    periodic_hum([55, 110, 165], [0.20, 0.13, 0.06], 0.30, 31), 0.34,
)
write_wav(
    "ambience/classroom_ambient_loop.wav", 4.0,
    periodic_hum([30, 50, 60, 80], [0.12, 0.08, 0.10, 0.04], 0.16, 32), 0.19,
)


def projector_power(startup: bool):
    duration = 1.15 if startup else 0.85
    noise = low_noise(40 if startup else 41, 0.96)

    def sample(time_s, _index, _count):
        progress = time_s / duration
        envelope = (1.0 - math.exp(-progress * 8.0))
        envelope *= min(1.0, progress * 2.2) if startup else (1.0 - progress)
        frequency = (95.0 + 95.0 * progress) if startup else (190.0 - 110.0 * progress)
        beep_center = 0.78 if startup else 0.18
        beep = math.exp(-((progress - beep_center) / 0.08) ** 2)
        beep *= math.sin(2.0 * math.pi * (820.0 if startup else 520.0) * time_s)
        return envelope * (
            0.30 * math.sin(2.0 * math.pi * frequency * time_s) + 0.65 * noise()
        ) + 0.35 * beep

    return duration, sample


for relative_path, startup in [
    ("projector/projector_startup.wav", True),
    ("projector/projector_shutdown.wav", False),
]:
    duration, function = projector_power(startup)
    write_wav(relative_path, duration, function, 0.42)

print(f"Prepared {len(list(ROOT.rglob('*.wav')))} WAV assets in {ROOT}; existing custom footsteps were preserved.")
