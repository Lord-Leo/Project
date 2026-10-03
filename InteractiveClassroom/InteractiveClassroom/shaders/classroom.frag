#version 330 core
layout (location = 0) out vec4 SceneColor;
layout (location = 1) out vec4 BrightColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;
in vec4 FragPosLightSpace;

#define MAX_LIGHTS 64

uniform vec3 lightPositions[MAX_LIGHTS];
uniform vec3 lightColors[MAX_LIGHTS];
uniform float lightIntensities[MAX_LIGHTS];
uniform int numLights;

uniform vec3 dirLightDirection;
uniform vec3 dirLightColor;
uniform float dirLightIntensity;
uniform vec3 viewPos;
uniform vec3 objectColor;
uniform vec3 emissiveColor;
uniform float objectAlpha;
uniform float shininess;
uniform float materialAmbient;
uniform float roomAmbientFactor;
uniform vec3 materialSpecular;
uniform bool useTexture;
uniform sampler2D diffuseTexture;
uniform vec2 uvTiling;
uniform sampler2D shadowMap;
uniform bool shadowsEnabled;
uniform bool gammaEnabled;
uniform float bloomThreshold;

float ShadowCalculation(vec4 fragPosLightSpace, vec3 normal, vec3 lightDirToSource)
{
    vec3 projCoords = fragPosLightSpace.xyz / max(fragPosLightSpace.w, 0.00001);
    projCoords = projCoords * 0.5 + 0.5;
    if (projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0)
        return 0.0;

    float currentDepth = projCoords.z;
    float bias = max(0.0035 * (1.0 - dot(normal, lightDirToSource)), 0.0008);
    float shadow = 0.0;
    vec2 texelSize = 1.0 / vec2(textureSize(shadowMap, 0));
    for (int x = -1; x <= 1; ++x)
        for (int y = -1; y <= 1; ++y)
            shadow += (currentDepth - bias) > texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r ? 1.0 : 0.0;
    return shadow / 9.0;
}

void main()
{
    vec3 base = useTexture ? texture(diffuseTexture, TexCoords * uvTiling).rgb * objectColor : objectColor;
    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 lighting = materialAmbient * roomAmbientFactor * base;

    for (int i = 0; i < numLights; ++i)
    {
        vec3 toLight = lightPositions[i] - FragPos;
        float dist = length(toLight);
        vec3 lightDir = toLight / max(dist, 0.0001);
        float attenuation = lightIntensities[i] / (1.0 + 0.15 * dist + 0.08 * dist * dist);
        float diff = max(dot(norm, lightDir), 0.0);
        vec3 halfwayDir = normalize(lightDir + viewDir);
        float spec = pow(max(dot(norm, halfwayDir), 0.0), shininess);
        lighting += diff * lightColors[i] * base * attenuation;
        lighting += spec * lightColors[i] * materialSpecular * attenuation;
    }

    vec3 lightDirToSource = normalize(-dirLightDirection);
    float diff = max(dot(norm, lightDirToSource), 0.0);
    vec3 halfwayDir = normalize(lightDirToSource + viewDir);
    float spec = pow(max(dot(norm, halfwayDir), 0.0), shininess);
    float shadow = shadowsEnabled ? ShadowCalculation(FragPosLightSpace, norm, lightDirToSource) : 0.0;
    lighting += (1.0 - shadow) * dirLightIntensity * dirLightColor *
                (diff * base + spec * materialSpecular);
    lighting += emissiveColor;

    vec3 linearOutput = max(lighting, vec3(0.0));
    vec3 displayOutput = gammaEnabled ? pow(linearOutput, vec3(1.0 / 2.2)) : linearOutput;
    SceneColor = vec4(displayOutput, objectAlpha);

    float luminance = dot(linearOutput, vec3(0.2126, 0.7152, 0.0722));
    BrightColor = luminance > bloomThreshold ? vec4(linearOutput, objectAlpha) : vec4(0.0, 0.0, 0.0, objectAlpha);
}
