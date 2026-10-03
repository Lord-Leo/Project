#ifndef TEXTURE_MANAGER_H
#define TEXTURE_MANAGER_H

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class TextureManager
{
public:
    enum class Semantic
    {
        BaseColour,
        Specular,
        Normal,
        Metallic,
        Roughness,
        AmbientOcclusion,
        Emissive,
        GenericLinear
    };

    TextureManager();
    ~TextureManager();

    TextureManager(const TextureManager&) = delete;
    TextureManager& operator=(const TextureManager&) = delete;

    // File textures are cached by canonical path + semantic + flip policy.
    unsigned int Load(const std::string& path, Semantic semantic, bool flipVertically = false);

    // Assimp embedded compressed textures (PNG/JPEG/etc.). `cacheKey` must be
    // stable for the lifetime of the loaded model, e.g. model path + "*0".
    unsigned int LoadEncodedMemory(const std::string& cacheKey,
                                   const unsigned char* bytes,
                                   int byteCount,
                                   Semantic semantic,
                                   bool flipVertically = false);

    // Assimp embedded raw aiTexel data is BGRA8. This method accepts that
    // exact byte layout and uploads it as RGBA after swizzling.
    unsigned int LoadRawBGRA(const std::string& cacheKey,
                             const unsigned char* bgraBytes,
                             int width,
                             int height,
                             Semantic semantic);

    unsigned int GetFallback(Semantic semantic) const;
    unsigned int GetWhiteBaseColour() const { return m_whiteBaseColour; }
    unsigned int GetBlackMetallic() const { return m_blackMetallic; }
    unsigned int GetMediumRoughness() const { return m_mediumRoughness; }
    unsigned int GetFlatNormal() const { return m_flatNormal; }
    unsigned int GetWhiteAO() const { return m_whiteAO; }
    unsigned int GetBlackEmissive() const { return m_blackEmissive; }

    size_t GetCachedTextureCount() const { return m_cache.size(); }

private:
    unsigned int Upload(const unsigned char* data, int width, int height, int channels,
                        Semantic semantic, const std::string& debugName);
    unsigned int CreateSolidRGBA(unsigned char r, unsigned char g, unsigned char b,
                                 unsigned char a, bool srgb);
    std::string MakeFileCacheKey(const std::string& path, Semantic semantic, bool flipVertically) const;
    static bool IsColourSemantic(Semantic semantic);
    void WarnOnce(const std::string& key, const std::string& message);

    std::unordered_map<std::string, unsigned int> m_cache;
    std::unordered_set<std::string> m_warnings;
    std::vector<unsigned int> m_ownedTextures;

    unsigned int m_whiteBaseColour = 0;
    unsigned int m_blackMetallic = 0;
    unsigned int m_mediumRoughness = 0;
    unsigned int m_flatNormal = 0;
    unsigned int m_whiteAO = 0;
    unsigned int m_blackEmissive = 0;
};

#endif // TEXTURE_MANAGER_H
