#version 330

// Fragment stage of the lighting of the 3D models. The renderer sets lightDirection (normalised, the
// direction the light travels in), lightColor and ambientColor (sRGB, 0 to 1) and useNormalMap (0 or 1).
//
// Lambert lighting, computed on linear colours (the texture and the light colours are sRGB, the vertex
// colours are already linear, as in glTF) so that the shadow fades naturally. The normal map adds the relief,
// but the shape of the model decides which side is in the dark: otherwise the relief would catch the light on
// the night side.

in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragNormal;
in vec4 fragTangent;

uniform sampler2D texture0;
uniform sampler2D texture2;
uniform vec4 colDiffuse;
uniform int useNormalMap;
uniform vec3 lightDirection;
uniform vec3 lightColor;
uniform vec3 ambientColor;

out vec4 finalColor;

vec3 toLinear(vec3 color)
{
    return pow(color, vec3(2.2));
}

vec3 toSrgb(vec3 color)
{
    return pow(color, vec3(1.0 / 2.2));
}

// The normal of the surface bent by the relief of the normal map, which is given in tangent space
vec3 applyNormalMap(vec3 surface)
{
    vec3 tangent = normalize(fragTangent.xyz - surface * dot(surface, fragTangent.xyz));
    vec3 bitangent = cross(surface, tangent) * fragTangent.w;
    vec3 relief = texture(texture2, fragTexCoord).rgb * 2.0 - 1.0;
    return normalize(mat3(tangent, bitangent, surface) * relief);
}

// How much of the directional light reaches the point, from 0 to 1
float computeDiffuse(vec3 surface, vec3 normal)
{
    vec3 toLight = -lightDirection;
    return max(dot(normal, toLight), 0.0) * smoothstep(-0.05, 0.2, dot(surface, toLight));
}

void main()
{
    vec4 texel = texture(texture0, fragTexCoord) * colDiffuse;
    vec3 albedo = toLinear(texel.rgb) * fragColor.rgb;
    vec3 surface = normalize(fragNormal);
    vec3 normal = useNormalMap == 1 ? applyNormalMap(surface) : surface;
    vec3 light = toLinear(ambientColor) + toLinear(lightColor) * computeDiffuse(surface, normal);
    finalColor = vec4(toSrgb(albedo * light), texel.a * fragColor.a);
}
