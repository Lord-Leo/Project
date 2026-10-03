#include "Classroom.h"
#include <glad/glad.h>
#include <glm/gtc/constants.hpp>
#include <algorithm>
#include <cmath>
#include "Material.h"
#include <glm/gtc/matrix_transform.hpp>

// ============================================================================
// LightSwitch
// ============================================================================
LightSwitch::LightSwitch(const glm::vec3& wallPosition, float facingYRadians, LightCircuit* circuit)
    : m_circuit(circuit)
{
    m_plate.position = wallPosition;
    m_plate.scale = glm::vec3(0.15f, 0.22f, 0.035f);
    m_plate.color = glm::vec3(0.90f, 0.90f, 0.88f);
    m_plate.rotationY = facingYRadians;
}

void LightSwitch::Draw(const Shader& shader, const Mesh& unitBox) const
{
    const bool isOn = m_circuit && m_circuit->IsOn();
    shader.setBool("useInstancing", false);

    // White wall plate like a real household/classroom rocker switch.
    shader.setMat4("model", m_plate.GetModelMatrix());
    Material plateMaterial = Material::FromLegacy(
        glm::vec3(0.90f, 0.90f, 0.88f), glm::vec3(0.0f),
        18.0f, glm::vec3(0.22f));
    plateMaterial.metallic = 0.0f;
    plateMaterial.roughness = 0.56f;
    plateMaterial.Apply(shader);
    unitBox.Draw();

    // Raised central rocker. Keep it white in both states; the small vertical
    // offset gives a physical pressed/up state without an unrealistic glow.
    BoxInstance rocker = m_plate;
    rocker.scale = glm::vec3(0.062f, 0.115f, 0.030f);
    rocker.position.y += isOn ? 0.016f : -0.016f;
    rocker.position.z += 0.036f;
    shader.setMat4("model", rocker.GetModelMatrix());
    Material rockerMaterial = Material::FromLegacy(
        glm::vec3(0.86f, 0.86f, 0.84f), glm::vec3(0.0f),
        22.0f, glm::vec3(0.25f));
    rockerMaterial.metallic = 0.0f;
    rockerMaterial.roughness = 0.48f;
    rockerMaterial.Apply(shader);
    unitBox.Draw();

    // Two small dark screw heads, matching the supplied switch reference.
    for (float xOffset : {-0.050f, 0.050f})
    {
        BoxInstance screw = m_plate;
        screw.position.x += xOffset;
        screw.position.z += 0.040f;
        screw.scale = glm::vec3(0.014f, 0.014f, 0.010f);
        shader.setMat4("model", screw.GetModelMatrix());
        Material screwMaterial = Material::FromLegacy(
            glm::vec3(0.30f, 0.30f, 0.30f), glm::vec3(0.0f),
            36.0f, glm::vec3(0.50f));
        screwMaterial.metallic = 0.55f;
        screwMaterial.roughness = 0.30f;
        screwMaterial.Apply(shader);
        unitBox.Draw();
    }
}

glm::vec3 LightSwitch::GetAABBMin() const
{
    // Keep the visible plate unchanged, but make the interaction target a
    // little more forgiving. The switch is thin and mounted flush to a wall,
    // so a pixel-perfect AABB made it unnecessarily easy to miss with the
    // centre-screen ray.
    return m_plate.GetWorldMin() - glm::vec3(0.07f, 0.06f, 0.05f);
}

glm::vec3 LightSwitch::GetAABBMax() const
{
    return m_plate.GetWorldMax() + glm::vec3(0.07f, 0.06f, 0.05f);
}

std::string LightSwitch::GetPrompt() const
{
    return m_circuit->IsOn() ? "Press E to Turn Off Lights" : "Press E to Turn On Lights";
}

void LightSwitch::Interact()
{
    if (m_circuit)
        m_circuit->Toggle();
}

bool LightSwitch::InteractWithResult()
{
    if (!m_circuit)
        return false;

    m_circuit->Toggle();
    return true;
}

// ============================================================================
// Classroom
// ============================================================================
Classroom::Classroom()
{
    m_unitBox = Mesh::CreateUnitBox();
    m_textureManager = new TextureManager();
    LoadSharedAssets();
    BuildRoom();

    // Included self-generated floor maps exercise the complete colour/data
    // texture path without relying on copyrighted assets.
    m_floor.useTexture = true;
    m_floor.textureID = m_textureManager->Load(
        "textures/materials/floor_tile_base.png", TextureManager::Semantic::BaseColour, false);
    m_floor.normalTextureID = m_textureManager->Load(
        "textures/materials/floor_tile_normal.png", TextureManager::Semantic::Normal, false);
    m_floor.roughnessTextureID = m_textureManager->Load(
        "textures/materials/floor_tile_roughness.png", TextureManager::Semantic::Roughness, false);
    m_floor.uvTiling = glm::vec2(kRoomWidth / 2.5f, kRoomDepth / 2.5f);
}

void Classroom::LoadSharedAssets()
{
    m_studentDeskModel = std::make_shared<Model>(
        "models/furniture/student_desk/student_desk.obj", *m_textureManager);
    m_chairModel = std::make_shared<Model>(
        "models/furniture/chair/chair.obj", *m_textureManager);
    m_monitorModel = std::make_shared<Model>(
        "models/computer/monitor/monitor.obj", *m_textureManager);
}

Classroom::~Classroom()
{
    for (LightSwitch* ls : m_lightSwitches) delete ls;
    delete m_door;
    delete m_projectorPowerSwitch;
    delete m_projectorControlPanel;
    delete m_projector;
    for (Computer* c : m_computers) delete c;
    m_computers.clear();
    m_monitorModel.reset();
    m_chairModel.reset();
    m_studentDeskModel.reset();
    delete m_textureManager;
    delete m_unitBox;
}

void Classroom::BuildRoom()
{
    const float halfW = kRoomWidth * 0.5f;   // 13
    const float halfD = kRoomDepth * 0.5f;   // 10
    const glm::vec3 wallColor(0.62f, 0.83f, 0.84f); // light cyan
    const float wallThickness = 0.2f;

    // ---- Floor ----
    // Kept as its own member (not in m_collidableObjects) since the floor
    // never blocks XZ movement; this also lets SetFloorTexture() find and
    // update it directly once a texture has been loaded.
    {
        m_floor.position = glm::vec3(0.0f, -0.1f, 0.0f);
        m_floor.scale = glm::vec3(kRoomWidth, 0.1f, kRoomDepth);
        m_floor.color = glm::vec3(0.58f, 0.59f, 0.61f); // medium cool-gray classroom tile
        m_floor.shininess = 72.0f; // restrained tile highlight; avoids a black/mirror-like floor
        m_floor.specular = glm::vec3(0.28f, 0.28f, 0.30f);
        m_floor.ambient = 0.18f;
    }

    // ---- Ceiling ----
    {
        BoxInstance ceiling;
        ceiling.position = glm::vec3(0.0f, kCeilingHeight, 0.0f);
        ceiling.scale = glm::vec3(kRoomWidth, 0.1f, kRoomDepth);
        ceiling.color = glm::vec3(0.93f, 0.93f, 0.91f); // white office ceiling
        m_renderOnlyObjects.push_back(ceiling);
    }

    // ---- Front wall (whiteboard wall), at Z = -halfD ----
    {
        BoxInstance front;
        front.position = glm::vec3(0.0f, 0.0f, -halfD);
        front.scale = glm::vec3(kRoomWidth, kCeilingHeight, wallThickness);
        front.color = wallColor;
        m_collidableObjects.push_back(front);
    }

    // ---- Left and right walls, each with three window openings ----
    BuildSideWallWithWindows(-halfW, 1.0f);
    BuildSideWallWithWindows(halfW, -1.0f);

    // ---- Back wall, at Z = +halfD, with a doorway cut out ----
    const float doorHalfWidth = 0.9f;
    const float doorHeight = 2.4f;
    {
        float segWidth = halfW - doorHalfWidth;
        BoxInstance backLeft;
        backLeft.position = glm::vec3(-doorHalfWidth - segWidth * 0.5f, 0.0f, halfD);
        backLeft.scale = glm::vec3(segWidth, kCeilingHeight, wallThickness);
        backLeft.color = wallColor;
        m_collidableObjects.push_back(backLeft);

        BoxInstance backRight;
        backRight.position = glm::vec3(doorHalfWidth + segWidth * 0.5f, 0.0f, halfD);
        backRight.scale = glm::vec3(segWidth, kCeilingHeight, wallThickness);
        backRight.color = wallColor;
        m_collidableObjects.push_back(backRight);

        BoxInstance lintel;
        lintel.position = glm::vec3(0.0f, doorHeight, halfD);
        lintel.scale = glm::vec3(doorHalfWidth * 2.0f, kCeilingHeight - doorHeight, wallThickness);
        lintel.color = wallColor;
        m_collidableObjects.push_back(lintel);
    }

    // ---- Whiteboard, mounted on the inner face of the front wall ----
    glm::vec3 whiteboardCenter(0.0f, 2.5f, -halfD + wallThickness * 0.5f + 0.03f);
    glm::vec3 whiteboardSize(8.0f, 2.4f, 0.05f);
    {
        BoxInstance board;
        board.position = glm::vec3(whiteboardCenter.x, whiteboardCenter.y - whiteboardSize.y * 0.5f, whiteboardCenter.z);
        board.scale = whiteboardSize;
        board.color = glm::vec3(0.95f, 0.95f, 0.95f);
        board.shininess = 8.0f;
        m_renderOnlyObjects.push_back(board);

        // Whiteboard frame (thin darker border) - purely cosmetic.
        BoxInstance frame = board;
        frame.scale = whiteboardSize + glm::vec3(0.14f, 0.14f, -0.02f);
        frame.position.y = board.position.y - 0.02f;
        frame.position.z = board.position.z - 0.02f;
        frame.color = glm::vec3(0.55f, 0.55f, 0.55f);
        m_renderOnlyObjects.push_back(frame);
    }

    // ---- Teacher desk ----
    // Use the exact same desk visual/material as the student desks.
    // Only the workstation placement is different. No extra foot-support bar.
    glm::vec3 teacherDeskPos(-halfW + 2.5f, 0.0f, -halfD + 2.0f);
    {
        BoxInstance teacherTable;
        teacherTable.position = teacherDeskPos;
        teacherTable.scale = glm::vec3(2.3f, 0.75f, 0.65f);
        teacherTable.color = glm::vec3(0.85f, 0.85f, 0.83f);
        teacherTable.metallic = 0.0f;
        teacherTable.roughness = 0.52f;

        // Imported student desk is also used for the teacher desk so both
        // have exactly the same appearance. The box remains for collision.
        if (m_studentDeskModel && m_studentDeskModel->IsLoaded())
        {
            teacherTable.render = false;
            m_studentDeskInstances.push_back(
                glm::translate(glm::mat4(1.0f), teacherDeskPos));
        }
        m_collidableObjects.push_back(teacherTable);

        // Same surface height convention used by every student workstation.
        // This keeps keyboard and mouse clearly above the desktop.
        glm::vec3 teacherDeskTopCenter = teacherDeskPos + glm::vec3(0.0f, 0.75f, 0.0f);
        m_teacherComputer = new Computer(teacherDeskTopCenter, 0.75f, glm::pi<float>(), 0, m_monitorModel);
        m_computers.push_back(m_teacherComputer);
    }

    // ---- Ceiling LED light panels (3x3 grid, scaled for the larger lab) ----
    {
        const float lightY = kCeilingHeight - 0.05f;
        const float xs[3] = { -8.0f, 0.0f, 8.0f };
        const float zs[3] = { -5.0f, 0.0f, 5.0f };
        for (float x : xs)
        {
            for (float z : zs)
            {
                BoxInstance panel;
                panel.position = glm::vec3(x, lightY, z);
                panel.scale = glm::vec3(3.0f, 0.05f, 2.2f);
                panel.color = glm::vec3(0.85f, 0.85f, 0.85f);
                m_ceilingLightPanels.push_back(panel);
            }
        }
    }

    // ---- Student double-tables + chairs + computers ----
    // 5 columns x 4 rows = 20 tables, two computers/two chairs per table
    // (one seated on each side of the shared table) = 40 computers + 40 chairs.
    const float rowZ[4] = { -4.5f, -1.5f, 1.5f, 4.5f };
    const float colX[5] = { -9.6f, -4.8f, 0.0f, 4.8f, 9.6f };
    const float seatOffsets[2] = { -0.55f, 0.55f };
    int nextStudentComputerId = 1; // 0 is reserved for the teacher workstation
    for (float z : rowZ)
    {
        for (float x : colX)
        {
            BoxInstance table;
            table.position = glm::vec3(x, 0.0f, z);
            table.scale = glm::vec3(2.3f, 0.75f, 0.65f);
            table.color = glm::vec3(0.85f, 0.85f, 0.83f);
            table.metallic = 0.0f;
            table.roughness = 0.52f;
            if (m_studentDeskModel && m_studentDeskModel->IsLoaded())
            {
                table.render = false; // collision stays active; imported visual replaces only rendering
                m_studentDeskInstances.push_back(
                    glm::translate(glm::mat4(1.0f), glm::vec3(x, 0.0f, z)));
            }
            m_collidableObjects.push_back(table);

            for (float offset : seatOffsets)
            {
                float seatX = x + offset;

                BoxInstance seat;
                seat.position = glm::vec3(seatX, 0.0f, z + 0.55f);
                seat.scale = glm::vec3(0.42f, 0.42f, 0.42f);
                seat.color = glm::vec3(0.06f, 0.06f, 0.07f);
                // Collidable (not render-only): a chair is a real obstacle.
                // Using just the seat box (not the backrest too) keeps the
                // collision list small - "simplified AABB" per chair.
                const bool importedChair = m_chairModel && m_chairModel->IsLoaded();
                seat.render = !importedChair;
                m_collidableObjects.push_back(seat);

                BoxInstance backrest;
                backrest.position = glm::vec3(seatX, 0.40f, z + 0.75f);
                backrest.scale = glm::vec3(0.42f, 0.45f, 0.06f);
                backrest.color = glm::vec3(0.06f, 0.06f, 0.07f);
                backrest.render = !importedChair;
                m_renderOnlyObjects.push_back(backrest);
                if (importedChair)
                {
                    m_chairInstances.push_back(glm::translate(
                        glm::mat4(1.0f), glm::vec3(seatX, 0.0f, z + 0.55f)));
                }

                glm::vec3 deskTopCenter(seatX, 0.75f, z);
                m_computers.push_back(new Computer(deskTopCenter, 0.75f, 0.0f, nextStudentComputerId, m_monitorModel));
                ++nextStudentComputerId;
            }
        }
    }

    // ---- Projector (ceiling mounted, aimed at whiteboard) ----
    glm::vec3 projectorMount(0.0f, kCeilingHeight - 0.16f, -6.5f);
    m_projector = new Projector(projectorMount, whiteboardCenter, whiteboardSize, 0.0f);

    // Always-available projector switch at normal hand height. It forwards
    // to the same Projector instance as the keyboard shortcuts and teacher-PC
    // control panel; the ceiling hardware is intentionally not an E target.
    glm::vec3 directProjectorSwitchPos(-halfW + 6.0f, 1.15f,
                                       -halfD + wallThickness * 0.5f + 0.025f);
    m_projectorPowerSwitch = new ProjectorPowerSwitch(directProjectorSwitchPos,
                                                       0.0f, m_projector);

    // A small, non-collidable controller on the teacher desk. It forwards to
    // this exact Projector instance and only becomes operational when the
    // teacher workstation reaches Desktop.
    glm::vec3 teacherControlPos = teacherDeskPos + glm::vec3(0.76f, 0.82f, 0.20f);
    m_projectorControlPanel = new ProjectorControlPanel(teacherControlPos, glm::pi<float>(),
                                                        m_projector, m_teacherComputer);

    // ---- Wall switches: one by the door, one by the teacher's end. Both
    // are wired to the same m_lightCircuit, so either one toggles the same
    // shared ON/OFF state - pressing switch A on and then switch B always
    // turns the same lights back off. ----
    glm::vec3 switchNearDoor(halfW - 3.0f, 1.2f, halfD - wallThickness * 0.5f - 0.02f);
    glm::vec3 switchNearTeacher(-halfW + 5.0f, 1.2f, -halfD + wallThickness * 0.5f + 0.02f);
    m_lightSwitches.push_back(new LightSwitch(switchNearDoor, 0.0f, &m_lightCircuit));
    m_lightSwitches.push_back(new LightSwitch(switchNearTeacher, 0.0f, &m_lightCircuit));

    // ---- Door, hinged at the left edge of the doorway ----
    glm::vec3 hingePos(-doorHalfWidth, 0.0f, halfD);
    m_door = new Door(hingePos, doorHalfWidth * 2.0f, doorHeight, 0.06f, 1.0f);

    // ---- Spawn point, just inside the door, facing the whiteboard ----
    m_spawnPosition = glm::vec3(0.0f, 1.7f, halfD - 1.5f);
    m_spawnYaw = -90.0f;
}

void Classroom::BuildSideWallWithWindows(float wallX, float normalSign)
{
    const float halfD = kRoomDepth * 0.5f;
    const glm::vec3 wallColor(0.62f, 0.83f, 0.84f);
    const float wallThickness = 0.2f;

    const float windowCenters[3] = { -6.0f, 0.0f, 6.0f };
    const float windowHalfWidth = 1.0f;
    const float sillHeight = 1.0f;
    const float windowHeight = 1.6f;

    // Full-height wall segments that fill the gaps between/around windows.

    auto addFullHeightSegment = [&](float z0, float z1)
    {
        float width = z1 - z0;
        if (width <= 0.001f) return;
        BoxInstance seg;
        seg.position = glm::vec3(wallX, 0.0f, z0 + width * 0.5f);
        seg.scale = glm::vec3(wallThickness, kCeilingHeight, width);
        seg.color = wallColor;
        m_collidableObjects.push_back(seg);
    };

    // Segments: before window1, between window1/2, between window2/3, after window3.
    addFullHeightSegment(-halfD, windowCenters[0] - windowHalfWidth);
    addFullHeightSegment(windowCenters[0] + windowHalfWidth, windowCenters[1] - windowHalfWidth);
    addFullHeightSegment(windowCenters[1] + windowHalfWidth, windowCenters[2] - windowHalfWidth);
    addFullHeightSegment(windowCenters[2] + windowHalfWidth, halfD);

    for (float centerZ : windowCenters)
    {
        // Below the sill: full collidable wall (a window doesn't remove the
        // wall at floor level - only the glazed section above it).
        BoxInstance belowSill;
        belowSill.position = glm::vec3(wallX, 0.0f, centerZ);
        belowSill.scale = glm::vec3(wallThickness, sillHeight, windowHalfWidth * 2.0f);
        belowSill.color = wallColor;
        m_collidableObjects.push_back(belowSill);

        // Above the lintel: solid wall again, render-only (below-sill piece
        // above already covers this XZ footprint for collision purposes).
        float lintelY = sillHeight + windowHeight;
        BoxInstance aboveLintel;
        aboveLintel.position = glm::vec3(wallX, lintelY, centerZ);
        aboveLintel.scale = glm::vec3(wallThickness, kCeilingHeight - lintelY, windowHalfWidth * 2.0f);
        aboveLintel.color = wallColor;
        m_renderOnlyObjects.push_back(aboveLintel);

        // Glass pane filling the opening, slightly inset into the room and
        // faintly emissive so it reads as daylight coming through.
        BoxInstance glass;
        glass.position = glm::vec3(wallX + normalSign * (wallThickness * 0.5f + 0.02f), sillHeight, centerZ);
        glass.scale = glm::vec3(0.05f, windowHeight, windowHalfWidth * 1.8f);
        glass.color = glm::vec3(0.75f, 0.88f, 0.95f);
        glass.emissive = glm::vec3(0.35f, 0.5f, 0.6f);
        glass.shininess = 96.0f;
        glass.specular = glm::vec3(0.8f, 0.8f, 0.85f);
        m_renderOnlyObjects.push_back(glass);

        // A soft, constant daylight glow near each window (not tied to the
        // room's electric lights - a real window still admits light when
        // the ceiling panels are off).
        PointLight daylight;
        daylight.position = glm::vec3(wallX + normalSign * 1.0f, sillHeight + windowHeight * 0.5f, centerZ);
        daylight.color = glm::vec3(0.65f, 0.78f, 0.9f);
        daylight.intensity = 0.12f;
        m_windowDaylights.push_back(daylight);
    }
}

bool Classroom::CollidesXZ(const glm::vec3& pos, float radius) const
{
    auto circleHitsAABB = [&](const glm::vec3& lo, const glm::vec3& hi) -> bool
    {
        float closestX = std::clamp(pos.x, lo.x, hi.x);
        float closestZ = std::clamp(pos.z, lo.z, hi.z);
        float dx = pos.x - closestX;
        float dz = pos.z - closestZ;
        return (dx * dx + dz * dz) < (radius * radius);
    };

    for (const BoxInstance& box : m_collidableObjects)
    {
        if (circleHitsAABB(box.GetWorldMin(), box.GetWorldMax()))
            return true;
    }

    // Computer monitors and CPU cases are practical obstacles too (keyboard
    // and mouse are small/low enough that colliding with them isn't worth
    // the extra checks). Reusing each Computer's own precomputed box avoids
    // creating dozens of extra standalone collision entries.
    for (const Computer* c : m_computers)
    {
        if (circleHitsAABB(c->GetCollisionMin(), c->GetCollisionMax()))
            return true;
    }

    // Also block against the currently-closed door leaf so the player can't
    // walk through it while it's shut.
    if (m_door)
    {
        if (circleHitsAABB(m_door->GetAABBMin(), m_door->GetAABBMax()))
            return true;
    }

    return false;
}

glm::vec3 Classroom::ResolveCollision(const glm::vec3& currentPos, const glm::vec3& desiredPos, float radius) const
{
    glm::vec3 result = currentPos;

    glm::vec3 tryX = result;
    tryX.x = desiredPos.x;
    if (!CollidesXZ(tryX, radius))
        result.x = desiredPos.x;

    glm::vec3 tryZ = result;
    tryZ.z = desiredPos.z;
    if (!CollidesXZ(tryZ, radius))
        result.z = desiredPos.z;

    result.y = desiredPos.y;
    return result;
}

bool Classroom::IsRayOccluded(const glm::vec3& origin, const glm::vec3& direction, float maxDistance,
                               const glm::vec3& targetMin, const glm::vec3& targetMax) const
{
    // Standard slab-method ray/AABB test (mirrors
    // InteractionSystem::RayIntersectsAABB) but in full 3D, since occlusion
    // needs to respect wall/table height too, unlike the XZ-only player
    // collision test above.
    glm::vec3 rayOrigin = origin;
    glm::vec3 rayDir = direction;

    // The target's own horizontal centre - any collidable whose footprint
    // contains this point is (by construction, since every workstation's
    // monitor/keyboard/mouse/CPU case all sit within their own table's
    // footprint) that target's own supporting desk, not a wall or an
    // unrelated piece of furniture, so it must not be able to block
    // interaction with the very thing it's supporting.
    glm::vec3 targetCenter = (targetMin + targetMax) * 0.5f;

    auto isTargetsOwnSupport = [&](const glm::vec3& boxMin, const glm::vec3& boxMax) -> bool
    {
        return targetCenter.x >= boxMin.x && targetCenter.x <= boxMax.x &&
               targetCenter.z >= boxMin.z && targetCenter.z <= boxMax.z;
    };

    // Thin interactables are intentionally mounted very close to another
    // surface (wall switches beside a wall, projector touching the ceiling,
    // computers sitting on desks). Treat a blocker that overlaps/touches the
    // target AABB as part of that target's mounting/support geometry. Without
    // this target-aware epsilon, the support surface could win the second
    // occlusion test by a few millimetres and make a visibly selected object
    // impossible to activate with E.
    auto overlapsTarget = [&](const glm::vec3& boxMin, const glm::vec3& boxMax) -> bool
    {
        constexpr float epsilon = 0.035f;
        return boxMin.x <= targetMax.x + epsilon && boxMax.x >= targetMin.x - epsilon &&
               boxMin.y <= targetMax.y + epsilon && boxMax.y >= targetMin.y - epsilon &&
               boxMin.z <= targetMax.z + epsilon && boxMax.z >= targetMin.z - epsilon;
    };

    auto rayHitsAABB = [&](const glm::vec3& boxMin, const glm::vec3& boxMax) -> bool
    {
        float tmin = 0.02f;
        float tmax = maxDistance;
        for (int axis = 0; axis < 3; ++axis)
        {
            float d = rayDir[axis];
            float o = rayOrigin[axis];
            float lo = boxMin[axis];
            float hi = boxMax[axis];
            if (std::abs(d) < 1e-8f)
            {
                if (o < lo || o > hi) return false;
            }
            else
            {
                float t1 = (lo - o) / d;
                float t2 = (hi - o) / d;
                if (t1 > t2) std::swap(t1, t2);
                tmin = std::max(tmin, t1);
                tmax = std::min(tmax, t2);
                if (tmin > tmax) return false;
            }
        }
        return true;
    };

    for (const BoxInstance& box : m_collidableObjects)
    {
        glm::vec3 lo = box.GetWorldMin();
        glm::vec3 hi = box.GetWorldMax();
        if (overlapsTarget(lo, hi) || isTargetsOwnSupport(lo, hi))
            continue;
        if (rayHitsAABB(lo, hi))
            return true;
    }

    // Phase 5 also treats visual architectural surfaces as ray blockers.
    // This includes the ceiling, whiteboard/frame, window panes/sills and
    // chair backrests, preventing an interaction ray from passing through
    // the ceiling or whiteboard even though those thin pieces are not part
    // of player movement collision.
    for (const BoxInstance& box : m_renderOnlyObjects)
    {
        glm::vec3 lo = box.GetWorldMin();
        glm::vec3 hi = box.GetWorldMax();
        if (overlapsTarget(lo, hi))
            continue;
        if (rayHitsAABB(lo, hi))
            return true;
    }

    if (m_door)
    {
        glm::vec3 doorMin = m_door->GetAABBMin();
        glm::vec3 doorMax = m_door->GetAABBMax();
        if (!overlapsTarget(doorMin, doorMax) && !isTargetsOwnSupport(doorMin, doorMax) &&
            rayHitsAABB(doorMin, doorMax))
            return true;
    }

    return false;
}

void Classroom::Update(float deltaTime)
{
    // The light circuit is shared by every switch, so it is advanced here
    // exactly once regardless of how many switches are wired to it.
    m_lightCircuit.Update(deltaTime);
    m_door->Update(deltaTime);
    m_projector->Update(deltaTime);
    for (Computer* c : m_computers)
        c->Update(deltaTime);
}

void Classroom::Draw(const Shader& shader, bool includeVisualEffects) const
{
    auto drawBox = [&](const BoxInstance& b)
    {
        if (!b.render || (!includeVisualEffects && !b.castsShadow))
            return;

        shader.setBool("useInstancing", false);
        shader.setMat4("model", b.GetModelMatrix());
        Material material = Material::FromLegacy(b.color, b.emissive,
                                                  b.shininess, b.specular);
        material.metallic = b.metallic;
        material.roughness = b.roughness;
        material.ambientOcclusion = b.ambientOcclusion;
        material.tangentSpaceValid = true;
        material.baseColourMap = b.textureID;
        material.hasBaseColourMap = b.useTexture && b.textureID != 0;
        material.normalMap = b.normalTextureID;
        material.hasNormalMap = b.normalTextureID != 0;
        material.metallicMap = b.metallicTextureID;
        material.hasMetallicMap = b.metallicTextureID != 0;
        material.roughnessMap = b.roughnessTextureID;
        material.hasRoughnessMap = b.roughnessTextureID != 0;
        material.aoMap = b.aoTextureID;
        material.hasAOMap = b.aoTextureID != 0;
        material.emissiveMap = b.emissiveTextureID;
        material.hasEmissiveMap = b.emissiveTextureID != 0;
        material.Apply(shader, b.uvTiling);
        m_unitBox->Draw();
    };

    drawBox(m_floor);
    for (const BoxInstance& b : m_collidableObjects) drawBox(b);
    for (const BoxInstance& b : m_renderOnlyObjects) drawBox(b);

    // Shared imported geometry is loaded once and rendered with instancing.
    // Collision remains the manually defined BoxInstance set above.
    if (m_studentDeskModel && m_studentDeskModel->IsLoaded())
        m_studentDeskModel->DrawInstanced(shader, m_studentDeskInstances);
    if (m_chairModel && m_chairModel->IsLoaded())
        m_chairModel->DrawInstanced(shader, m_chairInstances);

    float lightIntensity = m_lightCircuit.GetIntensity();
    for (const BoxInstance& panel : m_ceilingLightPanels)
    {
        BoxInstance lit = panel;
        lit.emissive = glm::vec3(1.0f, 0.98f, 0.9f) * lightIntensity * 2.2f;
        lit.roughness = 0.28f;
        drawBox(lit);
    }

    for (const LightSwitch* ls : m_lightSwitches)
        ls->Draw(shader, *m_unitBox);
    m_door->Draw(shader, *m_unitBox);
    m_projector->Draw(shader, *m_unitBox, includeVisualEffects);
    if (m_projectorPowerSwitch)
        m_projectorPowerSwitch->Draw(shader, *m_unitBox);
    if (m_projectorControlPanel)
        m_projectorControlPanel->Draw(shader, *m_unitBox);
    // Computer cases are opaque objects. Projector effects use blending, so
    // explicitly restore opaque depth rendering before drawing PCs.
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    for (const Computer* c : m_computers)
        c->Draw(shader, *m_unitBox);
}

void Classroom::SetFloorTexture(unsigned int textureID)
{
    m_floor.useTexture = true;
    m_floor.textureID = textureID;
    // One tile roughly every 2.5m in each direction so a square source
    // image reads as repeating floor tiles rather than one stretched image
    // across the whole 26x20m room.
    m_floor.uvTiling = glm::vec2(kRoomWidth / 2.5f, kRoomDepth / 2.5f);
}

size_t Classroom::GetTextureCacheSize() const
{
    return m_textureManager ? m_textureManager->GetCachedTextureCount() : 0u;
}

bool Classroom::HasImportedDeskModel() const
{
    return m_studentDeskModel && m_studentDeskModel->IsLoaded();
}

bool Classroom::HasImportedChairModel() const
{
    return m_chairModel && m_chairModel->IsLoaded();
}

bool Classroom::HasImportedMonitorModel() const
{
    return m_monitorModel && m_monitorModel->IsLoaded();
}

std::vector<Interactable*> Classroom::GetInteractables() const
{
    std::vector<Interactable*> result;
    for (LightSwitch* ls : m_lightSwitches)
        result.push_back(ls);
    result.push_back(m_door);
    // The ceiling unit remains fully owned, updated, rendered and included
    // in projector lighting, but normal E interaction is intentionally routed
    // through the reachable wall switch or teacher control panel.
    if (m_projectorPowerSwitch) result.push_back(m_projectorPowerSwitch);
    if (m_projectorControlPanel) result.push_back(m_projectorControlPanel);
    for (Computer* c : m_computers)
        result.push_back(c);
    return result;
}


std::vector<SpotLight> Classroom::GetSpotLights() const
{
    std::vector<SpotLight> lights;
    if (m_projector && m_projector->IsEmittingLight())
    {
        SpotLight light;
        light.position = m_projector->GetLightWorldPosition();
        light.direction = m_projector->GetLightDirection();
        light.color = m_projector->GetLightColor();
        light.intensity = 4.5f * m_projector->GetLampBrightness();
        light.innerCutoffCos = std::cos(glm::radians(14.0f));
        light.outerCutoffCos = std::cos(glm::radians(22.0f));
        lights.push_back(light);
    }
    return lights;
}

std::vector<PointLight> Classroom::GetLights(const glm::vec3& cameraPos, bool includeProjectorPoint) const
{
    std::vector<PointLight> lights;

    float ceilingIntensity = m_lightCircuit.GetIntensity();
    if (ceilingIntensity > 0.001f)
    {
        for (const BoxInstance& panel : m_ceilingLightPanels)
        {
            PointLight light;
            light.position = panel.position;
            light.color = glm::vec3(1.0f, 0.98f, 0.9f);
            light.intensity = ceilingIntensity * 1.8f;
            lights.push_back(light);
        }
    }

    // Windows admit a faint daylight glow regardless of whether the
    // electric ceiling lights are on.
    for (const PointLight& daylight : m_windowDaylights)
        lights.push_back(daylight);

    for (const Computer* c : m_computers)
    {
        if (c->IsScreenLit())
        {
            PointLight light;
            light.position = c->GetScreenWorldPosition();
            light.color = c->GetScreenLightColor();
            light.intensity = 0.8f;
            lights.push_back(light);
        }
    }

    if (includeProjectorPoint && m_projector->IsEmittingLight())
    {
        PointLight light;
        light.position = m_projector->GetLightWorldPosition();
        light.color = m_projector->GetLightColor();
        light.intensity = m_projector->GetLightIntensity();
        lights.push_back(light);
    }

    // The scene can produce more active lights than the shader's MAX_LIGHTS
    // budget (kMaxUploadedLights, kept in sync with classroom.frag). Rather
    // than silently truncating in build order (which could drop a light
    // right next to the camera in favor of one on the far side of the
    // room), keep whichever lights are physically nearest the camera.
    if (lights.size() > kMaxUploadedLights)
    {
        std::sort(lights.begin(), lights.end(), [&](const PointLight& a, const PointLight& b)
        {
            float da = glm::dot(a.position - cameraPos, a.position - cameraPos);
            float db = glm::dot(b.position - cameraPos, b.position - cameraPos);
            return da < db;
        });
        lights.resize(kMaxUploadedLights);
    }

    return lights;
}
