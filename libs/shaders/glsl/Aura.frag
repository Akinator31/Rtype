#version 330

// Fragment stage of the aura of the 3D models: a light of one colour on the shell of Aura.vert, added to what
// is behind it. It is bright where the shell grazes the view, at the edges of the silhouette, and clear where
// it faces the camera, so that the model shows through it (Fresnel). It breathes slowly, a little, so that it
// reads as a light rather than as a drawn outline. The renderer sets:
// - auraColor (sRGB, 0 to 1), its alpha how bright the aura is;
// - viewPosition and viewDirection (world space), and orthographic (0 or 1): where the camera looks from;
// - time, in seconds: the breathing.

in vec3 fragPosition;
in vec3 fragNormal;

uniform vec4 auraColor;
uniform vec3 viewPosition;
uniform vec3 viewDirection;
uniform int orthographic;
uniform float time;

out vec4 finalColor;

// How tight the light is on the edges: higher keeps it on a thinner line of the silhouette
const float edgePower = 3.5;
// How bright the aura is at most: a light touch, the model stays the first thing seen
const float brightness = 0.55;
// The breathing: its speed, in radians per second, and how much of the light it takes away at its lowest
const float breathSpeed = 2.0;
const float breathDepth = 0.15;

void main()
{
    vec3 normal = normalize(fragNormal);
    vec3 toView = orthographic == 1 ? -viewDirection : normalize(viewPosition - fragPosition);
    float facing = abs(dot(normal, toView));
    float edge = pow(1.0 - facing, edgePower);

    float breath = 1.0 - breathDepth * (0.5 + 0.5 * sin(time * breathSpeed));
    float light = edge * brightness * breath * auraColor.a;

    // Added over what is behind it (BLEND_ADDITIVE): the alpha weighs the colour
    finalColor = vec4(auraColor.rgb, clamp(light, 0.0, 1.0));
}
