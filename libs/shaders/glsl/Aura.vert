#version 330

// Vertex stage of the aura of the 3D models (Aura.frag): a shell around the model, its vertices pushed out
// along their normals by `thickness` world units, so that it outlines the silhouette whatever the scale of
// the model. raylib provides the matrices and the vertex attributes; the renderer sets thickness. An animated
// mesh (skinned is 1) is first moved by its bones, as in Lighting.vert.

// MaxShaderBones in SkeletonPose.hpp
#define MAX_BONES 128

in vec3 vertexPosition;
in vec3 vertexNormal;
in vec4 vertexBoneIndices;
in vec4 vertexBoneWeights;

uniform mat4 matModel;
uniform mat4 matNormal;
uniform mat4 matView;
uniform mat4 matProjection;
uniform mat4 boneMatrices[MAX_BONES];
uniform int skinned;
uniform float thickness;

out vec3 fragPosition;
out vec3 fragNormal;

void main()
{
    vec4 posed = vec4(vertexPosition, 1.0);
    vec3 posedNormal = vertexNormal;
    if (skinned == 1) {
        ivec4 bones = ivec4(vertexBoneIndices);
        mat4 skin = boneMatrices[bones.x] * vertexBoneWeights.x + boneMatrices[bones.y] * vertexBoneWeights.y +
                    boneMatrices[bones.z] * vertexBoneWeights.z + boneMatrices[bones.w] * vertexBoneWeights.w;
        posed = skin * posed;
        posedNormal = mat3(skin) * posedNormal;
    }
    vec3 normal = normalize(mat3(matNormal) * posedNormal);
    vec3 position = vec3(matModel * posed) + normal * thickness;
    fragPosition = position;
    fragNormal = normal;
    gl_Position = matProjection * matView * vec4(position, 1.0);
}
