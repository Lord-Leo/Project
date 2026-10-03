#ifndef CLASSROOM_H
#define CLASSROOM_H

#include "InteractionSystem.h"
#include "Mesh.h"
#include "Shader.h"
#include "Door.h"
#include "Computer.h"
#include "Projector.h"
#include "Model.h"
#include "TextureManager.h"
#include <memory>
#include <glm/glm.hpp>
#include <vector>
#include <cmath>

struct PointLight
{
    glm::vec3 position;
    glm::vec3 color;
    float intensity;
};

struct SpotLight
{
    glm::vec3 position{0.0f};
    glm::vec3 direction{0.0f, 0.0f, -1.0f};
    glm::vec3 color{1.0f};
    float intensity = 0.0f;
    float innerCutoffCos = 0.95f;
    float outerCutoffCos = 0.90f;
};

// Shared state for the room's ceiling-light circuit. Every LightSwitch that
// is wired to the room holds a pointer to the same LightCircuit instance,
// so flipping any one switch changes the one shared ON/OFF state and fade,
// exactly like multiple real wall switches wired to the same lights.
// Update() must be called exactly once per frame (Classroom::Update owns
// this call) - the switches themselves never advance the fade.
class LightCircuit
{
public:
    void Toggle() { m_isOn = !m_isOn; }
    void Update(float deltaTime)
    {
        float target = m_isOn ? 1.0f : 0.0f;
        float diff = target - m_intensity;
        float maxStep = kFadeSpeed * deltaTime;
        if (std::abs(diff) <= maxStep)
            m_intensity = target;
        else
            m_intensity += (diff > 0.0f ? maxStep : -maxStep);
    }

    bool IsOn() const { return m_isOn; }
    float GetIntensity() const { return m_intensity; }

private:
    // Start with the classroom illuminated. This makes the first frame
    // immediately readable, makes the shadow toggle visually testable, and
    // still allows either wall switch (or the L shortcut) to turn the same
    // shared circuit off.
    bool m_isOn = true;
    float m_intensity = 1.0f;
    static constexpr float kFadeSpeed = 2.5f; // per second; quick but still visibly smooth
};

// A wall-mounted light switch. Any number of these can be wired to the same
// LightCircuit; toggling any one of them toggles the single shared light
// state, and every switch's prompt/visual always reflects that shared
// state (never its own local copy).
class LightSwitch : public Interactable
{
public:
    LightSwitch(const glm::vec3& wallPosition, float facingYRadians, LightCircuit* circuit);

    void Draw(const Shader& shader, const Mesh& unitBox) const;

    glm::vec3 GetAABBMin() const override;
    glm::vec3 GetAABBMax() const override;
    std::string GetPrompt() const override;
    void Interact() override;
    bool InteractWithResult() override;
    // No Update() override: the fade lives in the shared LightCircuit,
    // which Classroom::Update advances exactly once per frame regardless
    // of how many switches are wired to it.

private:
    BoxInstance m_plate;
    LightCircuit* m_circuit;
};

// Owns and builds every static and interactive piece of the room: floor,
// ceiling, four walls (with a doorway and window openings), whiteboard,
// teacher desk, 20 double student tables (40 chairs/40 computers), ceiling
// LED panels, the door, and the projector. Also implements simple AABB
// collision so the player cannot walk through walls, tables, chairs,
// computer monitors/CPU cases, or the teacher desk.
class Classroom
{
public:
    Classroom();
    ~Classroom();

    Classroom(const Classroom&) = delete;
    Classroom& operator=(const Classroom&) = delete;

    void Update(float deltaTime);
    void Draw(const Shader& shader, bool includeVisualEffects = true) const;

    // Resolves movement from currentPos toward desiredPos against all
    // collidable geometry, treating the player as a vertical cylinder of
    // the given radius (only X/Z are considered - the room is single-story).
    glm::vec3 ResolveCollision(const glm::vec3& currentPos, const glm::vec3& desiredPos, float radius) const;

    std::vector<Interactable*> GetInteractables() const;

    // All 41 Computer instances (40 student + 1 teacher), for the F4/F5/F6
    // debug/demo controls in main.cpp. Returns raw pointers (Classroom
    // retains ownership) - the same pattern already used by
    // GetInteractables().
    std::vector<Computer*> GetComputers() const { return m_computers; }

    Projector* GetProjector() const { return m_projector; }
    Computer* GetTeacherComputer() const { return m_teacherComputer; }
    Door* GetDoor() const { return m_door; }

    // Reliable keyboard fallback for the two physical wall switches. This
    // toggles the exact same LightCircuit, never a duplicate light state.
    void ToggleLights() { m_lightCircuit.Toggle(); }
    bool AreLightsOn() const { return m_lightCircuit.IsOn(); }
    float GetCeilingLightIntensity() const { return m_lightCircuit.GetIntensity(); }

    // True if classroom geometry (walls, ceiling, whiteboard, window
    // segments, tables, chairs, teacher desk) blocks the line of sight from origin
    // toward direction before maxDistance is reached. Used to make sure a
    // workstation can't be interacted with through a wall or another large
    // object, even though its own AABB might technically be within range.
    //
    // targetMin/targetMax is the AABB of whatever the player is actually
    // trying to interact with (e.g. the hit Computer's combined bounding
    // box). Any collidable whose own footprint contains that target's
    // horizontal centre is treated as "the target's own supporting desk"
    // and excluded from the test - so a workstation's own table can never
    // incorrectly block interaction with its own monitor/keyboard/mouse/
    // CPU case, while every other wall, window segment, or unrelated desk
    // still blocks normally. This replaces the old fixed-percentage
    // distance margin with an approach aware of which geometry actually
    // belongs to the target.
    bool IsRayOccluded(const glm::vec3& origin, const glm::vec3& direction, float maxDistance,
                        const glm::vec3& targetMin, const glm::vec3& targetMax) const;

    // Returns the lights that should be uploaded to the shader this frame.
    // If more active lights exist than the shader's MAX_LIGHTS budget, the
    // ones nearest cameraPos are kept so no visually-important nearby light
    // is silently dropped in favor of a far-away one.
    std::vector<PointLight> GetLights(const glm::vec3& cameraPos, bool includeProjectorPoint = true) const;
    std::vector<SpotLight> GetSpotLights() const;

    // Assigns a loaded (or fallback) texture to the floor and enables
    // texture sampling for it, tiled so it reads as floor tiles rather than
    // one image stretched across the whole room.
    void SetFloorTexture(unsigned int textureID);
    size_t GetTextureCacheSize() const;
    bool HasImportedDeskModel() const;
    bool HasImportedChairModel() const;
    bool HasImportedMonitorModel() const;

    glm::vec3 GetSpawnPosition() const { return m_spawnPosition; }
    float GetSpawnYaw() const { return m_spawnYaw; }

    // Phase 2: enlarged to a real university-lab footprint so 20 double
    // tables (40 computers / 40 chairs) fit with realistic aisle widths.
    static constexpr float kRoomWidth = 26.0f;    // meters, X extent
    static constexpr float kRoomDepth = 20.0f;    // meters, Z extent
    static constexpr float kCeilingHeight = 4.0f; // meters

    // Must match (or be <=) MAX_LIGHTS in shaders/classroom.frag.
    static constexpr size_t kMaxUploadedLights = 64;

private:
    void LoadSharedAssets();
    void BuildRoom();
    void BuildSideWallWithWindows(float wallX, float normalSign);
    bool CollidesXZ(const glm::vec3& pos, float radius) const;

    Mesh* m_unitBox = nullptr;
    TextureManager* m_textureManager = nullptr;
    std::shared_ptr<Model> m_studentDeskModel;
    std::shared_ptr<Model> m_chairModel;
    std::shared_ptr<Model> m_monitorModel;
    std::vector<glm::mat4> m_studentDeskInstances;
    std::vector<glm::mat4> m_chairInstances;

    BoxInstance m_floor;
    std::vector<BoxInstance> m_renderOnlyObjects; // ceiling, whiteboard, chairs' backrests, window glass/sills...
    std::vector<BoxInstance> m_collidableObjects; // walls, tables, chair seats, teacher desk (also rendered)
    std::vector<BoxInstance> m_ceilingLightPanels;
    std::vector<PointLight>  m_windowDaylights;   // faint fixed "daylight" glow near each window

    LightCircuit m_lightCircuit; // single shared ON/OFF + fade state for every wall switch
    std::vector<LightSwitch*> m_lightSwitches; // multiple wall switches, all wired to m_lightCircuit
    Door* m_door = nullptr;
    Projector* m_projector = nullptr;
    ProjectorPowerSwitch* m_projectorPowerSwitch = nullptr;
    ProjectorControlPanel* m_projectorControlPanel = nullptr;
    Computer* m_teacherComputer = nullptr; // non-owning alias; owned in m_computers
    std::vector<Computer*> m_computers;

    glm::vec3 m_spawnPosition{0.0f, 1.7f, 5.0f};
    float m_spawnYaw = -90.0f;
};

#endif // CLASSROOM_H
