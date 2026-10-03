#ifndef COMPUTER_H
#define COMPUTER_H

#include "InteractionSystem.h"
#include "Mesh.h"
#include "Shader.h"
#include "Model.h"
#include <memory>
#include <glm/glm.hpp>

// Explicit four-state machine (Phase 4) - never collapsed into a single
// "isOn" boolean. Booting and ShuttingDown each run their own timed,
// multi-stage animation (see Computer::Update) rather than jumping
// straight between Off and Desktop.
enum class PowerState
{
    Off,
    Booting,
    Desktop,
    ShuttingDown
};

// One workstation: a dark plastic monitor casing with a distinct front
// screen panel, keyboard (+ its own small LED), mouse, and CPU tower case
// (+ its own small front power/indicator LED), sitting on a desk. Every
// instance owns its own complete state - power state, boot/shutdown
// progress, screen brightness, desktop color, keyboard/CPU indicator
// state, timers - so powering one workstation on or off can never affect
// any other one. The teacher computer is just another instance of this
// same class (with a reserved computer ID / accent color), not a separate
// implementation.
//
// The monitor casing, keyboard, mouse, and CPU case each report their own
// individual AABB via GetPartCount()/GetPartAABB() (Phase 4 correction:
// previously one large combined box also covered the empty space between
// them). InteractionSystem still always resolves a hit on any of these
// parts back to this single Computer instance, so looking at any part
// shows the same prompt and presses E on the same object - the prompt can
// never flicker between parts of one workstation.
class Computer : public Interactable
{
public:
    // deskTopCenter: world-space point on the desk surface where this
    // computer sits. deskTopY: world-space Y of the desk surface.
    // computerId: unique, stable per-workstation ID used to deterministically
    // pick this computer's desktop color variation (and, for id 0, the
    // reserved teacher accent color) - the same ID always yields the same
    // look across runs.
    Computer(const glm::vec3& deskTopCenter, float deskTopY, float facingYRadians, int computerId,
             std::shared_ptr<Model> monitorModel = nullptr);

    void Draw(const Shader& shader, const Mesh& unitBox) const;

    // Interactable - combined box (all parts), kept for API completeness
    // and as the base-class-required fallback; the raycast itself uses the
    // more precise per-part AABBs below.
    glm::vec3 GetAABBMin() const override;
    glm::vec3 GetAABBMax() const override;
    std::string GetPrompt() const override;
    void Interact() override;
    bool InteractWithResult() override;
    void Update(float deltaTime) override;

    // Phase 4 correction (requirement 7): four real per-part hit boxes -
    // monitor casing, keyboard, mouse, CPU case - instead of one box that
    // also covers the empty space between them. All four still resolve to
    // this same Computer when hit (InteractionSystem returns the owning
    // Interactable*, never a per-part identity).
    int GetPartCount() const override { return 4; }
    void GetPartAABB(int partIndex, glm::vec3& outMin, glm::vec3& outMax) const override;

    bool IsScreenLit() const { return m_screenBrightness > 0.01f; }
    glm::vec3 GetScreenWorldPosition() const;
    glm::vec3 GetScreenLightColor() const;
    glm::vec3 GetSoundPosition() const;

    // Simplified movement-collision box: monitor casing + CPU case only.
    // Keyboard and mouse are left out on purpose (too small/low to matter
    // for player collision and not worth the extra AABB tests every frame).
    glm::vec3 GetCollisionMin() const { return glm::min(m_monitorCasing.GetWorldMin(), m_cpuCase.GetWorldMin()); }
    glm::vec3 GetCollisionMax() const { return glm::max(m_monitorCasing.GetWorldMax(), m_cpuCase.GetWorldMax()); }

    int GetComputerId() const { return m_computerId; }
    PowerState GetPowerState() const { return m_state; }

    // Phase 4 correction (requirement 6): a safe bulk-shutdown request
    // (used by F5) distinct from the player's Interact(). Desktop ->
    // ShuttingDown as normal; Booting -> also enters ShuttingDown, but
    // starting its brightness ramp from whatever the screen's actual
    // brightness was at that moment (never a hardcoded 1.0), so there is
    // no visible jump/flash and no corrupted timer. Off and already-
    // ShuttingDown computers are left completely unchanged.
    void RequestShutdown();

    // Debug/demo-only (F6 in main.cpp): instantly resets this workstation
    // to Off, bypassing the shutdown animation entirely. Never called from
    // the normal E-interaction path or from RequestShutdown().
    void ForceOff();

private:
    void UpdateBooting(float deltaTime);
    void UpdateShuttingDown(float deltaTime);
    void EnterShuttingDown(); // shared by Interact() (Desktop) and RequestShutdown() (Desktop/Booting)
    static glm::vec3 ComputeDesktopColour(int computerId);
    static float ShutdownBrightnessCurve(float progress); // 1..0 shape across the shutdown, independent of the starting brightness

    // World-space screen basis, derived from the screen panel's own
    // position/rotation, used to place the procedural screen-content
    // elements (logo, loading bar, taskbar, icons, window, clock, ...)
    // precisely on its front face regardless of which way the workstation
    // faces.
    glm::vec3 ScreenCenterWorld() const;
    glm::vec3 ScreenRightWorld() const;
    glm::vec3 ScreenNormalWorld() const;

    // Draws one small procedural screen-content panel centred at (u, v)
    // meters from the screen's own centre (u = right, v = up - NOT a
    // corner or an edge), sized width x height meters, sitting
    // depthOffset meters in front of the screen glass to avoid z-fighting.
    // Centring is done in local space (see the .cpp), so u/v/position all
    // consistently mean "centre", with no separate world-space height
    // correction needed at the call site.
    void DrawScreenElement(const Shader& shader, const Mesh& unitBox,
                            float u, float v, float width, float height,
                            const glm::vec3& color, const glm::vec3& emissive,
                            float depthOffset = 0.006f) const;
    void DrawScreenContent(const Shader& shader, const Mesh& unitBox) const;
    void DrawDesktopChrome(const Shader& shader, const Mesh& unitBox, float fade) const;

    BoxInstance m_monitorCasing; // dark plastic bezel/back/sides - never changes colour, never emissive
    BoxInstance m_screenPanel;   // thin front glass - the only part that changes colour/emits light
    BoxInstance m_keyboard;
    BoxInstance m_keyboardLed;   // small indicator on the keyboard - the only part of it that glows
    BoxInstance m_mouse;
    BoxInstance m_cpuCase;
    BoxInstance m_cpuLed;        // small power/indicator light on the CPU case's front face

    int m_computerId;
    float m_facingYRadians;
    glm::vec3 m_desktopColour;
    std::shared_ptr<Model> m_monitorModel; // shared once by all 41 workstations

    PowerState m_state = PowerState::Off;
    float m_stateTimer = 0.0f;       // seconds elapsed since entering the current state
    float m_bootProgress = 0.0f;     // 0..1 across the whole boot sequence
    float m_shutdownProgress = 0.0f; // 0..1 across the whole shutdown sequence
    float m_screenBrightness = 0.0f; // 0..1, drives colour lerp, emissive light, and the point light
    float m_shutdownStartBrightness = 0.0f; // brightness captured at the moment ShuttingDown began
    bool m_keyboardLedOn = false;
    bool m_cpuIndicatorOn = false;

    // ~2.5-4s boot / ~1.5-3s shutdown, per the Phase 4 spec.
    static constexpr float kBootDuration = 3.2f;
    static constexpr float kShutdownDuration = 2.0f;
};

#endif // COMPUTER_H
