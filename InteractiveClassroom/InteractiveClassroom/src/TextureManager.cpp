#include "TextureManager.h"

#include <glad/glad.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <sstream>

namespace fs = std::filesystem;

TextureManager::TextureManager()
{
    m_whiteBaseColour = CreateSolidRGBA(255, 255, 255, 255, true);
    m_blackMetallic = CreateSolidRGBA(0, 0, 0, 255, false);
    m_mediumRoughness = CreateSolidRGBA(128, 128, 128, 255, false);
    m_flatNormal = CreateSolidRGBA(128, 128, 255, 255, false);
    m_whiteAO = CreateSolidRGBA(255, 255, 255, 255, false);
    m_blackEmissive = CreateSolidRGBA(0, 0, 0, 255, true);
}

TextureManager::~TextureManager()
{
    if (!m_ownedTextures.empty())
        glDeleteTextures(static_cast<GLsizei>(m_ownedTextures.size()), m_ownedTextures.data());
}

bool TextureManager::IsColourSemantic(Semantic semantic)
{
    return semantic == Semantic::BaseColour ||
           semantic == Semantic::Emissive ||
           semantic == Semantic::Specular;
}

void TextureManager::WarnOnce(const std::string& key, const std::string& message)
{
    if (m_warnings.insert(key).second)
        std::cerr << message << '\n';
}

std::string TextureManager::MakeFileCacheKey(const std::string& path,
                                             Semantic semantic,
                                             bool flipVertically) const
{
    std::error_code ec;
    fs::path canonical = fs::weakly_canonical(fs::path(path), ec);
    if (ec)
        canonical = fs::absolute(fs::path(path), ec);
    if (ec)
        canonical = fs::path(path);

    std::ostringstream key;
    key << canonical.generic_string() << "|semantic=" << static_cast<int>(semantic)
        << "|flip=" << (flipVertically ? 1 : 0);
    return key.str();
}

unsigned int TextureManager::GetFallback(Semantic semantic) const
{
    switch (semantic)
    {
        case Semantic::BaseColour:       return m_whiteBaseColour;
        case Semantic::Specular:         return m_whiteBaseColour;
        case Semantic::Normal:           return m_flatNormal;
        case Semantic::Metallic:         return m_blackMetallic;
        case Semantic::Roughness:        return m_mediumRoughness;
        case Semantic::AmbientOcclusion: return m_whiteAO;
        case Semantic::Emissive:         return m_blackEmissive;
        case Semantic::GenericLinear:    return m_whiteAO;
        default:                         return m_whiteBaseColour;
    }
}

unsigned int TextureManager::CreateSolidRGBA(unsigned char r, unsigned char g,
                                             unsigned char b, unsigned char a,
                                             bool srgb)
{
    unsigned char pixel[4] = {r, g, b, a};
    unsigned int texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, srgb ? GL_SRGB_ALPHA : GL_RGBA,
                 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    m_ownedTextures.push_back(texture);
    return texture;
}

unsigned int TextureManager::Upload(const unsigned char* data, int width, int height,
                                    int channels, Semantic semantic,
                                    const std::string& debugName)
{
    if (!data || width <= 0 || height <= 0 || channels < 1 || channels > 4)
    {
        WarnOnce("invalid:" + debugName,
                 "[TextureManager] Invalid texture data for '" + debugName + "'; using fallback.");
        return GetFallback(semantic);
    }

    GLenum dataFormat = GL_RGB;
    GLint internalFormat = GL_RGB;
    switch (channels)
    {
        case 1:
            dataFormat = GL_RED;
            internalFormat = GL_RED;
            break;
        case 2:
            dataFormat = GL_RG;
            internalFormat = GL_RG;
            break;
        case 3:
            dataFormat = GL_RGB;
            internalFormat = IsColourSemantic(semantic) ? GL_SRGB : GL_RGB;
            break;
        case 4:
            dataFormat = GL_RGBA;
            internalFormat = IsColourSemantic(semantic) ? GL_SRGB_ALPHA : GL_RGBA;
            break;
        default:
            return GetFallback(semantic);
    }

    unsigned int texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0,
                 dataFormat, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    m_ownedTextures.push_back(texture);
    return texture;
}

unsigned int TextureManager::Load(const std::string& path, Semantic semantic,
                                  bool flipVertically)
{
    const std::string key = MakeFileCacheKey(path, semantic, flipVertically);
    auto found = m_cache.find(key);
    if (found != m_cache.end())
        return found->second;

    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_set_flip_vertically_on_load(flipVertically ? 1 : 0);
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 0);
    if (!data)
    {
        WarnOnce("missing:" + key,
                 "[TextureManager] Missing or unreadable texture '" + path + "'; using semantic fallback.");
        unsigned int fallback = GetFallback(semantic);
        m_cache.emplace(key, fallback);
        return fallback;
    }

    unsigned int texture = Upload(data, width, height, channels, semantic, path);
    stbi_image_free(data);
    m_cache.emplace(key, texture);
    return texture;
}

unsigned int TextureManager::LoadEncodedMemory(const std::string& cacheKey,
                                               const unsigned char* bytes,
                                               int byteCount,
                                               Semantic semantic,
                                               bool flipVertically)
{
    std::ostringstream keyBuilder;
    keyBuilder << "embedded:" << cacheKey << "|semantic=" << static_cast<int>(semantic)
               << "|flip=" << (flipVertically ? 1 : 0);
    const std::string key = keyBuilder.str();
    auto found = m_cache.find(key);
    if (found != m_cache.end())
        return found->second;

    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_set_flip_vertically_on_load(flipVertically ? 1 : 0);
    unsigned char* decoded = stbi_load_from_memory(bytes, byteCount, &width, &height, &channels, 0);
    if (!decoded)
    {
        WarnOnce("missing:" + key,
                 "[TextureManager] Could not decode embedded texture '" + cacheKey + "'; using fallback.");
        unsigned int fallback = GetFallback(semantic);
        m_cache.emplace(key, fallback);
        return fallback;
    }

    unsigned int texture = Upload(decoded, width, height, channels, semantic, cacheKey);
    stbi_image_free(decoded);
    m_cache.emplace(key, texture);
    return texture;
}

unsigned int TextureManager::LoadRawBGRA(const std::string& cacheKey,
                                         const unsigned char* bgraBytes,
                                         int width,
                                         int height,
                                         Semantic semantic)
{
    std::ostringstream keyBuilder;
    keyBuilder << "embedded-raw:" << cacheKey << "|semantic=" << static_cast<int>(semantic);
    const std::string key = keyBuilder.str();
    auto found = m_cache.find(key);
    if (found != m_cache.end())
        return found->second;

    if (!bgraBytes || width <= 0 || height <= 0)
    {
        unsigned int fallback = GetFallback(semantic);
        m_cache.emplace(key, fallback);
        return fallback;
    }

    std::vector<unsigned char> rgba(static_cast<size_t>(width) * static_cast<size_t>(height) * 4u);
    for (size_t i = 0; i < rgba.size(); i += 4)
    {
        rgba[i + 0] = bgraBytes[i + 2];
        rgba[i + 1] = bgraBytes[i + 1];
        rgba[i + 2] = bgraBytes[i + 0];
        rgba[i + 3] = bgraBytes[i + 3];
    }

    unsigned int texture = Upload(rgba.data(), width, height, 4, semantic, cacheKey);
    m_cache.emplace(key, texture);
    return texture;
}
