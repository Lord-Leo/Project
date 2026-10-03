#include "Door.h"
#include "Material.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <limits>

Door::Door(const glm::vec3& hingePosition, float width, float height, float thickness, float openDirection)
    : m_hingePosition(hingePosition)
    , m_width(width)
    , m_height(height)
    , m_thickness(thickness)
    , m_openDirection(openDirection)
{
}

glm::mat4 Door::GetModelMatrix() const
{
    // Rotate about the hinge (at m_hingePosition), then push the slab out
    // by half its width so the hinge edge, not the center, is the pivot.
    glm::mat4 model = glm::translate(glm::mat4(1.0f), m_hingePosition);
    model = glm::rotate(model, m_currentAngle * m_openDirection, glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::translate(model, glm::vec3(m_width * 0.5f, 0.0f, 0.0f));
    model = glm::scale(model, glm::vec3(m_width, m_height, m_thickness));
    return model;
}

void Door::Draw(const Shader& shader, const Mesh& unitBox) const
{
    shader.setBool("useInstancing", false);
    shader.setMat4("model", GetModelMatrix());
    Material material = Material::FromLegacy(glm::vec3(0.45f, 0.30f, 0.18f),
                                             glm::vec3(0.0f), 32.0f,
                                             glm::vec3(0.2f));
    material.metallic = 0.0f;
    material.roughness = 0.56f;
    material.Apply(shader);
    unitBox.Draw();
}

glm::vec3 Door::GetAABBMin() const
{
    glm::mat4 model = GetModelMatrix();
    glm::vec3 lo(std::numeric_limits<float>::max());
    const float xs[2] = { -0.5f, 0.5f };
    const float ys[2] = { 0.0f, 1.0f };
    const float zs[2] = { -0.5f, 0.5f };
    for (float x : xs) for (float y : ys) for (float z : zs)
        lo = glm::min(lo, glm::vec3(model * glm::vec4(x, y, z, 1.0f)));
    return lo;
}

glm::vec3 Door::GetAABBMax() const
{
    glm::mat4 model = GetModelMatrix();
    glm::vec3 hi(std::numeric_limits<float>::lowest());
    const float xs[2] = { -0.5f, 0.5f };
    const float ys[2] = { 0.0f, 1.0f };
    const float zs[2] = { -0.5f, 0.5f };
    for (float x : xs) for (float y : ys) for (float z : zs)
        hi = glm::max(hi, glm::vec3(model * glm::vec4(x, y, z, 1.0f)));
    return hi;
}

std::string Door::GetPrompt() const
{
    if (IsMoving())
        return m_isOpen ? "Door is opening..." : "Door is closing...";
    return m_isOpen ? "Press E to Close Door" : "Press E to Open Door";
}

void Door::Interact()
{
    (void)InteractWithResult();
}

bool Door::InteractWithResult()
{
    // Do not reverse the door repeatedly while it is moving. This keeps the
    // physical animation deterministic and guarantees one open/close cue per
    // accepted transition instead of overlapping duplicates.
    if (IsMoving()) return false;

    m_isOpen = !m_isOpen;
    m_targetAngle = m_isOpen ? kOpenAngle : 0.0f;
    return true;
}

bool Door::IsMoving() const
{
    return std::abs(m_targetAngle - m_currentAngle) > 0.001f;
}

void Door::Update(float deltaTime)
{
    float diff = m_targetAngle - m_currentAngle;
    float maxStep = kAngularSpeed * deltaTime;
    if (std::abs(diff) <= maxStep)
        m_currentAngle = m_targetAngle;
    else
        m_currentAngle += (diff > 0.0f ? maxStep : -maxStep);
}
