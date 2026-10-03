#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec3 aTangent;
layout (location = 4) in vec3 aBitangent;
layout (location = 5) in vec4 aInstance0;
layout (location = 6) in vec4 aInstance1;
layout (location = 7) in vec4 aInstance2;
layout (location = 8) in vec4 aInstance3;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoords;
out vec4 FragPosLightSpace;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 lightSpaceMatrix;
uniform bool useInstancing;

mat4 EffectiveModel()
{
    return useInstancing ? mat4(aInstance0, aInstance1, aInstance2, aInstance3) : model;
}

void main()
{
    mat4 drawModel = EffectiveModel();
    vec4 worldPos = drawModel * vec4(aPos, 1.0);
    FragPos = worldPos.xyz;
    Normal = normalize(transpose(inverse(mat3(drawModel))) * aNormal);
    TexCoords = aTexCoords;
    FragPosLightSpace = lightSpaceMatrix * worldPos;
    gl_Position = projection * view * worldPos;
}
