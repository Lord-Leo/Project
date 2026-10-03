#version 330 core
out vec4 FragColor;
in vec2 TexCoords;
uniform sampler2D image;
uniform bool horizontal;

void main()
{
    const float weights[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);
    vec2 texel = 1.0 / vec2(textureSize(image, 0));
    vec3 result = texture(image, TexCoords).rgb * weights[0];
    for (int i = 1; i < 5; ++i)
    {
        vec2 offset = horizontal ? vec2(texel.x * float(i), 0.0) : vec2(0.0, texel.y * float(i));
        result += texture(image, TexCoords + offset).rgb * weights[i];
        result += texture(image, TexCoords - offset).rgb * weights[i];
    }
    FragColor = vec4(result, 1.0);
}
