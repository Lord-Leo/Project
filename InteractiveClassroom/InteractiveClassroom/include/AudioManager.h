#ifndef AUDIO_MANAGER_H
#define AUDIO_MANAGER_H

#include <glm/glm.hpp>
#include <cstdint>
#include <memory>
#include <string>

// Centralized Phase 6 audio engine. The implementation owns the single
// miniaudio engine, all preloaded one-shot voice pools, retained loop sources,
// listener state, volume categories and missing-file warnings.
class AudioManager
{
public:
    enum class Category
    {
        Effects,
        Ambient,
        Machinery,
        Footsteps
    };

    using LoopHandle = std::uint32_t;
    static constexpr LoopHandle InvalidLoop = 0;

    AudioManager();
    ~AudioManager();

    AudioManager(const AudioManager&) = delete;
    AudioManager& operator=(const AudioManager&) = delete;

    bool Initialize(const std::string& soundRoot = "sounds");
    void Shutdown();

    bool IsAvailable() const;

    // Registers and preloads a fixed number of reusable voices. Playback does
    // not open files from the render loop.
    bool RegisterSound(const std::string& key, const std::string& relativePath,
                       Category category, int voiceCount = 3);

    bool Play2D(const std::string& key, float volume = 1.0f, float pitch = 1.0f);
    bool Play3D(const std::string& key, const glm::vec3& position,
                float volume = 1.0f, float pitch = 1.0f,
                float minDistance = 0.7f, float maxDistance = 12.0f);

    // Stops and rewinds only the reusable one-shot voices registered under
    // this key. Retained loops and unrelated sounds are not affected.
    void StopSound(const std::string& key);

    LoopHandle CreateLoop(const std::string& key, bool positional,
                          const glm::vec3& position = glm::vec3(0.0f),
                          float minDistance = 0.8f, float maxDistance = 12.0f);
    void StartLoop(LoopHandle handle);
    void StopLoop(LoopHandle handle);
    void SetLoopPosition(LoopHandle handle, const glm::vec3& position);
    void SetLoopVolume(LoopHandle handle, float volume);
    void SetLoopPitch(LoopHandle handle, float pitch);
    bool IsLoopPlaying(LoopHandle handle) const;

    void UpdateListener(const glm::vec3& position, const glm::vec3& forward,
                        const glm::vec3& up);

    void SetMasterVolume(float value);
    void SetEffectsVolume(float value);
    void SetAmbientVolume(float value);
    void SetMachineryVolume(float value);
    void SetFootstepVolume(float value);

    float GetMasterVolume() const;
    float GetEffectsVolume() const;
    float GetAmbientVolume() const;
    float GetMachineryVolume() const;
    float GetFootstepVolume() const;

    void ToggleMute();
    void ToggleAmbientMute();
    bool IsMuted() const;
    bool IsAmbientMuted() const;

    std::string GetStatusLabel() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

#endif // AUDIO_MANAGER_H
