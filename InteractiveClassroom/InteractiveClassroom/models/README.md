# Phase 7 model assets

## Included and actively integrated

These low-poly assets were created for this project, are included in the ZIP,
and are loaded through Assimp once per shared resource:

- `furniture/student_desk/student_desk.obj` — rendered as 20 instances
- `furniture/chair/chair.obj` — rendered as 40 instances
- `computer/monitor/monitor.obj` — shared by all 41 `Computer` objects

The `.mtl` files contain scalar material values. Gameplay ownership, collision,
computer state, monitor screen emission, LEDs, audio positions, and interaction
remain in the existing classroom classes rather than in `Model`.

## Optional future asset paths

The current project intentionally keeps procedural geometry for objects not
listed above. Legally obtained replacements may later be registered at these
paths without changing classroom world coordinates:

- `furniture/teacher_desk/teacher_desk.glb`
- `computer/keyboard/keyboard.glb`
- `computer/mouse/mouse.glb`
- `computer/cpu/cpu.glb`
- `classroom/door/door.glb`
- `classroom/projector/projector.glb`
- `classroom/light_switch/light_switch.glb`

No file at an optional path is claimed as integrated in this delivery.

If an actively registered model is missing or Assimp cannot read it, one
warning is printed and the existing procedural primitive remains visible.
Collision remains a simplified AABB and never uses triangle-mesh collision.
