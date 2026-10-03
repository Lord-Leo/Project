#include "AudioManager.h"

#include <miniaudio.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace
{
float Clamp01(float value)
{
    return std::clamp(value, 0.0f, 1.0f);
}

float ClampPitch(float value)
{
    return std::clamp(value, 0.5f, 2.0f);
}
}

struct AudioManager::Impl
{
    struct Voice
    {
        ma_sound sound{};
        bool initialized = false;
        float baseVolume = 1.0f;
        std::uint64_t serial = 0;
    };

    struct SoundPool
    {
        std::string path;
        Category category = Category::Effects;
        std::vector<Voice> voices;
    };

    struct LoopSource
    {
        ma_sound sound{};
        bool initialized = false;
        bool positional = false;
        bool requestedPlaying = false;
        Category category = Category::Effects;
        float baseVolume = 1.0f;
    };

    ma_engine engine{};
    bool initialized = false;
    std::string soundRoot = "sounds";

    float masterVolume = 0.8f;
    float effectsVolume = 0.9f;
    float ambientVolume = 0.35f;
    float machineryVolume = 0.45f;
    float footstepVolume = 0.85f;
    bool muted = false;
    bool ambientMuted = false;

    std::uint64_t serialCounter = 1;
    LoopHandle nextLoopHandle = 1;

    std::unordered_map<std::string, SoundPool> pools;
    std::unordered_map<LoopHandle, LoopSource> loops;
    std::unordered_set<std::string> warnedPaths;

    float CategoryGain(Category category) const
    {
        switch (category)
        {
            case Category::Effects:   return effectsVolume;
            case Category::Ambient:   return ambientMuted ? 0.0f : ambientVolume;
            case Category::Machinery: return machineryVolume;
            case Category::Footsteps: return footstepVolume;
        }
        return 1.0f;
    }

    float EffectiveGain(Category category, float baseVolume) const
    {
        if (muted) return 0.0f;
        return Clamp01(baseVolume) * masterVolume * CategoryGain(category);
    }

    Voice* ChooseVoice(SoundPool& pool)
    {
        Voice* oldest = nullptr;
        for (Voice& voice : pool.voices)
        {
            if (!voice.initialized) continue;
            if (ma_sound_is_playing(&voice.sound) == MA_FALSE)
                return &voice;
            if (!oldest || voice.serial < oldest->serial)
                oldest = &voice;
        }

        if (oldest)
            ma_sound_stop(&oldest->sound);
        return oldest;
    }

    void WarnMissingOnce(const std::string& path)
    {
        if (warnedPaths.insert(path).second)
            std::cerr << "[Audio] Missing or unreadable sound file: '" << path
                      << "'. This sound will be skipped.\n";
    }

    void StopAllOneShots()
    {
        for (auto& poolPair : pools)
        {
            for (Voice& voice : poolPair.second.voices)
            {
                if (!voice.initialized) continue;
                ma_sound_stop(&voice.sound);
                ma_sound_seek_to_pcm_frame(&voice.sound, 0);
            }
        }
    }

    void ApplyVolumes()
    {
        if (!initialized) return;

        for (auto& poolPair : pools)
        {
            SoundPool& pool = poolPair.second;
            for (Voice& voice : pool.voices)
            {
                if (voice.initialized)
                    ma_sound_set_volume(&voice.sound, EffectiveGain(pool.category, voice.baseVolume));
            }
        }

        for (auto& loopPair : loops)
        {
            LoopSource& loop = loopPair.second;
            if (loop.initialized)
                ma_sound_set_volume(&loop.sound, EffectiveGain(loop.category, loop.baseVolume));
        }
    }
};

AudioManager::AudioManager()
    : m_impl(std::make_unique<Impl>())
{
}

AudioManager::~AudioManager()
{
    Shutdown();
}

bool AudioManager::Initialize(const std::string& soundRoot)
{
    Shutdown();
    m_impl->soundRoot = soundRoot;

    ma_engine_config config = ma_engine_config_init();
    config.listenerCount = 1;

    ma_result result = ma_engine_init(&config, &m_impl->engine);
    if (result != MA_SUCCESS)
    {
        std::cerr << "[Audio] WARNING: miniaudio engine initialization failed (error "
                  << static_cast<int>(result)
                  << "). The classroom will continue without sound.\n";
        m_impl->initialized = false;
        return false;
    }

    m_impl->initialized = true;
    ma_engine_set_volume(&m_impl->engine, 1.0f);
    std::cout << "[Audio] miniaudio initialized successfully.\n";
    return true;
}

void AudioManager::Shutdown()
{
    if (!m_impl) return;

    for (auto& loopPair : m_impl->loops)
    {
        auto& loop = loopPair.second;
        if (loop.initialized)
        {
            ma_sound_stop(&loop.sound);
            ma_sound_uninit(&loop.sound);
            loop.initialized = false;
        }
    }
    m_impl->loops.clear();

    for (auto& poolPair : m_impl->pools)
    {
        for (auto& voice : poolPair.second.voices)
        {
            if (voice.initialized)
            {
                ma_sound_stop(&voice.sound);
                ma_sound_uninit(&voice.sound);
                voice.initialized = false;
            }
        }
    }
    m_impl->pools.clear();

    if (m_impl->initialized)
    {
        ma_engine_uninit(&m_impl->engine);
        m_impl->initialized = false;
    }
}

bool AudioManager::IsAvailable() const
{
    return m_impl && m_impl->initialized;
}

bool AudioManager::RegisterSound(const std::string& key, const std::string& relativePath,
                                 Category category, int voiceCount)
{
    if (!IsAvailable() || key.empty()) return false;

    voiceCount = std::clamp(voiceCount, 1, 8);
    std::filesystem::path fullPath = std::filesystem::path(m_impl->soundRoot) / relativePath;
    std::error_code pathError;
    const bool isRegularFile = std::filesystem::is_regular_file(fullPath, pathError);
    if (pathError || !isRegularFile)
    {
        m_impl->WarnMissingOnce(fullPath.string());
        return false;
    }

    Impl::SoundPool pool;
    pool.path = fullPath.string();
    pool.category = category;
    pool.voices.resize(static_cast<std::size_t>(voiceCount));

    int initializedVoices = 0;
    ma_result preloadError = MA_SUCCESS;
    for (Impl::Voice& voice : pool.voices)
    {
        ma_uint32 flags = MA_SOUND_FLAG_DECODE;
        const ma_result result = ma_sound_init_from_file(&m_impl->engine, pool.path.c_str(), flags,
                                                         nullptr, nullptr, &voice.sound);
        if (result != MA_SUCCESS)
        {
            preloadError = result;
            break;
        }

        voice.initialized = true;
        ma_sound_set_spatialization_enabled(&voice.sound, MA_TRUE);
        ma_sound_set_attenuation_model(&voice.sound, ma_attenuation_model_inverse);
        ma_sound_set_rolloff(&voice.sound, 1.0f);
        ma_sound_set_min_gain(&voice.sound, 0.0f);
        ma_sound_set_max_gain(&voice.sound, 1.0f);
        ++initializedVoices;
    }

    if (preloadError != MA_SUCCESS || initializedVoices != voiceCount)
    {
        for (Impl::Voice& voice : pool.voices)
        {
            if (voice.initialized)
            {
                ma_sound_uninit(&voice.sound);
                voice.initialized = false;
            }
        }

        if (m_impl->warnedPaths.insert(pool.path).second)
        {
            std::cerr << "[Audio] Failed to preload sound file '" << pool.path
                      << "' (error " << static_cast<int>(preloadError)
                      << "). This sound will be skipped.\n";
        }
        return false;
    }

    m_impl->pools[key] = std::move(pool);
    return true;
}

bool AudioManager::Play2D(const std::string& key, float volume, float pitch)
{
    if (!IsAvailable() || m_impl->muted) return false;
    auto it = m_impl->pools.find(key);
    if (it == m_impl->pools.end()) return false;

    Impl::Voice* voice = m_impl->ChooseVoice(it->second);
    if (!voice) return false;

    voice->serial = m_impl->serialCounter++;
    voice->baseVolume = Clamp01(volume);
    ma_sound_stop(&voice->sound);
    ma_sound_seek_to_pcm_frame(&voice->sound, 0);
    ma_sound_set_spatialization_enabled(&voice->sound, MA_FALSE);
    ma_sound_set_pitch(&voice->sound, ClampPitch(pitch));
    ma_sound_set_volume(&voice->sound, m_impl->EffectiveGain(it->second.category, voice->baseVolume));
    return ma_sound_start(&voice->sound) == MA_SUCCESS;
}

bool AudioManager::Play3D(const std::string& key, const glm::vec3& position,
                          float volume, float pitch, float minDistance, float maxDistance)
{
    if (!IsAvailable() || m_impl->muted) return false;
    auto it = m_impl->pools.find(key);
    if (it == m_impl->pools.end()) return false;

    Impl::Voice* voice = m_impl->ChooseVoice(it->second);
    if (!voice) return false;

    voice->serial = m_impl->serialCounter++;
    voice->baseVolume = Clamp01(volume);
    ma_sound_stop(&voice->sound);
    ma_sound_seek_to_pcm_frame(&voice->sound, 0);
    ma_sound_set_spatialization_enabled(&voice->sound, MA_TRUE);
    ma_sound_set_positioning(&voice->sound, ma_positioning_absolute);
    ma_sound_set_position(&voice->sound, position.x, position.y, position.z);
    ma_sound_set_attenuation_model(&voice->sound, ma_attenuation_model_inverse);
    ma_sound_set_min_distance(&voice->sound, std::max(0.05f, minDistance));
    ma_sound_set_max_distance(&voice->sound, std::max(minDistance + 0.05f, maxDistance));
    ma_sound_set_rolloff(&voice->sound, 1.0f);
    ma_sound_set_pitch(&voice->sound, ClampPitch(pitch));
    ma_sound_set_volume(&voice->sound, m_impl->EffectiveGain(it->second.category, voice->baseVolume));
    return ma_sound_start(&voice->sound) == MA_SUCCESS;
}

void AudioManager::StopSound(const std::string& key)
{
    if (!IsAvailable()) return;
    auto it = m_impl->pools.find(key);
    if (it == m_impl->pools.end()) return;

    for (Impl::Voice& voice : it->second.voices)
    {
        if (!voice.initialized) continue;
        ma_sound_stop(&voice.sound);
        ma_sound_seek_to_pcm_frame(&voice.sound, 0);
    }
}

AudioManager::LoopHandle AudioManager::CreateLoop(const std::string& key, bool positional,
                                                  const glm::vec3& position,
                                                  float minDistance, float maxDistance)
{
    if (!IsAvailable()) return InvalidLoop;
    auto poolIt = m_impl->pools.find(key);
    if (poolIt == m_impl->pools.end()) return InvalidLoop;

    LoopHandle handle = m_impl->nextLoopHandle++;
    if (handle == InvalidLoop) handle = m_impl->nextLoopHandle++;

    // ma_sound is an opaque engine object and must not be byte-moved after it
    // has been initialized. Create the map node first, then initialize the
    // retained loop directly in its final storage location.
    auto [loopIt, inserted] = m_impl->loops.try_emplace(handle);
    if (!inserted) return InvalidLoop;

    Impl::LoopSource& loop = loopIt->second;
    loop.positional = positional;
    loop.category = poolIt->second.category;

    ma_uint32 flags = MA_SOUND_FLAG_DECODE;
    if (!positional) flags |= MA_SOUND_FLAG_NO_SPATIALIZATION;
    const ma_result result = ma_sound_init_from_file(&m_impl->engine,
                                                     poolIt->second.path.c_str(), flags,
                                                     nullptr, nullptr, &loop.sound);
    if (result != MA_SUCCESS)
    {
        std::cerr << "[Audio] Failed to create loop from '" << poolIt->second.path
                  << "' (error " << static_cast<int>(result) << ").\n";
        m_impl->loops.erase(loopIt);
        return InvalidLoop;
    }

    loop.initialized = true;
    ma_sound_set_looping(&loop.sound, MA_TRUE);
    ma_sound_set_spatialization_enabled(&loop.sound, positional ? MA_TRUE : MA_FALSE);
    if (positional)
    {
        ma_sound_set_positioning(&loop.sound, ma_positioning_absolute);
        ma_sound_set_position(&loop.sound, position.x, position.y, position.z);
        ma_sound_set_attenuation_model(&loop.sound, ma_attenuation_model_inverse);
        ma_sound_set_min_distance(&loop.sound, std::max(0.05f, minDistance));
        ma_sound_set_max_distance(&loop.sound, std::max(minDistance + 0.05f, maxDistance));
        ma_sound_set_rolloff(&loop.sound, 1.0f);
    }

    ma_sound_set_volume(&loop.sound, m_impl->EffectiveGain(loop.category, loop.baseVolume));
    return handle;
}

void AudioManager::StartLoop(LoopHandle handle)
{
    auto it = m_impl->loops.find(handle);
    if (!IsAvailable() || it == m_impl->loops.end() || !it->second.initialized) return;
    if (it->second.requestedPlaying &&
        ma_sound_is_playing(&it->second.sound) == MA_TRUE)
        return;

    it->second.requestedPlaying = true;
    if (ma_sound_at_end(&it->second.sound) == MA_TRUE)
        ma_sound_seek_to_pcm_frame(&it->second.sound, 0);
    if (ma_sound_is_playing(&it->second.sound) == MA_FALSE)
        ma_sound_start(&it->second.sound);
}

void AudioManager::StopLoop(LoopHandle handle)
{
    auto it = m_impl->loops.find(handle);
    if (!IsAvailable() || it == m_impl->loops.end() || !it->second.initialized) return;
    if (!it->second.requestedPlaying) return;
    it->second.requestedPlaying = false;
    ma_sound_stop(&it->second.sound);
    ma_sound_seek_to_pcm_frame(&it->second.sound, 0);
}

void AudioManager::SetLoopPosition(LoopHandle handle, const glm::vec3& position)
{
    auto it = m_impl->loops.find(handle);
    if (!IsAvailable() || it == m_impl->loops.end() || !it->second.initialized || !it->second.positional) return;
    ma_sound_set_position(&it->second.sound, position.x, position.y, position.z);
}

void AudioManager::SetLoopVolume(LoopHandle handle, float volume)
{
    auto it = m_impl->loops.find(handle);
    if (!IsAvailable() || it == m_impl->loops.end() || !it->second.initialized) return;
    it->second.baseVolume = Clamp01(volume);
    ma_sound_set_volume(&it->second.sound, m_impl->EffectiveGain(it->second.category, it->second.baseVolume));
}

void AudioManager::SetLoopPitch(LoopHandle handle, float pitch)
{
    auto it = m_impl->loops.find(handle);
    if (!IsAvailable() || it == m_impl->loops.end() || !it->second.initialized) return;
    ma_sound_set_pitch(&it->second.sound, ClampPitch(pitch));
}

bool AudioManager::IsLoopPlaying(LoopHandle handle) const
{
    auto it = m_impl->loops.find(handle);
    if (!IsAvailable() || it == m_impl->loops.end() || !it->second.initialized) return false;
    return ma_sound_is_playing(&it->second.sound) == MA_TRUE;
}

void AudioManager::UpdateListener(const glm::vec3& position, const glm::vec3& forward,
                                  const glm::vec3& up)
{
    if (!IsAvailable()) return;
    ma_engine_listener_set_position(&m_impl->engine, 0, position.x, position.y, position.z);
    ma_engine_listener_set_direction(&m_impl->engine, 0, forward.x, forward.y, forward.z);
    ma_engine_listener_set_world_up(&m_impl->engine, 0, up.x, up.y, up.z);
}

void AudioManager::SetMasterVolume(float value)
{
    m_impl->masterVolume = Clamp01(value);
    m_impl->ApplyVolumes();
}

void AudioManager::SetEffectsVolume(float value)
{
    m_impl->effectsVolume = Clamp01(value);
    m_impl->ApplyVolumes();
}

void AudioManager::SetAmbientVolume(float value)
{
    m_impl->ambientVolume = Clamp01(value);
    m_impl->ApplyVolumes();
}

void AudioManager::SetMachineryVolume(float value)
{
    m_impl->machineryVolume = Clamp01(value);
    m_impl->ApplyVolumes();
}

void AudioManager::SetFootstepVolume(float value)
{
    m_impl->footstepVolume = Clamp01(value);
    m_impl->ApplyVolumes();
}

float AudioManager::GetMasterVolume() const { return m_impl->masterVolume; }
float AudioManager::GetEffectsVolume() const { return m_impl->effectsVolume; }
float AudioManager::GetAmbientVolume() const { return m_impl->ambientVolume; }
float AudioManager::GetMachineryVolume() const { return m_impl->machineryVolume; }
float AudioManager::GetFootstepVolume() const { return m_impl->footstepVolume; }

void AudioManager::ToggleMute()
{
    m_impl->muted = !m_impl->muted;

    // Loops stay alive at zero gain so ambience and machinery resume smoothly.
    // One-shots are stopped and rewound so unmuting can never reveal the tail
    // of a sound that began before the mute command.
    if (m_impl->muted)
        m_impl->StopAllOneShots();

    m_impl->ApplyVolumes();
}

void AudioManager::ToggleAmbientMute()
{
    m_impl->ambientMuted = !m_impl->ambientMuted;
    m_impl->ApplyVolumes();
}

bool AudioManager::IsMuted() const { return m_impl->muted; }
bool AudioManager::IsAmbientMuted() const { return m_impl->ambientMuted; }

std::string AudioManager::GetStatusLabel() const
{
    if (!IsAvailable()) return "AUDIO UNAVAILABLE";
    if (IsMuted()) return "AUDIO MUTED";

    std::ostringstream stream;
    stream << "AUDIO " << static_cast<int>(std::round(GetMasterVolume() * 100.0f)) << "%"
           << " AMB " << (IsAmbientMuted()
                              ? std::string("MUTED")
                              : std::to_string(static_cast<int>(std::round(GetAmbientVolume() * 100.0f))) + "%")
           << " MACH " << static_cast<int>(std::round(GetMachineryVolume() * 100.0f)) << "%";
    return stream.str();
}
