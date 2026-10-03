#ifndef INTERACTION_SYSTEM_H
#define INTERACTION_SYSTEM_H

#include <glm/glm.hpp>
#include <string>
#include <vector>

// Common interface implemented by anything the player can point at and
// press E on: light switches, computers, the projector, and the door.
class Interactable
{
public:
    virtual ~Interactable() = default;

    // World-space axis-aligned bounding box used for the ray test. For a
    // multi-part object (see GetPartCount/GetPartAABB below) this should
    // still return the combined box of every part, kept for API
    // completeness/backward compatibility.
    virtual glm::vec3 GetAABBMin() const = 0;
    virtual glm::vec3 GetAABBMax() const = 0;

    // Text shown above the crosshair when this object is targeted, e.g.
    // "Press E to Turn On Light".
    virtual std::string GetPrompt() const = 0;

    // Called once when the player presses E while this object is targeted.
    virtual void Interact() = 0;

    // Same interaction with an acceptance result for audio/UI feedback.
    // Existing Phase 1-5 callers can keep using Interact(); Phase 6 uses this
    // to avoid click sounds when a state machine intentionally ignores input.
    virtual bool InteractWithResult() { Interact(); return true; }

    // Called every frame regardless of whether the object is targeted, so
    // it can animate (light fade, monitor boot, door swing, ...).
    virtual void Update(float deltaTime) { (void)deltaTime; }

    // Optional multi-part hit-testing (Phase 4 requirement 7): an
    // Interactable made of several distinct physical pieces (e.g. a
    // workstation's monitor/keyboard/mouse/CPU case) can expose each
    // piece's own AABB here instead of one large box that also covers the
    // empty space between them. InteractionSystem tests every part of
    // every Interactable and keeps whichever PART is hit nearest the
    // camera, but always resolves the hit back to the OWNING Interactable*
    // - so looking at any part shows the same prompt and interacts with
    // the same object, never flickering between parts. Objects that don't
    // override this default to exactly one part: their own combined
    // GetAABBMin()/GetAABBMax().
    virtual int GetPartCount() const { return 1; }
    virtual void GetPartAABB(int partIndex, glm::vec3& outMin, glm::vec3& outMax) const
    {
        (void)partIndex;
        outMin = GetAABBMin();
        outMax = GetAABBMax();
    }

    virtual glm::vec3 GetPartCenter(int partIndex) const
    {
        glm::vec3 partMin, partMax;
        GetPartAABB(partIndex, partMin, partMax);
        return (partMin + partMax) * 0.5f;
    }
};

struct RaycastHit
{
    bool hit = false;
    float distance = 0.0f;
    Interactable* object = nullptr;
    int partIndex = 0;
};

// Owns the list of interactables currently in the scene and answers "what
// is the player looking at right now" queries via simple ray-vs-AABB tests
// (slab method), picking whichever hit is closest along the ray.
class InteractionSystem
{
public:
    void SetInteractables(const std::vector<Interactable*>& interactables);
    void AddInteractable(Interactable* interactable);

    RaycastHit Raycast(const glm::vec3& origin, const glm::vec3& direction, float maxDistance) const;

    void UpdateAll(float deltaTime);

private:
    static bool RayIntersectsAABB(const glm::vec3& origin, const glm::vec3& dir,
                                   const glm::vec3& boxMin, const glm::vec3& boxMax,
                                   float maxDistance, float& outT);

    std::vector<Interactable*> m_interactables;
};

#endif // INTERACTION_SYSTEM_H
