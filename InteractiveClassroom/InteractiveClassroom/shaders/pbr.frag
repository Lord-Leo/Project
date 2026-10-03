#version 330 core
layout (location = 0) out vec4 SceneColor;
layout (location = 1) out vec4 BrightColor;

in VS_OUT
{
    vec3 worldPosition;
    vec3 worldNormal;
    vec2 texCoords;
    vec3 worldTangent;
    vec3 worldBitangent;
    vec4 lightSpacePosition;
} fsIn;

const float PI = 3.14159265359;
#define MAX_LIGHTS 64
#define MAX_SPOT_LIGHTS 4

uniform vec3 lightPositions[MAX_LIGHTS];
uniform vec3 lightColors[MAX_LIGHTS];
uniform float lightIntensities[MAX_LIGHTS];
uniform int numLights;

uniform vec3 spotLightPositions[MAX_SPOT_LIGHTS];
uniform vec3 spotLightDirections[MAX_SPOT_LIGHTS];
uniform vec3 spotLightColors[MAX_SPOT_LIGHTS];
uniform float spotLightIntensities[MAX_SPOT_LIGHTS];
uniform float spotLightInnerCutoffs[MAX_SPOT_LIGHTS];
uniform float spotLightOuterCutoffs[MAX_SPOT_LIGHTS];
uniform int numSpotLights;

uniform vec3 dirLightDirection;
uniform vec3 dirLightColor;
uniform float dirLightIntensity;
uniform vec3 viewPos;
uniform float roomAmbientFactor;

uniform vec4 baseColour;
uniform float metallic;
uniform float roughness;
uniform float ambientOcclusion;
uniform vec3 pbrEmissiveColour;
uniform float emissiveStrength;
uniform vec2 materialUVTiling;
uniform float objectAlpha;

uniform sampler2D baseColourMap;
uniform sampler2D normalMap;
uniform sampler2D metallicMap;
uniform sampler2D roughnessMap;
uniform sampler2D aoMap;
uniform sampler2D emissiveMap;
uniform sampler2D specularMap;
uniform bool hasBaseColourMap;
uniform bool hasNormalMap;
uniform bool hasMetallicMap;
uniform bool hasRoughnessMap;
uniform bool hasAOMap;
uniform bool hasEmissiveMap;
uniform bool hasSpecularMap;

uniform sampler2D shadowMap;
uniform bool shadowsEnabled;
uniform bool gammaEnabled;
uniform float bloomThreshold;

float DistributionGGX(vec3 normal, vec3 halfway, float materialRoughness)
{
    float a = materialRoughness * materialRoughness;
    float a2 = a * a;
    float nDotH = max(dot(normal, halfway), 0.0);
    float nDotH2 = nDotH * nDotH;
    float denominator = nDotH2 * (a2 - 1.0) + 1.0;
    return a2 / max(PI * denominator * denominator, 0.000001);
}

float GeometrySchlickGGX(float nDotV, float materialRoughness)
{
    float r = materialRoughness + 1.0;
    float k = (r * r) / 8.0;
    return nDotV / max(nDotV * (1.0 - k) + k, 0.000001);
}

float GeometrySmith(vec3 normal, vec3 viewDirection, vec3 lightDirection, float materialRoughness)
{
    float nDotV = max(dot(normal, viewDirection), 0.0);
    float nDotL = max(dot(normal, lightDirection), 0.0);
    return GeometrySchlickGGX(nDotV, materialRoughness) *
           GeometrySchlickGGX(nDotL, materialRoughness);
}

vec3 FresnelSchlick(float cosTheta, vec3 f0)
{
    return f0 + (1.0 - f0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float ShadowCalculation(vec4 lightSpacePosition, vec3 normal, vec3 lightDirection)
{
    vec3 projectionCoordinates = lightSpacePosition.xyz / max(lightSpacePosition.w, 0.00001);
    projectionCoordinates = projectionCoordinates * 0.5 + 0.5;
    if (projectionCoordinates.z > 1.0 || projectionCoordinates.x < 0.0 ||
        projectionCoordinates.x > 1.0 || projectionCoordinates.y < 0.0 ||
        projectionCoordinates.y > 1.0)
        return 0.0;

    float bias = max(0.0035 * (1.0 - dot(normal, lightDirection)), 0.0008);
    vec2 texelSize = 1.0 / vec2(textureSize(shadowMap, 0));
    float shadow = 0.0;
    for (int x = -1; x <= 1; ++x)
        for (int y = -1; y <= 1; ++y)
            shadow += (projectionCoordinates.z - bias) >
                      texture(shadowMap, projectionCoordinates.xy + vec2(x, y) * texelSize).r ? 1.0 : 0.0;
    return shadow / 9.0;
}

vec3 EvaluateBRDF(vec3 normal, vec3 viewDirection, vec3 lightDirection,
                  vec3 radiance, vec3 albedo, float materialMetallic,
                  float materialRoughness, vec3 f0)
{
    vec3 halfway = normalize(viewDirection + lightDirection);
    float ndf = DistributionGGX(normal, halfway, materialRoughness);
    float geometry = GeometrySmith(normal, viewDirection, lightDirection, materialRoughness);
    vec3 fresnel = FresnelSchlick(max(dot(halfway, viewDirection), 0.0), f0);
    vec3 numerator = ndf * geometry * fresnel;
    float denominator = 4.0 * max(dot(normal, viewDirection), 0.0) *
                        max(dot(normal, lightDirection), 0.0) + 0.0001;
    vec3 specular = numerator / denominator;
    vec3 kS = fresnel;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - materialMetallic);
    float nDotL = max(dot(normal, lightDirection), 0.0);
    return (kD * albedo / PI + specular) * radiance * nDotL;
}

void main()
{
    vec2 uv = fsIn.texCoords * materialUVTiling;
    vec4 sampledBase = hasBaseColourMap ? texture(baseColourMap, uv) : vec4(1.0);
    vec3 albedo = max(baseColour.rgb * sampledBase.rgb, vec3(0.0));
    float alpha = objectAlpha * baseColour.a * sampledBase.a;
    float materialMetallic = clamp(metallic * (hasMetallicMap ? texture(metallicMap, uv).r : 1.0), 0.0, 1.0);
    float materialRoughness = clamp(roughness * (hasRoughnessMap ? texture(roughnessMap, uv).r : 1.0), 0.045, 1.0);
    float ao = clamp(ambientOcclusion * (hasAOMap ? texture(aoMap, uv).r : 1.0), 0.0, 1.0);

    vec3 normal = normalize(fsIn.worldNormal);
    if (hasNormalMap)
    {
        vec3 tangent = normalize(fsIn.worldTangent - dot(fsIn.worldTangent, normal) * normal);
        vec3 referenceBitangent = normalize(fsIn.worldBitangent);
        vec3 bitangent = normalize(cross(normal, tangent));
        if (dot(bitangent, referenceBitangent) < 0.0)
            bitangent = -bitangent;
        if (length(tangent) > 0.01 && length(bitangent) > 0.01)
        {
            mat3 tbn = mat3(tangent, bitangent, normal);
            vec3 mappedNormal = texture(normalMap, uv).xyz * 2.0 - 1.0;
            normal = normalize(tbn * mappedNormal);
        }
    }

    vec3 viewDirection = normalize(viewPos - fsIn.worldPosition);
    vec3 f0 = mix(vec3(0.04), albedo, materialMetallic);
    if (hasSpecularMap)
        f0 *= texture(specularMap, uv).rgb;

    vec3 directLighting = vec3(0.0);
    for (int i = 0; i < numLights; ++i)
    {
        vec3 toLight = lightPositions[i] - fsIn.worldPosition;
        float distanceToLight = length(toLight);
        vec3 lightDirection = toLight / max(distanceToLight, 0.0001);
        float attenuation = lightIntensities[i] /
            (1.0 + 0.15 * distanceToLight + 0.08 * distanceToLight * distanceToLight);
        directLighting += EvaluateBRDF(normal, viewDirection, lightDirection,
            lightColors[i] * attenuation, albedo, materialMetallic, materialRoughness, f0);
    }

    for (int i = 0; i < numSpotLights; ++i)
    {
        vec3 toLight = spotLightPositions[i] - fsIn.worldPosition;
        float distanceToLight = length(toLight);
        vec3 lightDirection = toLight / max(distanceToLight, 0.0001);
        float theta = dot(lightDirection, normalize(-spotLightDirections[i]));
        float cone = clamp((theta - spotLightOuterCutoffs[i]) /
                           max(spotLightInnerCutoffs[i] - spotLightOuterCutoffs[i], 0.0001), 0.0, 1.0);
        float attenuation = cone * spotLightIntensities[i] /
            (1.0 + 0.10 * distanceToLight + 0.035 * distanceToLight * distanceToLight);
        directLighting += EvaluateBRDF(normal, viewDirection, lightDirection,
            spotLightColors[i] * attenuation, albedo, materialMetallic, materialRoughness, f0);
    }

    vec3 directionalDirection = normalize(-dirLightDirection);
    float shadow = shadowsEnabled ? ShadowCalculation(fsIn.lightSpacePosition, normal, directionalDirection) : 0.0;
    directLighting += (1.0 - shadow) * EvaluateBRDF(normal, viewDirection,
        directionalDirection, dirLightColor * dirLightIntensity, albedo,
        materialMetallic, materialRoughness, f0);

    vec3 ambient = vec3(0.025) * albedo * ao * roomAmbientFactor;
    vec3 emissive = pbrEmissiveColour * max(emissiveStrength, 0.0);
    if (hasEmissiveMap)
        emissive *= texture(emissiveMap, uv).rgb;
    vec3 linearOutput = max(ambient + directLighting + emissive, vec3(0.0));
    vec3 displayOutput = gammaEnabled ? pow(linearOutput, vec3(1.0 / 2.2)) : linearOutput;

    SceneColor = vec4(displayOutput, alpha);
    float luminance = dot(linearOutput, vec3(0.2126, 0.7152, 0.0722));
    BrightColor = luminance > bloomThreshold ? vec4(linearOutput, alpha) : vec4(0.0, 0.0, 0.0, alpha);
}
