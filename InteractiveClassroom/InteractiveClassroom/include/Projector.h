#ifndef PROJECTOR_H
#define PROJECTOR_H

#include "Computer.h"
#include "InteractionSystem.h"
#include "Mesh.h"
#include "Shader.h"
#include <glm/glm.hpp>
#include <string>

enum class ProjectorState
{
    Off,
    Starting,
    Active,
    ShuttingDown
};

enum class ProjectionMode
{
    Presentation,
    BlankWhite,
    ColourBars
};

// One ceiling-mounted projector. Hardware, timed power transitions,
// projection content, beam rendering, lighting and mode selection all live
// here so direct interaction, debug controls and the teacher desk panel can
// never create separate or conflicting projector state.
class Projector : public Interactable
{
public:
    Projector(const glm::vec3& ceilingMountPosition, const glm::vec3& whiteboardCenter,
              const glm::vec3& whiteboardSize, float roomFacingYRadians);
    ~Projector();

    Projector(const Projector&) = delete;
    Projector& operator=(const Projector&) = delete;

    // includeVisualEffects=false is used by the shadow-depth pass: the
    // physical casing still casts a shadow, while the projection and
    // transparent beam are deliberately excluded.
    void Draw(const Shader& shader, const Mesh& unitBox, bool includeVisualEffects = true) const;

    // Interactable
    glm::vec3 GetAABBMin() const override;
    glm::vec3 GetAABBMax() const override;
    std::string GetPrompt() const override;
    void Interact() override;
    bool InteractWithResult() override;
    void Update(float deltaTime) override;

    bool TogglePower();
    void CycleProjectionMode();
    void ForceOff();

    ProjectorState GetState() const { return m_state; }
    ProjectionMode GetProjectionMode() const { return m_projectionMode; }
    bool IsActive() const { return m_state == ProjectorState::Active; }
    bool IsEmittingLight() const { return m_lampBrightness > 0.005f; }

    float GetStartupProgress() const { return m_startupProgress; }
    float GetShutdownProgress() const { return m_shutdownProgress; }
    float GetLampBrightness() const { return m_lampBrightness; }
    float GetProjectionBrightness() const { return m_projectionBrightness; }
    float GetFanIntensity() const { return m_fanIntensity; }

    glm::vec3 GetLightWorldPosition() const;
    glm::vec3 GetLightDirection() const { return m_frontDirection; }
    glm::vec3 GetBodyPosition() const { return m_bodyBox.position + glm::vec3(0.0f, m_bodyBox.scale.y * 0.5f, 0.0f); }
    glm::vec3 GetLightColor() const { return glm::vec3(0.72f, 0.80f, 1.0f); }
    float GetLightIntensity() const { return 1.8f * m_lampBrightness; }

    std::string GetStateName() const;
    std::string GetProjectionModeName() const;

    static constexpr float kStartupDuration = 3.0f;
    static constexpr float kShutdownDuration = 3.0f;

private:
    void EnterStarting();
    void EnterShuttingDown();
    void UpdateStarting(float deltaTime);
    void UpdateShuttingDown(float deltaTime);

    void DrawHardware(const Shader& shader, const Mesh& unitBox) const;
    void DrawProjection(const Shader& shader, const Mesh& unitBox) const;
    void DrawPresentation(const Shader& shader, const Mesh& unitBox, float brightness) const;
    void DrawStartupContent(const Shader& shader, const Mesh& unitBox, float brightness) const;
    void DrawColourBars(const Shader& shader, const Mesh& unitBox, float brightness) const;
    void DrawProjectionElement(const Shader& shader, const Mesh& unitBox,
                               float u, float v, float width, float height,
                               const glm::vec3& color, float emissiveScale,
                               float depthOffset = 0.013f) const;
    void DrawBeam(const Shader& shader) const;

    static Mesh* CreateBeamMesh(const glm::vec3& nearCenter, const glm::vec3& farCenter,
                                float nearHalfWidth, float nearHalfHeight,
                                float farHalfWidth, float farHalfHeight);

    BoxInstance m_bodyBox;
    BoxInstance m_lensBox;
    BoxInstance m_powerLed;
    BoxInstance m_coolingLed;
    BoxInstance m_projectionPanel;
    Mesh* m_beamMesh = nullptr;

    // Projector-local directions transformed into world space from the
    // constructor yaw. At roomFacingYRadians == 0 these remain +X/right and
    // -Z/front, preserving the current classroom appearance.
    glm::vec3 m_frontDirection{0.0f, 0.0f, -1.0f};
    glm::vec3 m_rightDirection{1.0f, 0.0f, 0.0f};

    glm::vec3 m_projectionCenter{0.0f};
    float m_projectionWidth = 1.0f;
    float m_projectionHeight = 1.0f;

    ProjectorState m_state = ProjectorState::Off;
    ProjectionMode m_projectionMode = ProjectionMode::Presentation;
    float m_stateTimer = 0.0f;
    float m_startupProgress = 0.0f;
    float m_shutdownProgress = 0.0f;
    float m_lampBrightness = 0.0f;
    float m_projectionBrightness = 0.0f;
    float m_fanIntensity = 0.0f;
};

// Accessible wall-mounted projector control. This control is always
// available and forwards to the same Projector instance used by O/F7 and the
// teacher-PC panel. The ceiling hardware itself is rendering-only for E input.
class ProjectorPowerSwitch : public Interactable
{
public:
    ProjectorPowerSwitch(const glm::vec3& wallPosition, float facingYRadians,
                         Projector* projector);

    void Draw(const Shader& shader, const Mesh& unitBox) const;

    glm::vec3 GetAABBMin() const override;
    glm::vec3 GetAABBMax() const override;
    std::string GetPrompt() const override;
    void Interact() override;
    bool InteractWithResult() override;

private:
    BoxInstance m_plate;
    BoxInstance m_button;
    BoxInstance m_statusLed;
    Projector* m_projector = nullptr;
};

// Small teacher-desk control surface. It owns no projector state: when the
// teacher computer has reached Desktop, both its prompt and Interact() forward
// to the same Projector instance used by the wall switch and debug controls.
class ProjectorControlPanel : public Interactable
{
public:
    ProjectorControlPanel(const glm::vec3& deskSurfacePosition, float facingYRadians,
                          Projector* projector, const Computer* teacherComputer);

    void Draw(const Shader& shader, const Mesh& unitBox) const;

    glm::vec3 GetAABBMin() const override;
    glm::vec3 GetAABBMax() const override;
    std::string GetPrompt() const override;
    void Interact() override;
    bool InteractWithResult() override;

private:
    bool IsAvailable() const;

    BoxInstance m_panel;
    BoxInstance m_statusLed;
    Projector* m_projector = nullptr;
    const Computer* m_teacherComputer = nullptr;
};

#endif // PROJECTOR_H
