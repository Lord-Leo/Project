#include "Material.h"
#include <glad/glad.h>
#include <algorithm>
#include <cmath>

namespace
{
constexpr int kBaseColourUnit = 2;
constexpr int kNormalUnit = 3;
constexpr int kMetallicUnit = 4;
constexpr int kRoughnessUnit = 5;
constexpr int kAOUnit = 6;
constexpr int kEmissiveUnit = 7;
constexpr int kSpecularUnit = 8;

void bindTextureUnit(int unit, unsigned int texture)
{
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, texture);
}
}

void Material::Apply(const Shader& shader, const glm::vec2& uvTiling) const
{
    const float safeRoughness = std::clamp(roughness, 0.045f, 1.0f);
    const float safeMetallic = std::clamp(metallic, 0.0f, 1.0f);
    const float safeAO = std::clamp(ambientOcclusion, 0.0f, 1.0f);
    const glm::vec3 emission = emissiveColour * std::max(emissiveStrength, 0.0f);

    // PBR material values.
    shader.setVec4("baseColour", baseColour);
    shader.setFloat("metallic", safeMetallic);
    shader.setFloat("roughness", safeRoughness);
    shader.setFloat("ambientOcclusion", safeAO);
    shader.setVec3("pbrEmissiveColour", emissiveColour);
    shader.setFloat("emissiveStrength", std::max(emissiveStrength, 0.0f));
    shader.setVec2("materialUVTiling", uvTiling.x, uvTiling.y);

    shader.setBool("hasBaseColourMap", hasBaseColourMap && baseColourMap != 0);
    shader.setBool("hasNormalMap", hasNormalMap && normalMap != 0 && tangentSpaceValid);
    shader.setBool("hasMetallicMap", hasMetallicMap && metallicMap != 0);
    shader.setBool("hasRoughnessMap", hasRoughnessMap && roughnessMap != 0);
    shader.setBool("hasAOMap", hasAOMap && aoMap != 0);
    shader.setBool("hasEmissiveMap", hasEmissiveMap && emissiveMap != 0);
    shader.setBool("hasSpecularMap", hasSpecularMap && specularMap != 0);

    if (baseColourMap) bindTextureUnit(kBaseColourUnit, baseColourMap);
    if (normalMap) bindTextureUnit(kNormalUnit, normalMap);
    if (metallicMap) bindTextureUnit(kMetallicUnit, metallicMap);
    if (roughnessMap) bindTextureUnit(kRoughnessUnit, roughnessMap);
    if (aoMap) bindTextureUnit(kAOUnit, aoMap);
    if (emissiveMap) bindTextureUnit(kEmissiveUnit, emissiveMap);
    if (specularMap) bindTextureUnit(kSpecularUnit, specularMap);

    shader.setInt("baseColourMap", kBaseColourUnit);
    shader.setInt("normalMap", kNormalUnit);
    shader.setInt("metallicMap", kMetallicUnit);
    shader.setInt("roughnessMap", kRoughnessUnit);
    shader.setInt("aoMap", kAOUnit);
    shader.setInt("emissiveMap", kEmissiveUnit);
    shader.setInt("specularMap", kSpecularUnit);

    // Legacy material compatibility. A smooth PBR surface maps to a higher
    // Blinn-Phong exponent; the exact conversion is only for visual parity.
    const float legacyShininess = std::clamp(2.0f / (safeRoughness * safeRoughness) - 2.0f, 2.0f, 256.0f);
    const glm::vec3 dielectricSpecular(0.04f);
    const glm::vec3 legacySpecular = glm::mix(dielectricSpecular, glm::vec3(baseColour), safeMetallic);

    shader.setVec3("objectColor", glm::vec3(baseColour));
    shader.setVec3("emissiveColor", emission);
    shader.setFloat("objectAlpha", baseColour.a);
    shader.setFloat("shininess", legacyShininess);
    shader.setFloat("materialAmbient", 0.04f * safeAO);
    shader.setVec3("materialSpecular", legacySpecular);
    shader.setInt("useTexture", (hasBaseColourMap && baseColourMap != 0) ? 1 : 0);
    shader.setInt("diffuseTexture", kBaseColourUnit);
    shader.setVec2("uvTiling", uvTiling.x, uvTiling.y);
}

Material Material::FromLegacy(const glm::vec3& colour,
                              const glm::vec3& emissive,
                              float shininess,
                              const glm::vec3& specular,
                              float alpha)
{
    Material material;
    material.baseColour = glm::vec4(colour, alpha);
    material.emissiveColour = emissive;
    material.emissiveStrength = 1.0f;

    // Approximate Blinn-Phong-to-roughness conversion. It preserves the
    // existing material hierarchy without making all surfaces mirror-like.
    const float safeShininess = std::max(shininess, 2.0f);
    material.roughness = std::clamp(std::sqrt(2.0f / (safeShininess + 2.0f)), 0.08f, 1.0f);
    material.metallic = std::clamp((specular.r + specular.g + specular.b) / 3.0f - 0.20f, 0.0f, 1.0f);
    material.ambientOcclusion = 1.0f;
    return material;
}
