#ifndef DOOR_H
#define DOOR_H

#include "InteractionSystem.h"
#include "Mesh.h"
#include "Shader.h"
#include <glm/glm.hpp>

// A swinging door hinged on one vertical edge. Interact() toggles between
// closed (0 degrees) and open (~100 degrees); the rotation is smoothed over
// time in Update() rather than snapping instantly.
class Door : public Interactable
{
public:
    // hingePosition: world-space point at the floor where the hinge edge is.
    // width/height/thickness: dimensions of the door slab.
    // openDirection: +1 or -1, which way the door swings.
    Door(const glm::vec3& hingePosition, float width, float height, float thickness, float openDirection);

    void Draw(const Shader& shader, const Mesh& unitBox) const;

    // Interactable
    glm::vec3 GetAABBMin() const override;
    glm::vec3 GetAABBMax() const override;
    std::string GetPrompt() const override;
    void Interact() override;
    bool InteractWithResult() override;
    void Update(float deltaTime) override;

    bool IsOpen() const { return m_isOpen; }
    bool IsMoving() const;
    glm::vec3 GetHingePosition() const { return m_hingePosition; }

private:
    glm::mat4 GetModelMatrix() const;

    glm::vec3 m_hingePosition;
    float m_width;
    float m_height;
    float m_thickness;
    float m_openDirection;

    bool m_isOpen = false;
    float m_currentAngle = 0.0f; // radians
    float m_targetAngle = 0.0f;  // radians
    static constexpr float kOpenAngle = 1.74f; // ~100 degrees
    static constexpr float kAngularSpeed = 3.0f; // radians/sec
};

#endif // DOOR_H
