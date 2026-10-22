#version 330

// Fragment stage of the energy shots and effects: shots, beams, trails, plasma orbs, explosion flash. Meant to
// be drawn with additive blending (BLEND_ADDITIVE) and without writing depth, after the opaque models: the
// layers of a shot (core, shell, trail) then add up into a glow, and the alpha of the tint fades it out.
//
// The renderer sets viewPosition (world space) and time (seconds). Optional, 0 by default:
// - boost: extra brightness, for a charged shot, the firing phase of the boss beam or a hit flash;
// - dissolve: from 0 to 1, the noise eats the shot with a hot edge, to make it appear or vanish.
//
// texture0 is the colour baked in the model, black where nothing glows (the tips of the trails).
// noiseMap is images/fx/energy_noise.png, bound through MATERIAL_MAP_HEIGHT. The surface facing the camera
// turns white hot, the silhouette keeps the saturated colour, two layers of noise make the energy flicker and
// pulses of light run along the shot, towards its front.

in vec2 fragTexCoord;
in vec3 fragPosition;
in vec3 fragLocalPosition;
in vec3 fragNormal;

uniform sampler2D texture0;
uniform sampler2D noiseMap;
uniform vec4 colDiffuse;
uniform vec3 viewPosition;
uniform float time;
uniform float boost;
uniform float dissolve;

out vec4 finalColor;

const float flowSpeed = 1.5;

// Brightness of the energy at the point, from 0.5 to 1.5 around 1, changing over time
float computeFlicker()
{
    vec2 firstOffset = vec2(-time * flowSpeed * 0.35, time * 0.05);
    vec2 secondOffset = vec2(time * flowSpeed * 0.2, -time * 0.11);
    float first = texture(noiseMap, fragLocalPosition.xy * 0.18 + firstOffset).r;
    float second = texture(noiseMap, fragLocalPosition.zx * 0.23 + secondOffset).g;
    return 0.5 + 0.5 * (first + second);
}

// Pulses of light running along the local X axis, towards the front of the shot
float computePulses()
{
    float wave = sin(fragLocalPosition.x * 2.2 - time * 14.0);
    return 0.85 + 0.4 * smoothstep(0.55, 1.0, wave);
}

void main()
{
    vec3 base = texture(texture0, fragTexCoord).rgb * colDiffuse.rgb;
    float glow = max(base.r, max(base.g, base.b));
    float strength = 1.0 + boost;

    vec3 toView = normalize(viewPosition - fragPosition);
    float facing = abs(dot(normalize(fragNormal), toView));
    float core = pow(facing, 3.0);
    float rim = pow(1.0 - facing, 2.5);

    vec3 color = base * computeFlicker() * computePulses() * (0.6 + 0.9 * rim);
    color += (vec3(1.0) - color) * clamp(core * 0.55 * strength, 0.0, 1.0) * glow;

    if (dissolve > 0.0) {
        float margin = texture(noiseMap, fragTexCoord * 1.7).b - dissolve;
        if (margin < 0.0) {
            discard;
        }
        color += vec3(1.0, 0.8, 0.5) * smoothstep(0.08, 0.0, margin) * glow * 2.0;
    }
    finalColor = vec4(color * strength, colDiffuse.a);
}
