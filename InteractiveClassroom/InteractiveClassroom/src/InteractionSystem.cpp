#include "InteractionSystem.h"
#include <algorithm>
#include <limits>

void InteractionSystem::SetInteractables(const std::vector<Interactable*>& interactables)
{
    m_interactables = interactables;
}

void InteractionSystem::AddInteractable(Interactable* interactable)
{
    m_interactables.push_back(interactable);
}

bool InteractionSystem::RayIntersectsAABB(const glm::vec3& origin, const glm::vec3& dir,
                                           const glm::vec3& boxMin, const glm::vec3& boxMax,
                                           float maxDistance, float& outT)
{
    float tmin = 0.0001f;
    float tmax = maxDistance;

    for (int axis = 0; axis < 3; ++axis)
    {
        float d = dir[axis];
        float originAxis = origin[axis];
        float minAxis = boxMin[axis];
        float maxAxis = boxMax[axis];

        if (std::abs(d) < 1e-8f)
        {
            // Ray parallel to this slab: must already be within it.
            if (originAxis < minAxis || originAxis > maxAxis)
                return false;
        }
        else
        {
            float t1 = (minAxis - originAxis) / d;
            float t2 = (maxAxis - originAxis) / d;
            if (t1 > t2) std::swap(t1, t2);

            tmin = std::max(tmin, t1);
            tmax = std::min(tmax, t2);

            if (tmin > tmax)
                return false;
        }
    }

    outT = tmin;
    return true;
}

RaycastHit InteractionSystem::Raycast(const glm::vec3& origin, const glm::vec3& direction, float maxDistance) const
{
    RaycastHit best;
    float bestT = std::numeric_limits<float>::max();

    for (Interactable* obj : m_interactables)
    {
        if (!obj) continue;

        int partCount = obj->GetPartCount();
        for (int part = 0; part < partCount; ++part)
        {
            glm::vec3 partMin, partMax;
            obj->GetPartAABB(part, partMin, partMax);

            float t;
            if (RayIntersectsAABB(origin, direction, partMin, partMax, maxDistance, t))
            {
                if (t < bestT)
                {
                    bestT = t;
                    best.hit = true;
                    best.distance = t;
                    best.object = obj; // always the owning object, never a per-part identity
                    best.partIndex = part;
                }
            }
        }
    }
    return best;
}

void InteractionSystem::UpdateAll(float deltaTime)
{
    for (Interactable* obj : m_interactables)
    {
        if (obj) obj->Update(deltaTime);
    }
}
