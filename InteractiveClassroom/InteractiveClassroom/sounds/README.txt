PHASE 6 SOUND ASSETS
====================

The three files under `footsteps/` are edited, mono 44.1 kHz WAV excerpts made
from the user-provided `footsteps-male-362053(1).mp3` recording. They were
trimmed to individual strikes, edge-faded, and normalized for in-game playback.
The source MP3 is not required at runtime and is not included in this project.

All other WAV files were generated specifically for this project by
`tools/generate_phase6_sounds.py` as original procedural sounds.

CATEGORY MAP
- Ambient: ambience/classroom_ambient_loop.wav
- Machinery: projector/projector_fan_loop.wav
- Machinery: computer/computer_hum_loop.wav
- Footsteps: footsteps/footstep_01.wav, footstep_02.wav, footstep_03.wav
- Effects: physical light switch, door, computer/projector startup and shutdown,
  keyboard, mouse, and power-button sounds

M mutes only the Ambient category. Projector fan and computer hum remain under
the separate Machinery category. F10 master mute silences every category.
Retained loops continue silently under master mute and resume smoothly; new
one-shot playback is rejected while muted.

Expected runtime paths:
- footsteps/footstep_01.wav
- footsteps/footstep_02.wav
- footsteps/footstep_03.wav
- switches/light_switch.wav
- door/door_open.wav
- door/door_close.wav
- computer/computer_startup.wav
- computer/computer_shutdown.wav
- computer/computer_hum_loop.wav
- computer/keyboard_click.wav
- computer/mouse_click.wav
- computer/power_button.wav
- projector/projector_startup.wav
- projector/projector_shutdown.wav
- projector/projector_fan_loop.wav
- ambience/classroom_ambient_loop.wav

The program remains fully usable if this folder or any individual WAV is
missing. AudioManager prints one warning for each unavailable file, skips only
that sound, and continues rendering and interaction without crashing.

CUSTOM FOOTSTEP SOURCE
- Runtime files: footsteps/footstep_01.wav through footstep_03.wav
- Derived from the audio file supplied by the project user
- Processing: mono conversion, 44.1 kHz PCM, transient trimming, short fades,
  and peak normalization
- `tools/generate_phase6_sounds.py` preserves these custom files when present
  and creates procedural fallback footsteps only when they are missing.
