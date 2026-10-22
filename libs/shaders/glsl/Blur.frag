#version 330

// Fragment shader of the glow of the energy models (render::Renderer::drawGlow()): blurs their image along
// one direction. Drawn horizontally then vertically, it gives a Gaussian blur for a fraction of the cost of a
// 2D one. The renderer sets direction (one texel of the image, along x or along y) and gain (how much brighter
// the result is: 1 between the passes, more for the last one). raylib provides the vertex shader and the
// image (texture0).
//
// Nine samples, 1.5 texels apart: with the bilinear filtering of the image, each one blends two texels, so
// that each pass spreads the light about 6 texels further on each side.

in vec2 fragTexCoord;

uniform sampler2D texture0;
uniform vec2 direction;
uniform float gain;

out vec4 finalColor;

const float weights[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);
const float spacing = 1.5;

void main()
{
    vec3 sum = texture(texture0, fragTexCoord).rgb * weights[0];
    for (int i = 1; i < 5; ++i) {
        vec2 offset = direction * float(i) * spacing;
        vec3 pair = texture(texture0, fragTexCoord + offset).rgb + texture(texture0, fragTexCoord - offset).rgb;
        sum += pair * weights[i];
    }
    finalColor = vec4(sum * gain, 1.0);
}
