#include "Computer.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <limits>
#include <cmath>
#include <utility>
#include "Material.h"

Computer::Computer(const glm::vec3& deskTopCenter, float deskTopY, float facingYRadians, int computerId,
                   std::shared_ptr<Model> monitorModel)
    : m_computerId(computerId)
    , m_facingYRadians(facingYRadians)
    , m_desktopColour(ComputeDesktopColour(computerId))
    , m_monitorModel(std::move(monitorModel))
{
    // Canonical, rotation-consistent basis - derived once (from the mesh's
    // own local -Z front face, rotated by facingYRadians) and reused for
    // EVERY placement in this constructor, exactly the same as the runtime
    // screen-content helpers below use. Previously this constructor used
    // its own separately hand-rolled sin/cos formulas that only happened
    // to numerically agree with ScreenNormalWorld()/ScreenRightWorld() at
    // angle=0 and angle=PI (the two angles this project actually uses,
    // where sin(angle)=0 masks the discrepancy) - not for angles in
    // general. Calling the same member functions here instead removes
    // that latent inconsistency entirely.
    glm::vec3 towardUser = ScreenNormalWorld();  // the direction the screen (and the seated user) faces
    glm::vec3 awayFromUser = -towardUser;        // the mesh's own rotated local -Z: where the monitor sits, away from the user
    glm::vec3 right = ScreenRightWorld();

    // ---- Monitor: dark casing + a distinct thin front screen panel ----
    glm::vec3 monitorPos = deskTopCenter + awayFromUser * 0.14f;
    monitorPos.y = deskTopY + 0.015f;
    m_monitorCasing.position = monitorPos;
    m_monitorCasing.scale = glm::vec3(0.46f, 0.32f, 0.035f);
    m_monitorCasing.rotationY = facingYRadians;
    m_monitorCasing.color = glm::vec3(0.045f, 0.045f, 0.05f); // dark plastic - fixed, never emissive

    // The screen panel sits flush against the casing's front face (the
    // side facing the user - see towardUser above), inset slightly from
    // its edges so a visible bezel border remains on the casing.
    glm::vec3 screenPanelPos = monitorPos + towardUser * (m_monitorCasing.scale.z * 0.5f + 0.002f);
    m_screenPanel.position = screenPanelPos;
    m_screenPanel.scale = glm::vec3(0.40f, 0.27f, 0.01f);
    m_screenPanel.rotationY = facingYRadians;
    m_screenPanel.color = glm::vec3(0.02f, 0.02f, 0.025f); // off by default

    // ---- Keyboard + its own small LED ----
    glm::vec3 keyboardPos = deskTopCenter - awayFromUser * 0.16f;
    keyboardPos.y = deskTopY + 0.018f;
    m_keyboard.position = keyboardPos;
    m_keyboard.scale = glm::vec3(0.34f, 0.025f, 0.13f);
    m_keyboard.rotationY = facingYRadians;
    m_keyboard.color = glm::vec3(0.10f, 0.10f, 0.11f);

    glm::vec3 keyboardLedPos = keyboardPos
                              + glm::vec3(0.0f, m_keyboard.scale.y, 0.0f)
                              + right * (m_keyboard.scale.x * 0.38f)
                              + towardUser * (m_keyboard.scale.z * 0.30f);
    m_keyboardLed.position = keyboardLedPos;
    m_keyboardLed.scale = glm::vec3(0.02f, 0.006f, 0.012f);
    m_keyboardLed.rotationY = facingYRadians;
    m_keyboardLed.color = glm::vec3(0.05f, 0.05f, 0.05f);

    // ---- Mouse ----
    glm::vec3 mousePos = deskTopCenter - awayFromUser * 0.14f + right * 0.24f;
    mousePos.y = deskTopY + 0.018f;
    m_mouse.position = mousePos;
    m_mouse.scale = glm::vec3(0.06f, 0.03f, 0.10f);
    m_mouse.rotationY = facingYRadians;
    m_mouse.color = glm::vec3(0.10f, 0.10f, 0.11f);

    // ---- CPU tower: ON TOP of the desk, beside the monitor ----
    // Every workstation (teacher + students) uses this same placement.
    // The tower is deliberately rendered as a solid matte-black case.
    glm::vec3 cpuPos = deskTopCenter - right * 0.48f + awayFromUser * 0.08f;
    cpuPos.y = deskTopY + 0.018f; // BoxInstance Y starts at its base: sit directly on desktop
    m_cpuCase.position = cpuPos;
    m_cpuCase.scale = glm::vec3(0.20f, 0.34f, 0.28f);
    m_cpuCase.rotationY = facingYRadians;
    m_cpuCase.color = glm::vec3(0.018f, 0.018f, 0.020f); // fully solid black tower

    // The CPU LED sits on the case's visible front face - the same
    // towardUser direction as the screen panel, so it faces the user
    // rather than the wall/whiteboard side.
    glm::vec3 cpuFaceCenter = cpuPos
                             + glm::vec3(0.0f, m_cpuCase.scale.y * 0.28f, 0.0f)
                             + towardUser * (m_cpuCase.scale.z * 0.5f + 0.002f);
    m_cpuLed.position = cpuFaceCenter - glm::vec3(0.0f, 0.01f * 0.5f, 0.0f); // recenter a 0.01-tall LED on that point
    m_cpuLed.scale = glm::vec3(0.02f, 0.01f, 0.008f);
    m_cpuLed.rotationY = facingYRadians;
    m_cpuLed.color = glm::vec3(0.05f, 0.05f, 0.05f);
}

glm::vec3 Computer::ComputeDesktopColour(int computerId)
{
    // Computer ID 0 is reserved for the teacher workstation - a distinct
    // accent color, still produced by this exact same function/class (not
    // a separate implementation).
    if (computerId == 0)
        return glm::vec3(0.50f, 0.36f, 0.12f); // warm amber/gold, reads as "teacher station"

    // Five muted, realistic desktop-background variations, cycled
    // deterministically by ID so the same workstation always looks the
    // same across runs, and neighbors are visibly different.
    static const glm::vec3 kPalette[5] = {
        glm::vec3(0.15f, 0.35f, 0.65f), // blue
        glm::vec3(0.08f, 0.18f, 0.42f), // dark blue
        glm::vec3(0.10f, 0.45f, 0.45f), // teal
        glm::vec3(0.30f, 0.18f, 0.48f), // purple
        glm::vec3(0.12f, 0.42f, 0.28f), // green
    };
    int index = ((computerId - 1) % 5 + 5) % 5;
    return kPalette[index];
}

// ============================================================================
// State machine
// ============================================================================
std::string Computer::GetPrompt() const
{
    switch (m_state)
    {
        case PowerState::Off:          return "Press E to power on computer";
        case PowerState::Booting:      return "Computer is starting...";
        case PowerState::Desktop:      return "Press E to shut down computer";
        case PowerState::ShuttingDown: return "Computer is shutting down...";
    }
    return "";
}

void Computer::EnterShuttingDown()
{
    m_state = PowerState::ShuttingDown;
    m_stateTimer = 0.0f;
    m_shutdownProgress = 0.0f;
    // Always ramp down from whatever the screen's actual brightness is
    // right now - correct whether that's a full 1.0 (from Desktop) or some
    // partial value (from a boot cancelled via RequestShutdown), so there
    // is never a visible jump/flash and no corrupted timer.
    m_shutdownStartBrightness = m_screenBrightness;
}

void Computer::Interact()
{
    (void)InteractWithResult();
}

bool Computer::InteractWithResult()
{
    // Only the two stable states accept a new command; Booting and
    // ShuttingDown must finish their own animation first, so a repeated E
    // press can never restart or corrupt it mid-flight.
    if (m_state == PowerState::Off)
    {
        m_state = PowerState::Booting;
        m_stateTimer = 0.0f;
        m_bootProgress = 0.0f;
        return true;
    }
    if (m_state == PowerState::Desktop)
    {
        EnterShuttingDown();
        return true;
    }
    return false;
}

void Computer::RequestShutdown()
{
    // Safe bulk-shutdown entry point (F5): Desktop behaves exactly like a
    // player pressing E. Booting is cancelled cleanly straight into
    // ShuttingDown (EnterShuttingDown captures whatever brightness the
    // boot had reached so far, so the fade continues smoothly rather than
    // jumping). Off and already-ShuttingDown computers are left untouched.
    if (m_state == PowerState::Desktop || m_state == PowerState::Booting)
        EnterShuttingDown();
}

void Computer::ForceOff()
{
    // Debug/demo-only instant reset (F6) - deliberately bypasses the
    // animated shutdown; never reachable from Interact()/E or from
    // RequestShutdown().
    m_state = PowerState::Off;
    m_stateTimer = 0.0f;
    m_bootProgress = 0.0f;
    m_shutdownProgress = 0.0f;
    m_shutdownStartBrightness = 0.0f;
    m_screenBrightness = 0.0f;
    m_keyboardLedOn = false;
    m_cpuIndicatorOn = false;
}

void Computer::Update(float deltaTime)
{
    switch (m_state)
    {
        case PowerState::Off:
            m_screenBrightness = 0.0f;
            m_keyboardLedOn = false;
            m_cpuIndicatorOn = false;
            break;

        case PowerState::Booting:
            UpdateBooting(deltaTime);
            break;

        case PowerState::Desktop:
            m_stateTimer += deltaTime;
            m_screenBrightness = 1.0f;
            m_keyboardLedOn = true;
            m_cpuIndicatorOn = true;
            break;

        case PowerState::ShuttingDown:
            UpdateShuttingDown(deltaTime);
            break;
    }
}

void Computer::UpdateBooting(float deltaTime)
{
    m_stateTimer += deltaTime;
    m_bootProgress = std::min(m_stateTimer / kBootDuration, 1.0f);

    // Four boot stages, each with its own brightness ramp (matches the
    // visual stages drawn in DrawScreenContent): black+glow -> logo ->
    // loading bar -> desktop fade-in.
    if (m_bootProgress < 0.15f)
        m_screenBrightness = glm::mix(0.0f, 0.15f, m_bootProgress / 0.15f);
    else if (m_bootProgress < 0.40f)
        m_screenBrightness = glm::mix(0.15f, 0.40f, (m_bootProgress - 0.15f) / 0.25f);
    else if (m_bootProgress < 0.80f)
        m_screenBrightness = glm::mix(0.40f, 0.70f, (m_bootProgress - 0.40f) / 0.40f);
    else
        m_screenBrightness = glm::mix(0.70f, 1.0f, (m_bootProgress - 0.80f) / 0.20f);

    // Keyboard LED fades in only once the desktop itself is fading in.
    m_keyboardLedOn = m_bootProgress > 0.80f;
    // CPU indicator blinks while booting - a lightweight "working" cue.
    m_cpuIndicatorOn = std::fmod(m_stateTimer, 0.6f) < 0.3f;

    if (m_bootProgress >= 1.0f)
    {
        m_state = PowerState::Desktop;
        m_stateTimer = 0.0f;
        m_screenBrightness = 1.0f;
        m_keyboardLedOn = true;
        m_cpuIndicatorOn = true;
    }
}

float Computer::ShutdownBrightnessCurve(float progress)
{
    // Shape of the shutdown fade (as a fraction of the starting
    // brightness): dims -> brief "shutting down" plateau -> fades to zero.
    if (progress < 0.3f)
        return glm::mix(1.0f, 0.5f, progress / 0.3f);
    else if (progress < 0.6f)
        return glm::mix(0.5f, 0.25f, (progress - 0.3f) / 0.3f);
    else
        return glm::mix(0.25f, 0.0f, (progress - 0.6f) / 0.4f);
}

void Computer::UpdateShuttingDown(float deltaTime)
{
    m_stateTimer += deltaTime;
    m_shutdownProgress = std::min(m_stateTimer / kShutdownDuration, 1.0f);

    m_screenBrightness = m_shutdownStartBrightness * ShutdownBrightnessCurve(m_shutdownProgress);

    m_keyboardLedOn = m_shutdownProgress < 0.6f;
    m_cpuIndicatorOn = m_shutdownProgress < 0.9f;

    if (m_shutdownProgress >= 1.0f)
    {
        m_state = PowerState::Off;
        m_stateTimer = 0.0f;
        m_bootProgress = 0.0f;
        m_shutdownProgress = 0.0f;
        m_shutdownStartBrightness = 0.0f;
        m_screenBrightness = 0.0f;
        m_keyboardLedOn = false;
        m_cpuIndicatorOn = false;
    }
}

// ============================================================================
// Screen basis + procedural content
// ============================================================================
// The shared unit-box mesh (Mesh.cpp) defines its front face as local -Z.
// Rotating that local -Z axis by angle θ about world +Y (the same rotation
// BoxInstance::GetModelMatrix applies) gives:
//   RotateY((0,0,-1), θ) = (-sinθ, 0, -cosθ)
// That is the direction the mesh's own front face points in world space -
// i.e. away from the seated user for every workstation in this room (the
// monitor casing's front face points back toward the whiteboard/wall side,
// with the user sitting on the opposite side, behind the keyboard). So the
// direction that actually faces the user - what the screen panel, its
// procedural content, the CPU LED, and the monitor's point light all need
// to point toward - is the NEGATION of that: (sinθ, 0, cosθ). This is the
// single, canonical, rotation-consistent basis used everywhere in this
// class (including the constructor above, via these same two functions),
// so there is exactly one definition of "toward the user" / "right" that
// can never silently disagree between two independently hand-rolled
// formulas.
glm::vec3 Computer::ScreenNormalWorld() const
{
    return glm::normalize(glm::vec3(sinf(m_facingYRadians), 0.0f, cosf(m_facingYRadians)));
}

// Rotating the mesh's local +X axis (its own "right") the same way gives
// RotateY((1,0,0), θ) = (cosθ, 0, -sinθ) - the workstation's world-space
// right-hand direction as seen by the user facing it.
glm::vec3 Computer::ScreenRightWorld() const
{
    return glm::normalize(glm::vec3(cosf(m_facingYRadians), 0.0f, -sinf(m_facingYRadians)));
}

glm::vec3 Computer::ScreenCenterWorld() const
{
    return m_screenPanel.position
         + glm::vec3(0.0f, m_screenPanel.scale.y * 0.5f, 0.0f)
         + ScreenNormalWorld() * (m_screenPanel.scale.z * 0.5f);
}

void Computer::DrawScreenElement(const Shader& shader, const Mesh& unitBox,
                                  float u, float v, float width, float height,
                                  const glm::vec3& color, const glm::vec3& emissive,
                                  float depthOffset) const
{
    // u, v, width, height, and this element's effective position all
    // describe its CENTRE on the screen plane (u = right offset, v = up
    // offset, both in metres from the screen's own centre) - never a
    // corner or a bottom edge. Centring is done in local space (translate
    // by -0.5 on Y, in the object's own unscaled frame, before the scale
    // is applied - the shared unit-box mesh spans y:[0,1]) rather than as
    // a world-space "up * height * 0.5" correction, so it stays exactly
    // correct regardless of the workstation's facing.
    glm::vec3 right = ScreenRightWorld();
    glm::vec3 up(0.0f, 1.0f, 0.0f);
    glm::vec3 normal = ScreenNormalWorld();

    glm::vec3 centerWorld = ScreenCenterWorld() + right * u + up * v + normal * depthOffset;

    glm::mat4 model = glm::translate(glm::mat4(1.0f), centerWorld);
    model = glm::rotate(model, m_facingYRadians, glm::vec3(0.0f, 1.0f, 0.0f));

    // IMPORTANT: the unit box spans y:[0,1]. The centring translation must
    // be applied in unit-box space BEFORE scaling. With GLM's post-multiply
    // convention that means S * T, not T * S. The previous order translated
    // every icon/window/taskbar downward by a fixed 0.5 world metres, which
    // is why the desktop design appeared underneath the physical monitor.
    model = glm::scale(model, glm::vec3(width, height, 0.002f));
    model = glm::translate(model, glm::vec3(0.0f, -0.5f, 0.0f));

    shader.setBool("useInstancing", false);
    shader.setMat4("model", model);
    Material material = Material::FromLegacy(color, emissive, 16.0f, glm::vec3(0.15f));
    material.roughness = 0.35f;
    material.Apply(shader);
    unitBox.Draw();
}

void Computer::DrawDesktopChrome(const Shader& shader, const Mesh& unitBox, float fade) const
{
    fade = glm::clamp(fade, 0.0f, 1.0f);
    if (fade <= 0.001f) return;

    const float kBackgroundDepth = 0.002f; // drawn first, closer to the glass
    const float kForegroundDepth = 0.006f; // drawn after, in front of the background - never hidden by it

    // Full screen-sized background wash, drawn first and strictly behind
    // every element drawn after it, so nothing here ever covers the icons/
    // taskbar/window/clock.
    DrawScreenElement(shader, unitBox, 0.0f, 0.0f,
                       m_screenPanel.scale.x * 0.94f, m_screenPanel.scale.y * 0.94f,
                       m_desktopColour, m_desktopColour * 0.25f * fade, kBackgroundDepth);

    // Taskbar along the bottom (kept within the screen panel's +/-0.135
    // vertical half-extent).
    glm::vec3 taskbarColor(0.08f, 0.08f, 0.09f);
    DrawScreenElement(shader, unitBox, 0.0f, -0.115f, 0.36f, 0.03f,
                       taskbarColor, taskbarColor * 0.5f * fade, kForegroundDepth);

    // A handful of small app-icon squares, upper-left, muted (not brand marks).
    static const glm::vec3 kIconColors[4] = {
        glm::vec3(0.55f, 0.22f, 0.20f),
        glm::vec3(0.20f, 0.35f, 0.55f),
        glm::vec3(0.22f, 0.50f, 0.28f),
        glm::vec3(0.55f, 0.45f, 0.18f),
    };
    for (int i = 0; i < 4; ++i)
    {
        float u = -0.16f + (float)i * 0.05f;
        DrawScreenElement(shader, unitBox, u, 0.10f, 0.032f, 0.032f,
                           kIconColors[i], kIconColors[i] * 0.6f * fade, kForegroundDepth);
    }

    // One open window: body + a slightly darker title-bar strip.
    glm::vec3 windowBody = glm::mix(glm::vec3(0.85f, 0.85f, 0.85f), m_desktopColour, 0.15f);
    glm::vec3 windowTitle = glm::mix(windowBody, glm::vec3(0.0f), 0.35f);
    DrawScreenElement(shader, unitBox, 0.03f, -0.02f, 0.18f, 0.12f,
                       windowBody, windowBody * 0.35f * fade, kForegroundDepth);
    DrawScreenElement(shader, unitBox, 0.03f, 0.05f, 0.18f, 0.025f,
                       windowTitle, windowTitle * 0.35f * fade, kForegroundDepth + 0.001f);

    // A small clock-area readout on the taskbar's right edge.
    glm::vec3 clockColor(0.75f, 0.78f, 0.8f);
    DrawScreenElement(shader, unitBox, 0.16f, -0.115f, 0.045f, 0.018f,
                       clockColor, clockColor * 0.6f * fade, kForegroundDepth + 0.001f);
}

void Computer::DrawScreenContent(const Shader& shader, const Mesh& unitBox) const
{
    glm::vec3 accent = glm::mix(m_desktopColour, glm::vec3(1.0f), 0.35f);

    if (m_state == PowerState::Booting)
    {
        if (m_bootProgress < 0.15f)
        {
            // Stage 1: black screen with only the screen panel's own faint
            // power glow (handled by Draw() via m_screenBrightness) - no
            // extra geometry drawn here.
            return;
        }
        else if (m_bootProgress < 0.40f)
        {
            // Stage 2: simple centered boot symbol (not a real logo) plus a
            // classic blinking boot cursor beneath it.
            float t = (m_bootProgress - 0.15f) / 0.25f;
            DrawScreenElement(shader, unitBox, 0.0f, 0.02f, 0.065f, 0.065f, accent, accent * t * 0.9f);

            bool cursorOn = std::fmod(m_stateTimer, 0.6f) < 0.3f;
            if (cursorOn)
                DrawScreenElement(shader, unitBox, 0.0f, -0.06f, 0.018f, 0.018f, accent, accent * 0.8f);
            return;
        }
        else if (m_bootProgress < 0.80f)
        {
            // Stage 3: loading bar filling left-to-right, plus three
            // pulsing loading dots.
            float stageT = glm::clamp((m_bootProgress - 0.40f) / 0.40f, 0.0f, 1.0f);

            glm::vec3 barColor(0.12f, 0.12f, 0.13f);
            DrawScreenElement(shader, unitBox, 0.0f, -0.08f, 0.22f, 0.022f, barColor, barColor * 0.3f);

            float fillWidth = 0.20f * stageT;
            if (fillWidth > 0.002f)
                DrawScreenElement(shader, unitBox, -0.10f + fillWidth * 0.5f, -0.08f, fillWidth, 0.016f,
                                   accent, accent * 0.9f, 0.007f);

            for (int i = 0; i < 3; ++i)
            {
                float phase = m_stateTimer * 4.0f - (float)i * 0.6f;
                float pulse = 0.4f + 0.6f * std::max(0.0f, sinf(phase));
                DrawScreenElement(shader, unitBox, -0.03f + (float)i * 0.03f, 0.02f, 0.013f, 0.013f,
                                   accent, accent * pulse);
            }
            return;
        }
        else
        {
            // Stage 4: desktop content smoothly fading in.
            float fadeT = (m_bootProgress - 0.80f) / 0.20f;
            DrawDesktopChrome(shader, unitBox, fadeT);
            return;
        }
    }
    else if (m_state == PowerState::Desktop)
    {
        DrawDesktopChrome(shader, unitBox, 1.0f);
    }
    else if (m_state == PowerState::ShuttingDown)
    {
        if (m_shutdownProgress < 0.3f)
        {
            DrawDesktopChrome(shader, unitBox, 1.0f - (m_shutdownProgress / 0.3f));
        }
        else if (m_shutdownProgress < 0.6f)
        {
            float t = 1.0f - (m_shutdownProgress - 0.3f) / 0.3f;
            DrawScreenElement(shader, unitBox, 0.0f, 0.0f, 0.09f, 0.045f, glm::vec3(0.08f, 0.08f, 0.08f), accent * 0.5f * t);
        }
        // 0.6-1.0: nothing extra - the screen is fading fully to black.
    }
}

// ============================================================================
// Draw
// ============================================================================
void Computer::Draw(const Shader& shader, const Mesh& unitBox) const
{
    auto drawBox = [&](const BoxInstance& b, const glm::vec3& color, const glm::vec3& emissive,
                       float shininess, float ambient, const glm::vec3& specular,
                       float metallic, float roughness)
    {
        (void)ambient;
        shader.setBool("useInstancing", false);
        shader.setMat4("model", b.GetModelMatrix());
        Material material = Material::FromLegacy(color, emissive, shininess, specular);
        material.metallic = metallic;
        material.roughness = roughness;
        material.ambientOcclusion = 1.0f;
        material.tangentSpaceValid = true;
        material.Apply(shader);
        unitBox.Draw();
    };

    // The imported monitor casing is shared by all computers. Its rendering
    // replaces only the visual casing; the original BoxInstance still owns
    // collision, interaction AABBs, screen placement, lighting and audio.
    if (m_monitorModel && m_monitorModel->IsLoaded())
    {
        glm::mat4 monitorTransform = glm::translate(glm::mat4(1.0f), m_monitorCasing.position);
        monitorTransform = glm::rotate(monitorTransform, m_facingYRadians,
                                       glm::vec3(0.0f, 1.0f, 0.0f));
        m_monitorModel->Draw(shader, monitorTransform);
    }
    else
    {
        drawBox(m_monitorCasing, m_monitorCasing.color, glm::vec3(0.0f),
                24.0f, 0.05f, glm::vec3(0.2f), 0.0f, 0.78f);
    }

    glm::vec3 offColor(0.02f, 0.02f, 0.025f);
    glm::vec3 screenColor = glm::mix(offColor, m_desktopColour, m_screenBrightness);
    glm::vec3 screenEmissive = m_desktopColour * m_screenBrightness * 2.2f;
    drawBox(m_screenPanel, screenColor, screenEmissive,
            96.0f, 0.08f, glm::vec3(0.6f), 0.0f, 0.16f);

    drawBox(m_keyboard, m_keyboard.color, glm::vec3(0.0f),
            24.0f, 0.05f, glm::vec3(0.25f), 0.0f, 0.72f);
    glm::vec3 kbLedEmissive = m_keyboardLedOn
        ? glm::vec3(0.15f, 0.6f, 0.95f) * 2.4f : glm::vec3(0.0f);
    drawBox(m_keyboardLed, m_keyboardLed.color, kbLedEmissive,
            32.0f, 0.05f, glm::vec3(0.4f), 0.0f, 0.22f);

    drawBox(m_mouse, m_mouse.color, glm::vec3(0.0f),
            24.0f, 0.05f, glm::vec3(0.25f), 0.0f, 0.70f);
    drawBox(m_cpuCase, m_cpuCase.color, glm::vec3(0.0f),
            8.0f, 0.05f, glm::vec3(0.04f), 0.0f, 0.92f);
    glm::vec3 cpuLedEmissive = m_cpuIndicatorOn
        ? glm::vec3(0.15f, 0.85f, 0.25f) * 2.2f : glm::vec3(0.0f);
    drawBox(m_cpuLed, m_cpuLed.color, cpuLedEmissive,
            32.0f, 0.05f, glm::vec3(0.4f), 0.0f, 0.22f);

    if (m_screenBrightness > 0.005f)
        DrawScreenContent(shader, unitBox);
}

// ============================================================================
// Interactable AABB(s) / lighting
// ============================================================================
glm::vec3 Computer::GetAABBMin() const
{
    glm::vec3 lo = m_monitorCasing.GetWorldMin();
    lo = glm::min(lo, m_keyboard.GetWorldMin());
    lo = glm::min(lo, m_mouse.GetWorldMin());
    lo = glm::min(lo, m_cpuCase.GetWorldMin());
    return lo;
}

glm::vec3 Computer::GetAABBMax() const
{
    glm::vec3 hi = m_monitorCasing.GetWorldMax();
    hi = glm::max(hi, m_keyboard.GetWorldMax());
    hi = glm::max(hi, m_mouse.GetWorldMax());
    hi = glm::max(hi, m_cpuCase.GetWorldMax());
    return hi;
}

void Computer::GetPartAABB(int partIndex, glm::vec3& outMin, glm::vec3& outMax) const
{
    const BoxInstance* part = nullptr;
    switch (partIndex)
    {
        case 0: part = &m_monitorCasing; break;
        case 1: part = &m_keyboard;      break;
        case 2: part = &m_mouse;         break;
        case 3: part = &m_cpuCase;       break;
        default: part = &m_monitorCasing; break;
    }
    outMin = part->GetWorldMin();
    outMax = part->GetWorldMax();
}

glm::vec3 Computer::GetScreenWorldPosition() const
{
    return m_screenPanel.position + glm::vec3(0.0f, m_screenPanel.scale.y * 0.6f, 0.0f);
}

glm::vec3 Computer::GetScreenLightColor() const
{
    // A brightened version of this workstation's own desktop color, so the
    // point light it casts is colored consistently with its screen - and,
    // per Phase 4 requirement 6, this is exactly zero whenever
    // m_screenBrightness is zero (i.e. fully Off), which is what
    // Classroom::GetLights() uses (via IsScreenLit()) to decide whether to
    // add a point light for this computer at all.
    glm::vec3 glow = glm::mix(m_desktopColour, glm::vec3(1.0f), 0.3f);
    return glow * m_screenBrightness;
}


glm::vec3 Computer::GetSoundPosition() const
{
    return (GetAABBMin() + GetAABBMax()) * 0.5f;
}
