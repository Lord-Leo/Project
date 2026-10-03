#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 5) in vec4 aInstance0;
layout (location = 6) in vec4 aInstance1;
layout (location = 7) in vec4 aInstance2;
layout (location = 8) in vec4 aInstance3;

uniform mat4 model;
uniform mat4 lightSpaceMatrix;
uniform bool useInstancing;

void main()
{
    mat4 drawModel = useInstancing ? mat4(aInstance0, aInstance1, aInstance2, aInstance3) : model;
    gl_Position = lightSpaceMatrix * drawModel * vec4(aPos, 1.0);
}
