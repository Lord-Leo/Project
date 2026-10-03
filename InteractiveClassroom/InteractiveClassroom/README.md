# Interactive Computer Classroom — Final Phase 9 Project

This delivery continues from the corrected Phase 6 custom-footstep project and
adds Phase 7, then Phase 8, then Phase 9 without rebuilding or simplifying the
existing classroom. The room remains 26 m × 20 m × 4 m with 20 student tables,
40 student chairs, 40 student computers, one teacher computer, the existing
door/windows/whiteboard/projector, collision, interactions, shadows and
centralized miniaudio system.

## Phase 7 — Assimp models

Assimp **5.4.3** is pinned through CMake FetchContent. Tests, tools, samples,
exporters and unused importers are disabled; OBJ, FBX and glTF/GLB importers are
enabled. It is built statically, so the normal Windows build requires no Assimp
DLL.

Reusable classes:

- `Model`: Assimp import, node traversal, materials, local bounds and shared draw
- `ModelMesh`: VAO/VBO/EBO, tangent data, safe GL cleanup and instanced draws
- `TextureManager`: canonical cache, embedded textures, sRGB/data formats and fallbacks
- `Material`: scalar and texture-driven legacy/PBR material state

Self-created models actually included and integrated:

```text
models/furniture/student_desk/student_desk.obj
models/furniture/student_desk/student_desk.mtl
models/furniture/chair/chair.obj
models/furniture/chair/chair.mtl
models/computer/monitor/monitor.obj
models/computer/monitor/monitor.mtl
```

The desk model is drawn in 20 instances, the chair in 40 instances, and one
monitor resource is shared by all 41 computers. Existing AABB collision and all
state/interaction/audio ownership remain in the original classroom classes.
Door, projector, keyboard, mouse, CPU and switches remain procedural because no
external redistributable model for them is included.

## Phase 8 — PBR materials and lighting

`shaders/pbr.vert` and `shaders/pbr.frag` implement a linear-space
Cook–Torrance metallic-roughness workflow with GGX distribution, Smith geometry,
Schlick Fresnel, energy-conserving diffuse/specular, tangent-space normal maps,
scalar fallbacks, point lights, directional light, projector spotlight,
directional PCF shadows and emissive output.

Included self-generated maps:

```text
textures/materials/floor_tile_base.png
textures/materials/floor_tile_normal.png
textures/materials/floor_tile_roughness.png
textures/fallback/white_base_colour.png
textures/fallback/black_metallic.png
textures/fallback/medium_roughness.png
textures/fallback/flat_normal.png
textures/fallback/white_ao.png
textures/fallback/black_emissive.png
```

Colour maps use sRGB internal formats. Normal, metallic, roughness and AO data
stay linear. F11 switches between PBR and the preserved legacy material path.

## Phase 9 — HDR, bloom and exposure

The final renderer uses an optional `GL_RGBA16F` HDR pipeline:

1. directional shadow depth pass
2. main scene into normal or 4× multisampled HDR scene/bright attachments
3. explicit MSAA resolve to normal 2D HDR textures
4. bounded ping-pong Gaussian bloom blur
5. ACES, Reinhard or None/debug tone mapping
6. one final optional gamma conversion
7. HUD on the default framebuffer

Framebuffer completeness is checked. Failure disables HDR and safely uses the
existing non-HDR path. Resizing recreates size-dependent targets only when the
window dimensions change.

Defaults: ACES, exposure 1.0, bloom threshold 1.15, bloom strength 0.10 and 8
blur passes.

## Complete controls

| Key | Action |
|---|---|
| W / A / S / D | Move |
| Mouse | Look |
| Left Shift | Run |
| E | Interact with the selected object or computer part |
| Esc | Release/capture mouse |
| F1 or H | Shadows on/off |
| F2 | 4× MSAA on/off |
| F3 | Gamma comparison on/off |
| F4 | Power on all Off computers with limited bulk audio |
| F5 | Request shutdown for all computers with limited bulk audio |
| F6 | Force all computers Off and stop computer hum loops |
| F7 or O | Toggle projector through its state machine |
| F8 or P | Cycle projection mode only while Active |
| F9 | Silent emergency/debug projector reset; immediately stops projector audio |
| F10 | Master audio mute/unmute |
| Minus / numpad minus | Master volume −10% |
| Equal/Plus / numpad plus | Master volume +10% |
| M | Mute only classroom ambience; machinery remains audible |
| L | Silent debug toggle for the shared classroom light circuit |
| F11 | PBR / legacy material rendering |
| F12 | HDR / valid fallback path |
| B | Bloom on/off |
| T | Cycle Reinhard → ACES → None/debug |
| [ | Exposure −0.1, clamped to 0.1 |
| ] | Exposure +0.1, clamped to 5.0 |

The HUD/window title shows shadows, MSAA, gamma, PBR, HDR, bloom, exposure,
tone mapping, FPS, projector state/mode and audio status.

## Visual Studio 2022 build

Required:

- Windows 10/11
- Visual Studio 2022
- **Desktop development with C++** workload
- **C++ CMake tools for Windows** component
- Git and internet access during the first configure
- GPU/driver with OpenGL 3.3 Core support

Steps:

1. Extract the ZIP to a writable folder. Do not run from inside the archive.
2. Open Visual Studio 2022.
3. Choose **File → Open → Folder** and select `InteractiveClassroom`.
4. Wait for CMake to fetch pinned GLFW 3.4, GLM 1.0.1, stb commit
   `013ac3beddff3dbffafd5177e7972067cd2b5083`, miniaudio 0.11.25 and Assimp
   v5.4.3.
5. Select `x64-Debug`, then **Build → Build All**.
6. Select `InteractiveClassroom.exe` and press **Ctrl+F5**.
7. Repeat with `x64-Release` for the final performance test.
8. Click inside the game window to capture the mouse.

Command-line alternative from a VS 2022 Developer Command Prompt:

```bat
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
cmake --build build --config Release
build\Release\InteractiveClassroom.exe
```

CMake copies `shaders`, `textures`, `models` and `sounds` next to the built
executable and sets the Visual Studio debugger working directory to that folder.
If FetchContent configuration is interrupted, close Visual Studio, delete its
generated `out/build` or selected CMake cache directory, confirm Git/network
access, and configure again.

## Runtime libraries and DLLs

- Assimp is intentionally static: no Assimp DLL should be required.
- GLAD is vendored and static.
- miniaudio is compiled into the program; no audio DLL is required.
- GLFW follows its CMake target configuration and is linked by CMake.
- A defensive CMake copy command exists only if a downstream configuration
  deliberately replaces Assimp with a shared library.
- The normal Microsoft Visual C++ runtime requirements follow the selected
  Visual Studio runtime/toolchain configuration.

## Model fallback behaviour

An actively registered model path is loaded once. Missing/unreadable files or
models with no renderable mesh print one warning and retain the original
procedural primitive. No object becomes intentionally invisible unless its
replacement model loaded successfully. Collision never depends on model
triangles.

Optional future paths are documented in `models/README.md`; they are not
claimed as integrated in this ZIP.

## Texture fallback behaviour

`TextureManager` caches by canonical path, semantic and flip setting. Missing
or unreadable maps warn once and use:

- white base colour
- black metallic
- medium roughness
- flat normal
- white AO
- black emissive

This preserves visible scalar materials instead of producing black geometry.

## Recommended graphics settings

For a normal discrete GPU at 1920×1080:

- PBR ON
- HDR ON
- ACES
- exposure 1.0
- bloom ON
- 4× MSAA ON
- shadows ON
- gamma ON

For lower performance, disable bloom first, then MSAA, then shadows. Keep PBR
and HDR enabled if the GPU supports them comfortably. F12 gives a valid direct
fallback if HDR framebuffer creation fails.

## Validation

Run all deterministic project checks from the project root:

```bat
python tools\validate_phase79.py --phase 7
python tools\validate_phase79.py --phase 8
python tools\validate_phase79.py --phase 9
python tools\validate_phase79.py --phase regression
python tools\validate_phase79.py --phase all
```

Current packaged result: 131 grouped checks passed. Detailed records are in:

- `PHASE7_VALIDATION.txt`
- `PHASE8_VALIDATION.txt`
- `PHASE9_VALIDATION.txt`
- `PHASE79_BUILD_VALIDATION.txt`
- `FINAL_REGRESSION.txt`

## Known limitations

- Only the included desk, chair and monitor are imported; other visuals remain
  procedural.
- OBJ assets are included. FBX and glTF/GLB importers are enabled, but no FBX,
  glTF or GLB asset is included or claimed as visually integrated.
- PBR uses direct lights and an ambient approximation, not image-based lighting.
- Windows use the established simplified transparent path rather than full
  physically based refraction.
- Anisotropic filtering is not forced; normal mipmapped filtering is used.
- No skeletal animation, point-light cubemap shadows, screen-space effects,
  deferred renderer, volumetric ray marching or automatic eye adaptation is
  included.
- Model hot reload is not implemented.
- Actual 1080p performance is hardware/driver dependent.
- The supplied Linux container could not execute Visual Studio 2022, real
  Windows audio, or a normal GPU OpenGL driver. Debug/Release target builds and
  one-frame runs were validated with dependency-compatible deterministic
  harnesses; the real Windows/GPU checklist in `FINAL_REGRESSION.txt` must be
  completed on the target machine.
