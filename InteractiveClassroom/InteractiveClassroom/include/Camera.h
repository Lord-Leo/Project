#ifndef CAMERA_H
#define CAMERA_H

#include <glm/glm.hpp>

enum class CameraMovement
{
    Forward,
    Backward,
    Left,
    Right
};

// A first-person camera driven by Euler angles (yaw/pitch). It does not move
// itself directly against the world - main.cpp asks it for a "desired"
// position after WASD input, runs that through the Classroom collision
// check, and only then commits the final position via SetPosition().
class Camera
{
public:
    Camera(glm::vec3 position, float yaw = -90.0f, float pitch = 0.0f);

    glm::mat4 GetViewMatrix() const;

    // Returns the position the camera WOULD be at if this movement were
    // applied, without actually moving the camera. Caller resolves
    // collision then calls SetPosition with the accepted result.
    glm::vec3 ComputeDesiredPosition(CameraMovement direction, float deltaTime, bool running) const;

    void SetPosition(const glm::vec3& pos);
    glm::vec3 GetPosition() const { return Position; }
    glm::vec3 GetFront() const { return Front; }
    glm::vec3 GetRight() const { return Right; }
    glm::vec3 GetUp() const { return Up; }

    // Front/Right projected onto the XZ plane and re-normalized - what
    // WASD movement should actually walk along (looking up/down must not
    // speed up or slow down horizontal movement).
    glm::vec3 GetFlatFront() const;
    glm::vec3 GetFlatRight() const;

    void ProcessMouseMovement(float xoffset, float yoffset, bool constrainPitch = true);

    float MovementSpeed;
    float RunMultiplier;
    float MouseSensitivity;
    float Yaw;
    float Pitch;

private:
    void updateCameraVectors();

    glm::vec3 Position;
    glm::vec3 Front;
    glm::vec3 Up;
    glm::vec3 Right;
    glm::vec3 WorldUp;
};

#endif // CAMERA_H
