#include "Camera.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

Camera::Camera(glm::vec3 position, float yaw, float pitch)
    : MovementSpeed(3.0f)
    , RunMultiplier(1.8f)
    , MouseSensitivity(0.1f)
    , Yaw(yaw)
    , Pitch(pitch)
    , Position(position)
    , Front(glm::vec3(0.0f, 0.0f, -1.0f))
    , Up(glm::vec3(0.0f, 1.0f, 0.0f))
    , Right(glm::vec3(1.0f, 0.0f, 0.0f))
    , WorldUp(glm::vec3(0.0f, 1.0f, 0.0f))
{
    updateCameraVectors();
}

glm::mat4 Camera::GetViewMatrix() const
{
    return glm::lookAt(Position, Position + Front, Up);
}

glm::vec3 Camera::ComputeDesiredPosition(CameraMovement direction, float deltaTime, bool running) const
{
    float speed = MovementSpeed * (running ? RunMultiplier : 1.0f) * deltaTime;

    // Movement is flattened to the XZ plane so WASD does not fly the player
    // up/down when looking up or down.
    glm::vec3 flatFront = glm::normalize(glm::vec3(Front.x, 0.0f, Front.z));
    glm::vec3 flatRight = glm::normalize(glm::vec3(Right.x, 0.0f, Right.z));

    glm::vec3 desired = Position;
    switch (direction)
    {
        case CameraMovement::Forward:  desired += flatFront * speed; break;
        case CameraMovement::Backward: desired -= flatFront * speed; break;
        case CameraMovement::Left:     desired -= flatRight * speed; break;
        case CameraMovement::Right:    desired += flatRight * speed; break;
    }
    return desired;
}

glm::vec3 Camera::GetFlatFront() const
{
    glm::vec3 f(Front.x, 0.0f, Front.z);
    if (glm::length(f) < 1e-6f) return glm::vec3(0.0f, 0.0f, -1.0f);
    return glm::normalize(f);
}

glm::vec3 Camera::GetFlatRight() const
{
    glm::vec3 r(Right.x, 0.0f, Right.z);
    if (glm::length(r) < 1e-6f) return glm::vec3(1.0f, 0.0f, 0.0f);
    return glm::normalize(r);
}

void Camera::SetPosition(const glm::vec3& pos)
{
    Position = pos;
}

void Camera::ProcessMouseMovement(float xoffset, float yoffset, bool constrainPitch)
{
    xoffset *= MouseSensitivity;
    yoffset *= MouseSensitivity;

    Yaw += xoffset;
    Pitch += yoffset;

    if (constrainPitch)
        Pitch = std::clamp(Pitch, -89.0f, 89.0f);

    updateCameraVectors();
}

void Camera::updateCameraVectors()
{
    glm::vec3 front;
    front.x = cos(glm::radians(Yaw)) * cos(glm::radians(Pitch));
    front.y = sin(glm::radians(Pitch));
    front.z = sin(glm::radians(Yaw)) * cos(glm::radians(Pitch));
    Front = glm::normalize(front);
    Right = glm::normalize(glm::cross(Front, WorldUp));
    Up    = glm::normalize(glm::cross(Right, Front));
}
