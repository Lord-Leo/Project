#include "ClassroomAudioController.h"

#include "Computer.h"
#include "Door.h"
#include "Projector.h"

#include <algorithm>
#include <cmath>
#include <string>

ClassroomAudioController::ClassroomAudioController(AudioManager& audio, Classroom& classroom)
    : m_audio(audio), m_classroom(classroom)
{
    m_computerHumLoops.fill(AudioManager::InvalidLoop);
    m_humAssignments.fill(nullptr);
}

void ClassroomAudioController::Initialize()
{
    RegisterSounds();

    const auto computers = m_classroom.GetComputers();
    m_previousComputerStates.clear();
    m_previousComputerStates.reserve(computers.size());
    for (const Computer* computer : computers)
        m_previousComputerStates.push_back(computer->GetPowerState());

    if (Projector* projector = m_classroom.GetProjector())
        m_previousProjectorState = projector->GetState();
    if (Door* door = m_classroom.GetDoor())
        m_previousDoorOpen = door->IsOpen();

    m_ambientLoop = m_audio.CreateLoop("classroom_ambient", false);
    m_audio.SetLoopVolume(m_ambientLoop, 0.55f);
    m_audio.StartLoop(m_ambientLoop);

    if (Projector* projector = m_classroom.GetProjector())
    {
        m_projectorFanLoop = m_audio.CreateLoop("projector_fan", true,
                                               projector->GetBodyPosition(), 0.8f, 12.0f);
        m_audio.SetLoopVolume(m_projectorFanLoop, 0.0f);
    }

    for (std::size_t i = 0; i < kMaxComputerHumLoops; ++i)
    {
        m_computerHumLoops[i] = m_audio.CreateLoop("computer_hum", true,
                                                   glm::vec3(0.0f), 0.45f, 5.5f);
        m_audio.SetLoopVolume(m_computerHumLoops[i], 0.0f);
    }
}

void ClassroomAudioController::RegisterSounds()
{
    using Category = AudioManager::Category;

    m_audio.RegisterSound("footstep_01", "footsteps/footstep_01.wav", Category::Footsteps, 2);
    m_audio.RegisterSound("footstep_02", "footsteps/footstep_02.wav", Category::Footsteps, 2);
    m_audio.RegisterSound("footstep_03", "footsteps/footstep_03.wav", Category::Footsteps, 2);

    m_audio.RegisterSound("light_switch", "switches/light_switch.wav", Category::Effects, 3);
    m_audio.RegisterSound("door_open", "door/door_open.wav", Category::Effects, 2);
    m_audio.RegisterSound("door_close", "door/door_close.wav", Category::Effects, 2);

    m_audio.RegisterSound("computer_startup", "computer/computer_startup.wav", Category::Effects, 6);
    m_audio.RegisterSound("computer_shutdown", "computer/computer_shutdown.wav", Category::Effects, 6);
    m_audio.RegisterSound("computer_hum", "computer/computer_hum_loop.wav", Category::Machinery, 1);
    m_audio.RegisterSound("keyboard_click", "computer/keyboard_click.wav", Category::Effects, 4);
    m_audio.RegisterSound("mouse_click", "computer/mouse_click.wav", Category::Effects, 4);
    m_audio.RegisterSound("power_button", "computer/power_button.wav", Category::Effects, 4);

    m_audio.RegisterSound("projector_startup", "projector/projector_startup.wav", Category::Effects, 2);
    m_audio.RegisterSound("projector_shutdown", "projector/projector_shutdown.wav", Category::Effects, 2);
    m_audio.RegisterSound("projector_fan", "projector/projector_fan_loop.wav", Category::Machinery, 1);

    m_audio.RegisterSound("classroom_ambient", "ambience/classroom_ambient_loop.wav", Category::Ambient, 1);
}

void ClassroomAudioController::Update(float deltaTime,
                                      const glm::vec3& previousListenerPosition,
                                      const glm::vec3& listenerPosition,
                                      const glm::vec3& listenerForward,
                                      const glm::vec3& listenerUp,
                                      bool running,
                                      bool onGround)
{
    m_audio.UpdateListener(listenerPosition, listenerForward, listenerUp);

    DetectDoorTransition();
    DetectProjectorTransition();
    DetectComputerTransitions(listenerPosition);
    PlayQueuedComputerEvents();
    UpdateProjectorFan();
    UpdateComputerHums(listenerPosition);
    UpdateFootsteps(deltaTime, previousListenerPosition, listenerPosition, running, onGround);
}

void ClassroomAudioController::OnInteraction(Interactable* object, int partIndex, bool actionAccepted)
{
    if (!object || !actionAccepted) return;

    glm::vec3 position = object->GetPartCenter(partIndex);

    if (dynamic_cast<LightSwitch*>(object))
    {
        m_audio.Play3D("light_switch", position, 0.9f, 1.0f, 0.35f, 8.0f);
        return;
    }

    if (dynamic_cast<ProjectorPowerSwitch*>(object) ||
        dynamic_cast<ProjectorControlPanel*>(object))
    {
        m_audio.Play3D("power_button", position, 0.65f, 1.0f, 0.35f, 7.0f);
        return;
    }

    if (dynamic_cast<Computer*>(object))
    {
        switch (partIndex)
        {
            case 1:
                m_audio.Play3D("keyboard_click", position, 0.7f, 1.0f, 0.25f, 5.0f);
                break;
            case 2:
                m_audio.Play3D("mouse_click", position, 0.75f, 1.0f, 0.25f, 5.0f);
                break;
            case 0:
            case 3:
            default:
                m_audio.Play3D("power_button", position, 0.75f, 1.0f, 0.25f, 5.0f);
                break;
        }
    }
}

void ClassroomAudioController::DetectDoorTransition()
{
    Door* door = m_classroom.GetDoor();
    if (!door) return;

    bool isOpen = door->IsOpen();
    if (isOpen != m_previousDoorOpen)
    {
        m_audio.Play3D(isOpen ? "door_open" : "door_close",
                       door->GetHingePosition() + glm::vec3(0.0f, 1.0f, 0.0f),
                       0.85f, 1.0f, 0.8f, 12.0f);
        m_previousDoorOpen = isOpen;
    }
}

void ClassroomAudioController::DetectProjectorTransition()
{
    Projector* projector = m_classroom.GetProjector();
    if (!projector) return;

    ProjectorState current = projector->GetState();
    if (current == m_previousProjectorState) return;

    const glm::vec3 position = projector->GetBodyPosition();
    if (current == ProjectorState::Starting)
    {
        m_audio.StopSound("projector_shutdown");
        m_audio.Play3D("projector_startup", position, 0.9f, 1.0f, 0.9f, 14.0f);
        m_audio.StartLoop(m_projectorFanLoop);
    }
    else if (current == ProjectorState::ShuttingDown)
    {
        m_audio.StopSound("projector_startup");
        m_audio.Play3D("projector_shutdown", position, 0.9f, 1.0f, 0.9f, 14.0f);
        m_audio.StartLoop(m_projectorFanLoop);
    }
    else if (current == ProjectorState::Off)
    {
        m_audio.StopLoop(m_projectorFanLoop);
    }

    m_previousProjectorState = current;
}

void ClassroomAudioController::DetectComputerTransitions(const glm::vec3& listenerPosition)
{
    const auto computers = m_classroom.GetComputers();
    if (m_previousComputerStates.size() != computers.size())
    {
        m_previousComputerStates.assign(computers.size(), PowerState::Off);
    }

    for (std::size_t i = 0; i < computers.size(); ++i)
    {
        Computer* computer = computers[i];
        PowerState previous = m_previousComputerStates[i];
        PowerState current = computer->GetPowerState();
        if (current == previous) continue;

        if (current == PowerState::Booting || current == PowerState::ShuttingDown)
        {
            glm::vec3 delta = computer->GetSoundPosition() - listenerPosition;
            m_computerEvents.push_back({computer, current == PowerState::Booting, glm::dot(delta, delta)});
        }

        m_previousComputerStates[i] = current;
    }
}

void ClassroomAudioController::PlayQueuedComputerEvents()
{
    if (m_computerEvents.empty()) return;

    std::sort(m_computerEvents.begin(), m_computerEvents.end(),
              [](const ComputerEvent& a, const ComputerEvent& b)
              {
                  return a.distanceSquared < b.distanceSquared;
              });

    const bool bulk = m_computerEvents.size() > 6;
    const std::size_t maxVoices = bulk ? 4u : m_computerEvents.size();
    for (std::size_t i = 0; i < maxVoices; ++i)
    {
        const ComputerEvent& event = m_computerEvents[i];
        float volume = bulk ? (0.42f - static_cast<float>(i) * 0.05f) : 0.8f;
        float pitch = 0.98f + static_cast<float>((event.computer->GetComputerId() + static_cast<int>(i)) % 5) * 0.01f;
        m_audio.Play3D(event.startup ? "computer_startup" : "computer_shutdown",
                       event.computer->GetSoundPosition(), volume, pitch, 0.6f, 9.0f);
    }

    m_computerEvents.clear();
}

void ClassroomAudioController::UpdateProjectorFan()
{
    Projector* projector = m_classroom.GetProjector();
    if (!projector || m_projectorFanLoop == AudioManager::InvalidLoop) return;

    float fan = projector->GetFanIntensity();
    m_audio.SetLoopPosition(m_projectorFanLoop, projector->GetBodyPosition());
    m_audio.SetLoopVolume(m_projectorFanLoop, fan * 0.55f);

    if (fan > 0.005f)
        m_audio.StartLoop(m_projectorFanLoop);
    else
        m_audio.StopLoop(m_projectorFanLoop);
}

void ClassroomAudioController::UpdateComputerHums(const glm::vec3& listenerPosition)
{
    struct Candidate
    {
        const Computer* computer;
        float distanceSquared;
    };

    std::vector<Candidate> candidates;
    for (const Computer* computer : m_classroom.GetComputers())
    {
        if (computer->GetPowerState() != PowerState::Desktop) continue;
        glm::vec3 delta = computer->GetSoundPosition() - listenerPosition;
        float distanceSquared = glm::dot(delta, delta);
        if (distanceSquared <= 36.0f)
            candidates.push_back({computer, distanceSquared});
    }

    std::sort(candidates.begin(), candidates.end(),
              [](const Candidate& a, const Candidate& b)
              {
                  return a.distanceSquared < b.distanceSquared;
              });

    for (std::size_t slot = 0; slot < kMaxComputerHumLoops; ++slot)
    {
        const Computer* desired = slot < candidates.size() ? candidates[slot].computer : nullptr;
        AudioManager::LoopHandle handle = m_computerHumLoops[slot];

        if (!desired)
        {
            if (m_humAssignments[slot])
                m_audio.StopLoop(handle);
            m_humAssignments[slot] = nullptr;
            continue;
        }

        m_audio.SetLoopPosition(handle, desired->GetSoundPosition());
        float distance = std::sqrt(candidates[slot].distanceSquared);
        float volume = std::clamp(0.12f - distance * 0.012f, 0.035f, 0.12f);
        m_audio.SetLoopVolume(handle, volume);

        if (m_humAssignments[slot] != desired || !m_audio.IsLoopPlaying(handle))
            m_audio.StartLoop(handle);
        m_humAssignments[slot] = desired;
    }
}

void ClassroomAudioController::UpdateFootsteps(float deltaTime,
                                               const glm::vec3& previousPosition,
                                               const glm::vec3& currentPosition,
                                               bool running,
                                               bool onGround)
{
    glm::vec2 displacement(currentPosition.x - previousPosition.x,
                           currentPosition.z - previousPosition.z);
    float movedDistance = std::sqrt(displacement.x * displacement.x + displacement.y * displacement.y);
    float actualSpeed = deltaTime > 1e-5f ? movedDistance / deltaTime : 0.0f;

    if (!onGround || movedDistance < 0.0008f || actualSpeed < 0.08f)
    {
        m_footstepTimer = 0.0f;
        m_wasMoving = false;
        return;
    }

    const float interval = running || actualSpeed > 4.0f ? 0.31f : 0.46f;

    // Do not make the player hold a movement key for half a second before the
    // first audible step. Seed the cadence when movement begins, but still
    // require confirmed post-collision displacement before any sound plays.
    if (!m_wasMoving)
    {
        m_footstepTimer = interval * 0.58f;
        m_wasMoving = true;
    }

    m_footstepTimer += deltaTime;
    if (m_footstepTimer < interval) return;

    m_footstepTimer = std::fmod(m_footstepTimer, interval);
    static const char* kFootsteps[3] = {"footstep_01", "footstep_02", "footstep_03"};
    static const float kPitch[3] = {0.98f, 1.02f, 1.00f};
    static const float kVolume[3] = {0.96f, 1.00f, 0.98f};

    int sample = m_nextFootstep % 3;
    ++m_nextFootstep;

    // These are the local first-person player's own footsteps. Playing them
    // as a 2D one-shot avoids inverse-distance attenuation from the listener
    // at eye height to a source at floor height, which previously made the
    // steps nearly inaudible. Movement/collision still fully controls when
    // each step is allowed to start.
    m_audio.Play2D(kFootsteps[sample],
                   kVolume[sample] * (running ? 1.0f : 0.92f),
                   kPitch[sample]);
}

void ClassroomAudioController::StopComputerLoops()
{
    for (std::size_t i = 0; i < kMaxComputerHumLoops; ++i)
    {
        m_audio.StopLoop(m_computerHumLoops[i]);
        m_humAssignments[i] = nullptr;
    }
}

void ClassroomAudioController::ResetProjectorAudio()
{
    m_audio.StopLoop(m_projectorFanLoop);
    m_audio.SetLoopVolume(m_projectorFanLoop, 0.0f);
    m_audio.StopSound("projector_startup");
    m_audio.StopSound("projector_shutdown");

    if (Projector* projector = m_classroom.GetProjector())
        m_previousProjectorState = projector->GetState();
    else
        m_previousProjectorState = ProjectorState::Off;
}
