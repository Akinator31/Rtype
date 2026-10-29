#version 330

// Vertex stage of the lighting of the 3D models (Lighting.frag).
//
// raylib provides the matrices, the vertex attributes and the maps of the material. Positions, normals and
// tangents are moved to world space, where the lights and the camera are given.

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
    fragTangent = vec4(mat3(matModel) * vertexTangent.xyz, vertexTangent.w);
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
