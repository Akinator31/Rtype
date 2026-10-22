#version 330

// Vertex stage of the energy shots and effects (Energy.frag).
//
// The models animate themselves: each mesh stores its animation in its vertex colours (COLOR_0 in glTF),
// written by the asset generator. White, which raylib uses for meshes without colours, means no animation:
// - red minus green: spin around the local X axis, the axis of the shot, 1 for 12 radians per second;
// - blue: radial pulse, 1 for none, down to 0 for 50 % of the radius;
// - alpha: unused.
// time (seconds) is set by the renderer. The noise of Energy.frag is sampled in model space before the
// animation, so that its pattern travels with the shot instead of sliding over it.

in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;

uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;
uniform float time;

out vec2 fragTexCoord;
out vec3 fragPosition;
out vec3 fragLocalPosition;
out vec3 fragNormal;

const float maxSpin = 12.0;
const float maxPulse = 0.5;
const float pulseFrequency = 9.0;

void main()
{
    float angle = (vertexColor.r - vertexColor.g) * maxSpin * time;
    float pulse = (1.0 - vertexColor.b) * maxPulse * sin(time * pulseFrequency - vertexPosition.x * 1.5);
    mat2 rotation = mat2(cos(angle), sin(angle), -sin(angle), cos(angle));
    vec3 position = vertexPosition;
    vec3 normal = vertexNormal;
    position.yz = rotation * position.yz * (1.0 + pulse);
    normal.yz = rotation * normal.yz;

    fragTexCoord = vertexTexCoord;
    fragLocalPosition = vertexPosition;
    fragPosition = vec3(matModel * vec4(position, 1.0));
    fragNormal = normalize(mat3(matNormal) * normal);
    gl_Position = mvp * vec4(position, 1.0);
}
