#version 330

// Vertex stage of the lighting of shiny 3D models (PbrLighting.frag): Lighting.vert, plus the position in
// world space, which the specular highlight needs to know where the camera looks from.

in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexTangent;
in vec4 vertexColor;

uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;

out vec2 fragTexCoord;
out vec4 fragColor;
out vec3 fragPosition;
out vec3 fragNormal;
out vec4 fragTangent;

void main()
{
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
    fragPosition = vec3(matModel * vec4(vertexPosition, 1.0));
    fragNormal = normalize(mat3(matNormal) * vertexNormal);
    fragTangent = vec4(normalize(mat3(matModel) * vertexTangent.xyz), vertexTangent.w);
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
