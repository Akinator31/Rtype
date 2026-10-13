#version 330

// Fragment shader applied to every pixel of the frame when it is copied to the window. raylib provides the
// vertex shader and the texture (texture0) of the frame.
//
// Colour blindness:
// - simulation uses the matrices of Machado, Oliveira and Fernandes (2009) for a complete deficiency. They
//   apply to linear RGB, so colours are converted from sRGB first, and back afterwards;
// - correction ("daltonisation") computes the difference between the original and the simulated colour,
//   which is the information the player cannot see, and moves it to the channels they can see: towards green
//   and blue for protanopia and deuteranopia (red-green confusion), towards red and green for tritanopia
//   (blue-yellow confusion). It makes confused colours easier to tell apart, it does not restore them.
//
// The uniform "mode" selects the filter: 0 none, then the order of render::ColorFilter (1 protanopia ...
// 7 high contrast).
//
// GLSL fills matrices column by column: the matrices of the paper are written transposed, so that each group
// of three numbers is what red, green and blue (in this order) become.

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform int mode;

out vec4 finalColor;

const mat3 protanopia = mat3(
    0.152286, 0.114503, -0.003882,
    1.052583, 0.786281, -0.048116,
    -0.204868, 0.099216, 1.051998);
const mat3 deuteranopia = mat3(
    0.367322, 0.280085, -0.011820,
    0.860646, 0.672501, 0.042940,
    -0.227968, 0.047413, 0.968881);
const mat3 tritanopia = mat3(
    1.255528, -0.078411, 0.004733,
    -0.076749, 0.930809, 0.691367,
    -0.178779, 0.147602, 0.303900);

vec3 toLinear(vec3 color)
{
    return mix(color / 12.92, pow((color + 0.055) / 1.055, vec3(2.4)), step(vec3(0.04045), color));
}

vec3 toSrgb(vec3 color)
{
    return mix(color * 12.92, 1.055 * pow(color, vec3(1.0 / 2.4)) - 0.055, step(vec3(0.0031308), color));
}

// Both work on linear colours
vec3 simulate(vec3 color, mat3 deficiency)
{
    return clamp(deficiency * color, 0.0, 1.0);
}

vec3 correct(vec3 color, mat3 deficiency, bool blueYellow)
{
    vec3 error = color - simulate(color, deficiency);
    vec3 shift = blueYellow ? vec3(error.r + 0.7 * error.b, error.g + 0.7 * error.b, 0.0)
                            : vec3(0.0, error.g + 0.7 * error.r, error.b + 0.7 * error.r);
    return clamp(color + shift, 0.0, 1.0);
}

vec3 highContrast(vec3 color)
{
    float grey = dot(color, vec3(0.299, 0.587, 0.114));
    vec3 saturated = mix(vec3(grey), color, 1.4);
    return clamp((saturated - 0.5) * 1.5 + 0.5, 0.0, 1.0);
}

void main()
{
    vec4 texel = texture(texture0, fragTexCoord) * colDiffuse * fragColor;
    vec3 color = texel.rgb;
    vec3 linear = toLinear(color);
    if (mode == 1) color = toSrgb(correct(linear, protanopia, false));
    else if (mode == 2) color = toSrgb(correct(linear, deuteranopia, false));
    else if (mode == 3) color = toSrgb(correct(linear, tritanopia, true));
    else if (mode == 4) color = toSrgb(simulate(linear, protanopia));
    else if (mode == 5) color = toSrgb(simulate(linear, deuteranopia));
    else if (mode == 6) color = toSrgb(simulate(linear, tritanopia));
    else if (mode == 7) color = highContrast(color);
    finalColor = vec4(color, texel.a);
}
