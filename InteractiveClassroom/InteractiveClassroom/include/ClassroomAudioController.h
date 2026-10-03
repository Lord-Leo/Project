#ifndef CLASSROOM_AUDIO_CONTROLLER_H
#define CLASSROOM_AUDIO_CONTROLLER_H

#include "AudioManager.h"
#include "Classroom.h"
#include <glm/glm.hpp>
#include <array>
#include <cstddef>
#include <vector>

// Bridges gameplay state transitions to the one shared AudioManager. It keeps
// transition guards for the door, projector and all 41 computers, limits bulk
// one-shots, manages the retained projector/ambient/computer-hum loops, and
// generates footsteps from confirmed camera displacement.
class ClassroomAudioController
{
public:
    ClassroomAudioController(AudioManager& audio, Classroom& classroom);

    void Initialize();

    // Call after movement, Classroom::Update(), and E interaction for the
    // current frame so every transition is observed exactly once.
    void Update(float deltaTime,
                const glm::vec3& previousListenerPosition,
                const glm::vec3& listenerPosition,
                const glm::vec3& listenerForward,
                const glm::vec3& listenerUp,
                bool running,
                bool onGround);

    // Plays the local mechanical/input click for the exact object/child AABB
    // selected by the raycast. State-machine startup/shutdown sounds are still
    // triggered centrally from guarded transitions in Update().
    void OnInteraction(Interactable* object, int partIndex, bool actionAccepted);

    void StopComputerLoops();

    // F9 is a silent emergency/debug reset. Stop only projector-owned audio,
    // preserve unrelated classroom sounds, and synchronize the transition guard.
    void ResetProjectorAudio();

private:
    struct ComputerEvent
    {
        Computer* computer = nullptr;
        bool startup = false;
        float distanceSquared = 0.0f;
    };

    void RegisterSounds();
    void DetectDoorTransition();
    void DetectProjectorTransition();
    void DetectComputerTransitions(const glm::vec3& listenerPosition);
    void PlayQueuedComputerEvents();
    void UpdateProjectorFan();
    void UpdateComputerHums(const glm::vec3& listenerPosition);
    void UpdateFootsteps(float deltaTime,
                         const glm::vec3& previousPosition,
                         const glm::vec3& currentPosition,
                         bool running,
                         bool onGround);

    AudioManager& m_audio;
    Classroom& m_classroom;

    std::vector<PowerState> m_previousComputerStates;
    ProjectorState m_previousProjectorState = ProjectorState::Off;
    bool m_previousDoorOpen = false;
    std::vector<ComputerEvent> m_computerEvents;

    AudioManager::LoopHandle m_ambientLoop = AudioManager::InvalidLoop;
    AudioManager::LoopHandle m_projectorFanLoop = AudioManager::InvalidLoop;
    static constexpr std::size_t kMaxComputerHumLoops = 4;
    std::array<AudioManager::LoopHandle, kMaxComputerHumLoops> m_computerHumLoops{};
    std::array<const Computer*, kMaxComputerHumLoops> m_humAssignments{};

    float m_footstepTimer = 0.0f;
    bool m_wasMoving = false;
    int m_nextFootstep = 0;
};

#endif // CLASSROOM_AUDIO_CONTROLLER_H
