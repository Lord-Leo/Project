#include "Projector.h"
#include "Material.h"
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <vector>

namespace
{
float SmoothStep01(float t)
{
    t = glm::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

void SetMaterial(const Shader& shader, const glm::mat4& model,
                 const glm::vec3& color, const glm::vec3& emissive,
                 float shininess, float ambient, const glm::vec3& specular,
                 float alpha = 1.0f, float metallicOverride = -1.0f,
                 float roughnessOverride = -1.0f)
{
    (void)ambient;
    shader.setBool("useInstancing", false);
    shader.setMat4("model", model);
    Material material = Material::FromLegacy(color, emissive, shininess, specular, alpha);
    if (metallicOverride >= 0.0f) material.metallic = metallicOverride;
    if (roughnessOverride >= 0.0f) material.roughness = roughnessOverride;
    material.Apply(shader);
}
}

Projector::Projector(const glm::vec3& ceilingMountPosition, const glm::vec3& whiteboardCenter,
                     const glm::vec3& whiteboardSize, float roomFacingYRadians)
{
    // A zero yaw faces the existing front wall along world -Z. Rotating the
    // local -Z and +X axes here gives one consistent world-space basis for
    // every piece of front-mounted projector hardware.
    m_frontDirection = glm::normalize(glm::vec3(-std::sin(roomFacingYRadians), 0.0f,
                                                 -std::cos(roomFacingYRadians)));
    m_rightDirection = glm::normalize(glm::vec3( std::cos(roomFacingYRadians), 0.0f,
                                                 -std::sin(roomFacingYRadians)));

    m_bodyBox.position = ceilingMountPosition;
    m_bodyBox.scale = glm::vec3(0.42f, 0.16f, 0.32f);
    m_bodyBox.color = glm::vec3(0.08f, 0.08f, 0.09f);
    m_bodyBox.rotationY = roomFacingYRadians;

    // The unit-box position is its bottom centre. The lens is mounted on the
    // projector's derived front face (toward -Z when yaw is zero).
    m_lensBox.position = ceilingMountPosition + glm::vec3(0.0f, 0.035f, 0.0f)
                       + m_frontDirection * 0.175f;
    m_lensBox.scale = glm::vec3(0.10f, 0.09f, 0.045f);
    m_lensBox.color = glm::vec3(0.025f, 0.03f, 0.04f);
    m_lensBox.rotationY = roomFacingYRadians;

    m_powerLed.position = ceilingMountPosition + glm::vec3(0.0f, 0.050f, 0.0f)
                        + m_rightDirection * 0.13f + m_frontDirection * 0.165f;
    m_powerLed.scale = glm::vec3(0.028f, 0.020f, 0.012f);
    m_powerLed.color = glm::vec3(0.07f, 0.015f, 0.012f);
    m_powerLed.rotationY = roomFacingYRadians;

    m_coolingLed.position = ceilingMountPosition + glm::vec3(0.0f, 0.050f, 0.0f)
                          + m_rightDirection * 0.08f + m_frontDirection * 0.165f;
    m_coolingLed.scale = glm::vec3(0.020f, 0.020f, 0.012f);
    m_coolingLed.color = glm::vec3(0.03f, 0.025f, 0.01f);
    m_coolingLed.rotationY = roomFacingYRadians;

    // A dedicated, thin panel slightly in front of the board. The supplied
    // whiteboardCenter is the visual centre, while BoxInstance positions are
    // bottom-centres, hence the half-height subtraction here.
    m_projectionWidth = whiteboardSize.x * 0.78f;
    m_projectionHeight = whiteboardSize.y * 0.72f;
    m_projectionCenter = whiteboardCenter + glm::vec3(0.0f, 0.0f, 0.055f);
    m_projectionPanel.position = glm::vec3(m_projectionCenter.x,
                                            m_projectionCenter.y - m_projectionHeight * 0.5f,
                                            m_projectionCenter.z);
    m_projectionPanel.scale = glm::vec3(m_projectionWidth, m_projectionHeight, 0.012f);
    m_projectionPanel.color = glm::vec3(0.015f, 0.018f, 0.025f);

    glm::vec3 lensCenter = m_lensBox.position + glm::vec3(0.0f, m_lensBox.scale.y * 0.5f, 0.0f);
    glm::vec3 beamNear = lensCenter + m_frontDirection * 0.04f;
    glm::vec3 beamFar = m_projectionCenter + glm::vec3(0.0f, 0.0f, 0.12f);
    m_beamMesh = CreateBeamMesh(beamNear, beamFar,
                                0.045f, 0.035f,
                                m_projectionWidth * 0.48f, m_projectionHeight * 0.48f);
}

Projector::~Projector()
{
    delete m_beamMesh;
}

Mesh* Projector::CreateBeamMesh(const glm::vec3& nearCenter, const glm::vec3& farCenter,
                                float nearHalfWidth, float nearHalfHeight,
                                float farHalfWidth, float farHalfHeight)
{
    // The classroom projector and whiteboard are axis-aligned in Phase 5,
    // so an XY near/far rectangle gives a lightweight four-sided frustum.
    glm::vec3 n[4] = {
        nearCenter + glm::vec3(-nearHalfWidth, -nearHalfHeight, 0.0f),
        nearCenter + glm::vec3( nearHalfWidth, -nearHalfHeight, 0.0f),
        nearCenter + glm::vec3( nearHalfWidth,  nearHalfHeight, 0.0f),
        nearCenter + glm::vec3(-nearHalfWidth,  nearHalfHeight, 0.0f)
    };
    glm::vec3 f[4] = {
        farCenter + glm::vec3(-farHalfWidth, -farHalfHeight, 0.0f),
        farCenter + glm::vec3( farHalfWidth, -farHalfHeight, 0.0f),
        farCenter + glm::vec3( farHalfWidth,  farHalfHeight, 0.0f),
        farCenter + glm::vec3(-farHalfWidth,  farHalfHeight, 0.0f)
    };

    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    vertices.reserve(16);
    indices.reserve(24);

    for (int side = 0; side < 4; ++side)
    {
        int next = (side + 1) % 4;
        glm::vec3 edgeA = f[side] - n[side];
        glm::vec3 edgeB = n[next] - n[side];
        glm::vec3 normal = glm::normalize(glm::cross(edgeA, edgeB));
        unsigned int base = (unsigned int)vertices.size();
        vertices.push_back({n[side], normal, glm::vec2(0.0f, 0.0f)});
        vertices.push_back({n[next], normal, glm::vec2(1.0f, 0.0f)});
        vertices.push_back({f[next], normal, glm::vec2(1.0f, 1.0f)});
        vertices.push_back({f[side], normal, glm::vec2(0.0f, 1.0f)});
        indices.insert(indices.end(), {base, base + 1, base + 2, base + 2, base + 3, base});
    }

    return new Mesh(vertices, indices);
}

void Projector::EnterStarting()
{
    m_state = ProjectorState::Starting;
    m_stateTimer = 0.0f;
    m_startupProgress = 0.0f;
    m_shutdownProgress = 0.0f;
}

void Projector::EnterShuttingDown()
{
    m_state = ProjectorState::ShuttingDown;
    m_stateTimer = 0.0f;
    m_shutdownProgress = 0.0f;
    m_startupProgress = 1.0f;
}

bool Projector::TogglePower()
{
    // Only stable states accept commands. Repeated input during Starting or
    // ShuttingDown is intentionally ignored, protecting all timers/curves.
    if (m_state == ProjectorState::Off)
    {
        EnterStarting();
        return true;
    }
    if (m_state == ProjectorState::Active)
    {
        EnterShuttingDown();
        return true;
    }
    return false;
}

void Projector::Interact()
{
    (void)TogglePower();
}

bool Projector::InteractWithResult()
{
    return TogglePower();
}

void Projector::CycleProjectionMode()
{
    if (m_state != ProjectorState::Active)
        return;

    switch (m_projectionMode)
    {
        case ProjectionMode::Presentation: m_projectionMode = ProjectionMode::BlankWhite; break;
        case ProjectionMode::BlankWhite:   m_projectionMode = ProjectionMode::ColourBars; break;
        case ProjectionMode::ColourBars:   m_projectionMode = ProjectionMode::Presentation; break;
    }
}

void Projector::ForceOff()
{
    m_state = ProjectorState::Off;
    m_stateTimer = 0.0f;
    m_startupProgress = 0.0f;
    m_shutdownProgress = 0.0f;
    m_lampBrightness = 0.0f;
    m_projectionBrightness = 0.0f;
    m_fanIntensity = 0.0f;
}

void Projector::Update(float deltaTime)
{
    switch (m_state)
    {
        case ProjectorState::Off:
            m_stateTimer = 0.0f;
            m_startupProgress = 0.0f;
            m_shutdownProgress = 0.0f;
            m_lampBrightness = 0.0f;
            m_projectionBrightness = 0.0f;
            m_fanIntensity = 0.0f;
            break;

        case ProjectorState::Starting:
            UpdateStarting(deltaTime);
            break;

        case ProjectorState::Active:
            m_stateTimer += deltaTime;
            m_startupProgress = 1.0f;
            m_shutdownProgress = 0.0f;
            m_lampBrightness = 1.0f;
            m_projectionBrightness = 1.0f;
            m_fanIntensity = 0.70f;
            break;

        case ProjectorState::ShuttingDown:
            UpdateShuttingDown(deltaTime);
            break;
    }
}

void Projector::UpdateStarting(float deltaTime)
{
    m_stateTimer += deltaTime;
    m_startupProgress = glm::clamp(m_stateTimer / kStartupDuration, 0.0f, 1.0f);
    float p = m_startupProgress;

    if (p < 0.15f)
    {
        m_lampBrightness = 0.03f * SmoothStep01(p / 0.15f);
        m_projectionBrightness = 0.0f;
        m_fanIntensity = glm::mix(0.0f, 0.25f, SmoothStep01(p / 0.15f));
    }
    else if (p < 0.45f)
    {
        float t = SmoothStep01((p - 0.15f) / 0.30f);
        m_lampBrightness = glm::mix(0.03f, 0.55f, t);
        m_projectionBrightness = glm::mix(0.0f, 0.16f, t);
        m_fanIntensity = glm::mix(0.25f, 0.55f, t);
    }
    else if (p < 0.80f)
    {
        float t = SmoothStep01((p - 0.45f) / 0.35f);
        m_lampBrightness = glm::mix(0.55f, 0.90f, t);
        m_projectionBrightness = glm::mix(0.16f, 0.68f, t);
        m_fanIntensity = glm::mix(0.55f, 0.72f, t);
    }
    else
    {
        float t = SmoothStep01((p - 0.80f) / 0.20f);
        m_lampBrightness = glm::mix(0.90f, 1.0f, t);
        m_projectionBrightness = glm::mix(0.68f, 1.0f, t);
        m_fanIntensity = glm::mix(0.72f, 0.70f, t);
    }

    if (m_startupProgress >= 1.0f)
    {
        m_state = ProjectorState::Active;
        m_stateTimer = 0.0f;
        m_lampBrightness = 1.0f;
        m_projectionBrightness = 1.0f;
        m_fanIntensity = 0.70f;
    }
}

void Projector::UpdateShuttingDown(float deltaTime)
{
    m_stateTimer += deltaTime;
    m_shutdownProgress = glm::clamp(m_stateTimer / kShutdownDuration, 0.0f, 1.0f);
    float p = m_shutdownProgress;

    // Content falls away smoothly, then the lamp completes its fade while
    // the cooling fan/indicator remain alive until the very end.
    m_projectionBrightness = 1.0f - SmoothStep01(glm::clamp(p / 0.68f, 0.0f, 1.0f));
    m_lampBrightness = 1.0f - SmoothStep01(glm::clamp(p / 0.85f, 0.0f, 1.0f));
    m_fanIntensity = (p < 0.72f)
        ? glm::mix(0.70f, 1.0f, SmoothStep01(p / 0.72f))
        : glm::mix(1.0f, 0.0f, SmoothStep01((p - 0.72f) / 0.28f));

    if (m_shutdownProgress >= 1.0f)
        ForceOff();
}

std::string Projector::GetPrompt() const
{
    switch (m_state)
    {
        case ProjectorState::Off:          return "Press E to turn on projector";
        case ProjectorState::Starting:     return "Projector is starting...";
        case ProjectorState::Active:       return "Press E to turn off projector";
        case ProjectorState::ShuttingDown: return "Projector is cooling down...";
    }
    return "";
}

std::string Projector::GetStateName() const
{
    switch (m_state)
    {
        case ProjectorState::Off:          return "OFF";
        case ProjectorState::Starting:     return "STARTING";
        case ProjectorState::Active:       return "ACTIVE";
        case ProjectorState::ShuttingDown: return "COOLING";
    }
    return "UNKNOWN";
}

std::string Projector::GetProjectionModeName() const
{
    switch (m_projectionMode)
    {
        case ProjectionMode::Presentation: return "PRESENTATION";
        case ProjectionMode::BlankWhite:   return "BLANK WHITE";
        case ProjectionMode::ColourBars:   return "COLOUR BARS";
    }
    return "UNKNOWN";
}

glm::vec3 Projector::GetLightWorldPosition() const
{
    return m_lensBox.position + glm::vec3(0.0f, m_lensBox.scale.y * 0.5f, 0.0f)
         + m_frontDirection * 0.06f;
}

glm::vec3 Projector::GetAABBMin() const
{
    glm::vec3 lo = glm::min(m_bodyBox.GetWorldMin(), m_lensBox.GetWorldMin());
    lo = glm::min(lo, m_powerLed.GetWorldMin());
    return glm::min(lo, m_coolingLed.GetWorldMin());
}

glm::vec3 Projector::GetAABBMax() const
{
    glm::vec3 hi = glm::max(m_bodyBox.GetWorldMax(), m_lensBox.GetWorldMax());
    hi = glm::max(hi, m_powerLed.GetWorldMax());
    return glm::max(hi, m_coolingLed.GetWorldMax());
}

void Projector::DrawHardware(const Shader& shader, const Mesh& unitBox) const
{
    SetMaterial(shader, m_bodyBox.GetModelMatrix(), m_bodyBox.color, glm::vec3(0.0f),
                48.0f, 0.05f, glm::vec3(0.30f), 1.0f, 0.62f, 0.42f);
    unitBox.Draw();

    glm::vec3 lensGlow = glm::vec3(0.72f, 0.80f, 1.0f) * m_lampBrightness * 3.2f;
    SetMaterial(shader, m_lensBox.GetModelMatrix(), m_lensBox.color, lensGlow,
                96.0f, 0.04f, glm::vec3(0.65f));
    unitBox.Draw();

    bool blink = std::fmod(m_stateTimer, 0.55f) < 0.275f;
    glm::vec3 powerColor(0.12f, 0.01f, 0.01f);
    glm::vec3 powerGlow(0.0f);
    if (m_state == ProjectorState::Off)
    {
        powerColor = glm::vec3(0.10f, 0.015f, 0.012f);
        powerGlow = glm::vec3(0.10f, 0.01f, 0.01f);
    }
    else if (m_state == ProjectorState::Starting)
    {
        powerColor = glm::vec3(0.75f, 0.38f, 0.05f);
        powerGlow = blink ? glm::vec3(0.95f, 0.42f, 0.06f) : glm::vec3(0.12f, 0.05f, 0.01f);
    }
    else if (m_state == ProjectorState::Active)
    {
        powerColor = glm::vec3(0.08f, 0.65f, 0.22f);
        powerGlow = glm::vec3(0.08f, 0.85f, 0.28f);
    }
    else
    {
        powerColor = glm::vec3(0.75f, 0.38f, 0.05f);
        powerGlow = blink ? glm::vec3(0.95f, 0.42f, 0.06f) : glm::vec3(0.12f, 0.05f, 0.01f);
    }

    SetMaterial(shader, m_powerLed.GetModelMatrix(), powerColor, powerGlow,
                24.0f, 0.04f, glm::vec3(0.25f));
    unitBox.Draw();

    glm::vec3 coolingColor = glm::mix(glm::vec3(0.035f, 0.025f, 0.01f),
                                      glm::vec3(0.15f, 0.45f, 0.95f), m_fanIntensity);
    glm::vec3 coolingGlow = glm::vec3(0.10f, 0.38f, 0.95f) * m_fanIntensity;
    SetMaterial(shader, m_coolingLed.GetModelMatrix(), coolingColor, coolingGlow,
                24.0f, 0.04f, glm::vec3(0.25f));
    unitBox.Draw();
}

void Projector::DrawProjectionElement(const Shader& shader, const Mesh& unitBox,
                                      float u, float v, float width, float height,
                                      const glm::vec3& color, float emissiveScale,
                                      float depthOffset) const
{
    BoxInstance element;
    element.position = glm::vec3(m_projectionCenter.x + u,
                                 m_projectionCenter.y + v - height * 0.5f,
                                 m_projectionCenter.z + depthOffset);
    element.scale = glm::vec3(width, height, 0.006f);
    SetMaterial(shader, element.GetModelMatrix(), color * m_projectionBrightness,
                color * emissiveScale * m_projectionBrightness,
                8.0f, 0.02f, glm::vec3(0.05f));
    unitBox.Draw();
}

void Projector::DrawStartupContent(const Shader& shader, const Mesh& unitBox, float brightness) const
{
    if (m_startupProgress < 0.42f)
        return;

    glm::vec3 accent(0.25f, 0.55f, 0.95f);
    float appear = SmoothStep01((m_startupProgress - 0.42f) / 0.25f) * brightness;

    // Abstract, non-branded academic mark: four blocks around a centre.
    const float d = 0.16f;
    DrawProjectionElement(shader, unitBox, -d,  d, 0.18f, 0.18f, accent, 0.8f * appear);
    DrawProjectionElement(shader, unitBox,  d,  d, 0.18f, 0.18f, glm::vec3(0.20f, 0.75f, 0.65f), 0.8f * appear);
    DrawProjectionElement(shader, unitBox, -d, -d, 0.18f, 0.18f, glm::vec3(0.85f, 0.55f, 0.16f), 0.8f * appear);
    DrawProjectionElement(shader, unitBox,  d, -d, 0.18f, 0.18f, glm::vec3(0.65f, 0.30f, 0.75f), 0.8f * appear);

    float pulse = 0.45f + 0.55f * std::max(0.0f, std::sin(m_stateTimer * 4.5f));
    for (int i = 0; i < 3; ++i)
    {
        float x = -0.18f + i * 0.18f;
        DrawProjectionElement(shader, unitBox, x, -0.48f, 0.07f, 0.05f,
                              glm::vec3(0.72f), 0.45f * appear * (i == 1 ? pulse : 0.65f));
    }
}

void Projector::DrawPresentation(const Shader& shader, const Mesh& unitBox, float brightness) const
{
    (void)brightness;
    const float w = m_projectionWidth;
    const float h = m_projectionHeight;

    // Clean projected welcome slide.  The word is geometry (5x7 pixel
    // glyphs), so it works without an external font/texture dependency.
    glm::vec3 slideBg(0.93f, 0.95f, 0.99f);
    glm::vec3 title(0.08f, 0.30f, 0.72f);
    glm::vec3 accent(0.20f, 0.55f, 0.92f);
    DrawProjectionElement(shader, unitBox, 0.0f, 0.0f,
                          w * 0.985f, h * 0.97f, slideBg, 0.50f, 0.012f);

    static const char* glyphs[7] = {
        "1000111111100001000011111100001000111111", // row 0
        "1000110000100001000010000100001000110000", // row 1
        "1000110000100001000010000100001101110000", // row 2
        "1010111110100001000010000100001010111110", // row 3
        "1010110000100001000010000100001000110000", // row 4
        "1101110000100001000010000100001000110000", // row 5
        "1000111111111111111101111011111000111111"  // row 6
    };
    // Above rows contain 7 glyphs, each 5 pixels plus one separator:
    // W E L C O M E. Build from explicit patterns for readability.
    static const char* letters[7][7] = {
        {"10001","10001","10001","10101","10101","11011","10001"},
        {"11111","10000","10000","11110","10000","10000","11111"},
        {"10000","10000","10000","10000","10000","10000","11111"},
        {"11111","10000","10000","10000","10000","10000","11111"},
        {"01110","10001","10001","10001","10001","10001","01110"},
        {"10001","11011","10101","10101","10001","10001","10001"},
        {"11111","10000","10000","11110","10000","10000","11111"}
    };
    (void)glyphs;

    const float pixel = std::min(w / 54.0f, h / 13.0f);
    const float glyphWidth = 5.0f * pixel;
    const float gap = 1.25f * pixel;
    const float totalWidth = 7.0f * glyphWidth + 6.0f * gap;
    const float startX = -totalWidth * 0.5f + pixel * 0.5f;
    const float topY = 3.2f * pixel;

    for (int letter = 0; letter < 7; ++letter)
    {
        float letterX = startX + letter * (glyphWidth + gap);
        for (int row = 0; row < 7; ++row)
            for (int col = 0; col < 5; ++col)
                if (letters[letter][row][col] == '1')
                    DrawProjectionElement(shader, unitBox,
                        letterX + col * pixel,
                        topY - row * pixel,
                        pixel * 0.82f, pixel * 0.82f,
                        title, 0.78f, 0.020f);
    }

    // Simple underline/accent gives the slide a finished presentation look.
    DrawProjectionElement(shader, unitBox, 0.0f, -h * 0.24f,
                          w * 0.42f, h * 0.018f, accent, 0.48f, 0.018f);
}

void Projector::DrawColourBars(const Shader& shader, const Mesh& unitBox, float brightness) const
{
    (void)brightness;
    static const glm::vec3 colors[7] = {
        glm::vec3(0.95f, 0.95f, 0.90f), glm::vec3(0.95f, 0.90f, 0.10f),
        glm::vec3(0.10f, 0.85f, 0.85f), glm::vec3(0.10f, 0.80f, 0.20f),
        glm::vec3(0.85f, 0.12f, 0.70f), glm::vec3(0.85f, 0.12f, 0.12f),
        glm::vec3(0.12f, 0.22f, 0.90f)
    };
    float barW = m_projectionWidth * 0.138f;
    for (int i = 0; i < 7; ++i)
    {
        float u = -m_projectionWidth * 0.414f + barW * i;
        DrawProjectionElement(shader, unitBox, u, 0.08f, barW, m_projectionHeight * 0.78f,
                              colors[i], 0.52f, 0.014f);
    }
    DrawProjectionElement(shader, unitBox, 0.0f, -m_projectionHeight * 0.40f,
                          m_projectionWidth * 0.97f, m_projectionHeight * 0.12f,
                          glm::vec3(0.04f), 0.25f, 0.016f);
}

void Projector::DrawProjection(const Shader& shader, const Mesh& unitBox) const
{
    if (m_projectionBrightness <= 0.003f)
        return;

    glm::vec3 baseColor;
    if (m_state == ProjectorState::Starting && m_startupProgress < 0.80f)
        baseColor = glm::vec3(0.018f, 0.025f, 0.045f);
    else if (m_projectionMode == ProjectionMode::BlankWhite)
        baseColor = glm::vec3(0.92f, 0.94f, 0.98f);
    else
        baseColor = glm::vec3(0.04f, 0.055f, 0.085f);

    SetMaterial(shader, m_projectionPanel.GetModelMatrix(),
                baseColor * m_projectionBrightness,
                baseColor * (0.75f * m_projectionBrightness),
                8.0f, 0.02f, glm::vec3(0.04f));
    unitBox.Draw();

    if (m_state == ProjectorState::Starting && m_startupProgress < 0.80f)
    {
        DrawStartupContent(shader, unitBox, m_projectionBrightness);
        return;
    }

    switch (m_projectionMode)
    {
        case ProjectionMode::Presentation:
            DrawPresentation(shader, unitBox, m_projectionBrightness);
            break;
        case ProjectionMode::BlankWhite:
            // The bright dedicated base panel is the complete blank screen.
            break;
        case ProjectionMode::ColourBars:
            DrawColourBars(shader, unitBox, m_projectionBrightness);
            break;
    }
}

void Projector::DrawBeam(const Shader& shader) const
{
    if (!m_beamMesh || m_lampBrightness <= 0.01f)
        return;

    // During late cooldown the lamp has already faded and the beam naturally
    // disappears. It remains visible in Starting, Active and early shutdown.
    float alpha = 0.10f * m_lampBrightness;
    if (m_state == ProjectorState::ShuttingDown)
        alpha *= 1.0f - SmoothStep01(glm::clamp((m_shutdownProgress - 0.55f) / 0.30f, 0.0f, 1.0f));
    if (alpha <= 0.002f)
        return;

    // Save every OpenGL state touched by the translucent pass. Separate
    // RGB/alpha blend factors are queried and restored so the beam cannot
    // leak transparency into any later computer, switch, door or HUD draw.
    const GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
    const GLboolean cullWasEnabled = glIsEnabled(GL_CULL_FACE);
    GLboolean previousDepthMask = GL_TRUE;
    GLint previousBlendSrcRgb = GL_ONE;
    GLint previousBlendDstRgb = GL_ZERO;
    GLint previousBlendSrcAlpha = GL_ONE;
    GLint previousBlendDstAlpha = GL_ZERO;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &previousDepthMask);
    glGetIntegerv(GL_BLEND_SRC_RGB, &previousBlendSrcRgb);
    glGetIntegerv(GL_BLEND_DST_RGB, &previousBlendDstRgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &previousBlendSrcAlpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &previousBlendDstAlpha);

    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
                        GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);

    SetMaterial(shader, glm::mat4(1.0f),
                glm::vec3(0.18f, 0.30f, 0.55f) * m_lampBrightness,
                glm::vec3(0.20f, 0.35f, 0.70f) * m_lampBrightness,
                2.0f, 0.0f, glm::vec3(0.0f), alpha, 0.0f, 1.0f);
    // SetMaterial applies the requested alpha to both legacy and PBR paths.
    // Reasserting it here makes the beam invariant explicit before drawing.
    shader.setFloat("objectAlpha", alpha);
    m_beamMesh->Draw();

    // Restore shader and fixed-function state before any later opaque draw.
    shader.setFloat("objectAlpha", 1.0f);
    glDepthMask(previousDepthMask);
    if (cullWasEnabled) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    glBlendFuncSeparate(previousBlendSrcRgb, previousBlendDstRgb,
                        previousBlendSrcAlpha, previousBlendDstAlpha);
    if (blendWasEnabled) glEnable(GL_BLEND); else glDisable(GL_BLEND);
}

void Projector::Draw(const Shader& shader, const Mesh& unitBox, bool includeVisualEffects) const
{
    DrawHardware(shader, unitBox);
    if (!includeVisualEffects)
        return;

    DrawProjection(shader, unitBox);
    DrawBeam(shader);
}

ProjectorPowerSwitch::ProjectorPowerSwitch(const glm::vec3& wallPosition,
                                           float facingYRadians,
                                           Projector* projector)
    : m_projector(projector)
{
    const glm::vec3 normal = glm::normalize(glm::vec3(std::sin(facingYRadians), 0.0f,
                                                       std::cos(facingYRadians)));

    m_plate.position = wallPosition;
    m_plate.scale = glm::vec3(0.26f, 0.24f, 0.035f);
    m_plate.rotationY = facingYRadians;
    m_plate.color = glm::vec3(0.72f, 0.74f, 0.78f);

    m_button.position = wallPosition + glm::vec3(0.0f, 0.055f, 0.0f) + normal * 0.024f;
    m_button.scale = glm::vec3(0.11f, 0.095f, 0.035f);
    m_button.rotationY = facingYRadians;
    m_button.color = glm::vec3(0.09f, 0.11f, 0.15f);

    m_statusLed.position = wallPosition + glm::vec3(0.075f, 0.175f, 0.0f) + normal * 0.025f;
    m_statusLed.scale = glm::vec3(0.035f, 0.025f, 0.018f);
    m_statusLed.rotationY = facingYRadians;
    m_statusLed.color = glm::vec3(0.08f, 0.015f, 0.012f);
}

void ProjectorPowerSwitch::Draw(const Shader& shader, const Mesh& unitBox) const
{
    SetMaterial(shader, m_plate.GetModelMatrix(), m_plate.color, glm::vec3(0.0f),
                20.0f, 0.06f, glm::vec3(0.28f));
    unitBox.Draw();

    glm::vec3 buttonColor = m_button.color;
    if (m_projector && m_projector->GetState() == ProjectorState::Active)
        buttonColor = glm::vec3(0.08f, 0.18f, 0.12f);
    SetMaterial(shader, m_button.GetModelMatrix(), buttonColor, glm::vec3(0.0f),
                24.0f, 0.04f, glm::vec3(0.35f));
    unitBox.Draw();

    glm::vec3 ledColor(0.10f, 0.015f, 0.012f);
    glm::vec3 ledGlow(0.06f, 0.005f, 0.003f);
    if (m_projector)
    {
        switch (m_projector->GetState())
        {
            case ProjectorState::Off:
                break;
            case ProjectorState::Starting:
            case ProjectorState::ShuttingDown:
                ledColor = glm::vec3(0.78f, 0.38f, 0.05f);
                ledGlow = glm::vec3(0.90f, 0.34f, 0.04f);
                break;
            case ProjectorState::Active:
                ledColor = glm::vec3(0.06f, 0.68f, 0.22f);
                ledGlow = glm::vec3(0.06f, 0.95f, 0.28f);
                break;
        }
    }
    SetMaterial(shader, m_statusLed.GetModelMatrix(), ledColor, ledGlow,
                24.0f, 0.04f, glm::vec3(0.25f));
    unitBox.Draw();
}

glm::vec3 ProjectorPowerSwitch::GetAABBMin() const
{
    glm::vec3 lo = glm::min(m_plate.GetWorldMin(), m_button.GetWorldMin());
    return glm::min(lo, m_statusLed.GetWorldMin());
}

glm::vec3 ProjectorPowerSwitch::GetAABBMax() const
{
    glm::vec3 hi = glm::max(m_plate.GetWorldMax(), m_button.GetWorldMax());
    return glm::max(hi, m_statusLed.GetWorldMax());
}

std::string ProjectorPowerSwitch::GetPrompt() const
{
    return m_projector ? m_projector->GetPrompt() : "Projector unavailable";
}

void ProjectorPowerSwitch::Interact()
{
    (void)InteractWithResult();
}

bool ProjectorPowerSwitch::InteractWithResult()
{
    return m_projector && m_projector->TogglePower();
}

ProjectorControlPanel::ProjectorControlPanel(const glm::vec3& deskSurfacePosition, float facingYRadians,
                                             Projector* projector, const Computer* teacherComputer)
    : m_projector(projector), m_teacherComputer(teacherComputer)
{
    m_panel.position = deskSurfacePosition;
    m_panel.scale = glm::vec3(0.34f, 0.055f, 0.24f);
    m_panel.rotationY = facingYRadians;
    m_panel.color = glm::vec3(0.055f, 0.06f, 0.075f);

    m_statusLed.position = deskSurfacePosition + glm::vec3(0.10f, 0.058f, 0.0f);
    m_statusLed.scale = glm::vec3(0.035f, 0.014f, 0.035f);
    m_statusLed.rotationY = facingYRadians;
    m_statusLed.color = glm::vec3(0.05f);
}

bool ProjectorControlPanel::IsAvailable() const
{
    return m_projector && m_teacherComputer &&
           m_teacherComputer->GetPowerState() == PowerState::Desktop;
}

void ProjectorControlPanel::Draw(const Shader& shader, const Mesh& unitBox) const
{
    SetMaterial(shader, m_panel.GetModelMatrix(), m_panel.color, glm::vec3(0.0f),
                32.0f, 0.05f, glm::vec3(0.35f));
    unitBox.Draw();

    glm::vec3 ledColor(0.10f, 0.02f, 0.02f);
    glm::vec3 ledGlow(0.0f);
    if (IsAvailable())
    {
        if (m_projector->GetState() == ProjectorState::Active)
        {
            ledColor = glm::vec3(0.08f, 0.65f, 0.22f);
            ledGlow = glm::vec3(0.08f, 0.85f, 0.28f);
        }
        else if (m_projector->GetState() == ProjectorState::Starting ||
                 m_projector->GetState() == ProjectorState::ShuttingDown)
        {
            ledColor = glm::vec3(0.75f, 0.38f, 0.05f);
            ledGlow = glm::vec3(0.65f, 0.30f, 0.04f);
        }
        else
        {
            ledColor = glm::vec3(0.12f, 0.30f, 0.75f);
            ledGlow = glm::vec3(0.10f, 0.28f, 0.75f);
        }
    }

    SetMaterial(shader, m_statusLed.GetModelMatrix(), ledColor, ledGlow,
                32.0f, 0.04f, glm::vec3(0.30f));
    unitBox.Draw();
}

glm::vec3 ProjectorControlPanel::GetAABBMin() const
{
    return glm::min(m_panel.GetWorldMin(), m_statusLed.GetWorldMin());
}

glm::vec3 ProjectorControlPanel::GetAABBMax() const
{
    return glm::max(m_panel.GetWorldMax(), m_statusLed.GetWorldMax());
}

std::string ProjectorControlPanel::GetPrompt() const
{
    if (!m_teacherComputer || m_teacherComputer->GetPowerState() == PowerState::Off)
        return "Teacher PC is off - projector control unavailable";
    if (m_teacherComputer->GetPowerState() == PowerState::Booting)
        return "Teacher PC is starting - projector control unavailable";
    if (m_teacherComputer->GetPowerState() == PowerState::ShuttingDown)
        return "Teacher PC is shutting down - projector control unavailable";

    // Desktop uses the shared Projector state machine's exact prompt. This
    // correctly reports Starting/Cooling instead of advertising an E action
    // that TogglePower() intentionally ignores during transitions.
    return m_projector ? m_projector->GetPrompt() : "Projector unavailable";
}

void ProjectorControlPanel::Interact()
{
    (void)InteractWithResult();
}

bool ProjectorControlPanel::InteractWithResult()
{
    return IsAvailable() && m_projector->TogglePower();
}
