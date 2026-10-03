#ifndef MATERIAL_H
#define MATERIAL_H

#include <glm/glm.hpp>
#include "Shader.h"

// Metallic-roughness material shared by procedural primitives and Assimp
// meshes. Texture flags are explicit so fallback textures never accidentally
// enable a map that was not present in the source asset.
struct Material
{
    glm::vec4 baseColour{0.8f, 0.8f, 0.8f, 1.0f};
    float metallic = 0.0f;
    float roughness = 0.6f;
    float ambientOcclusion = 1.0f;
    glm::vec3 emissiveColour{0.0f};
    float emissiveStrength = 1.0f;

    unsigned int baseColourMap = 0;
    unsigned int normalMap = 0;
    unsigned int metallicMap = 0;
    unsigned int roughnessMap = 0;
    unsigned int aoMap = 0;
    unsigned int emissiveMap = 0;
    unsigned int specularMap = 0;

    bool hasBaseColourMap = false;
    bool hasNormalMap = false;
    bool hasMetallicMap = false;
    bool hasRoughnessMap = false;
    bool hasAOMap = false;
    bool hasEmissiveMap = false;
    bool hasSpecularMap = false;
    bool tangentSpaceValid = false;

    // Applies both the PBR uniform set and the existing legacy uniform set.
    // Uniforms absent from the currently bound shader resolve to location -1
    // and are ignored by OpenGL, allowing one draw path for depth/legacy/PBR.
    void Apply(const Shader& shader, const glm::vec2& uvTiling = glm::vec2(1.0f)) const;

    static Material FromLegacy(const glm::vec3& colour,
                               const glm::vec3& emissive,
                               float shininess,
                               const glm::vec3& specular,
                               float alpha = 1.0f);
};

#endif // MATERIAL_H
