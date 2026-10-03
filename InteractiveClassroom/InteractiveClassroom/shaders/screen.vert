#version 330 core
layout (location = 0) in vec2 aPos;

void main()
{
    // Positions are already in normalized device coordinates (-1..1),
    // computed on the CPU side from pixel coordinates each frame.
    gl_Position = vec4(aPos, 0.0, 1.0);
}
