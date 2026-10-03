#version 330 core
out vec4 FragColor;
in vec2 TexCoords;
uniform sampler2D sceneTexture;
uniform sampler2D bloomTexture;
uniform bool bloomEnabled;
uniform float bloomStrength;
uniform float exposure;
uniform int toneMappingMode;
uniform bool gammaEnabled;

vec3 ACESApprox(vec3 colour)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((colour * (a * colour + b)) / (colour * (c * colour + d) + e), 0.0, 1.0);
}

void main()
{
    vec3 hdr = texture(sceneTexture, TexCoords).rgb;
    if (bloomEnabled)
        hdr += texture(bloomTexture, TexCoords).rgb * bloomStrength;
    hdr *= exposure;

    vec3 mapped;
    if (toneMappingMode == 0)
        mapped = hdr / (hdr + vec3(1.0));
    else if (toneMappingMode == 1)
        mapped = ACESApprox(hdr);
    else
        mapped = clamp(hdr, 0.0, 1.0);

    if (gammaEnabled)
        mapped = pow(max(mapped, vec3(0.0)), vec3(1.0 / 2.2));
    FragColor = vec4(mapped, 1.0);
}
