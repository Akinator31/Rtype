#version 330

// Vertex stage of the aura of the 3D models (Aura.frag): a shell around the model, its vertices pushed out
// along their normals by `thickness` world units, so that it outlines the silhouette whatever the scale of
// the model. raylib provides the matrices and the vertex attributes; the renderer sets thickness.

in vec3 vertexPosition;
in vec3 vertexNormal;

uniform mat4 matModel;
uniform mat4 matNormal;
uniform mat4 matView;
uniform mat4 matProjection;
uniform float thickness;

out vec3 fragPosition;
out vec3 fragNormal;

void main()
{
    vec3 normal = normalize(mat3(matNormal) * vertexNormal);
    vec3 position = vec3(matModel * vec4(vertexPosition, 1.0)) + normal * thickness;
    fragPosition = position;
    fragNormal = normal;
    gl_Position = matProjection * matView * vec4(position, 1.0);
}
