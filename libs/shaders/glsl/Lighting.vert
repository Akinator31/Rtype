#version 330

// Vertex stage of the lighting of the 3D models (Lighting.frag).
//
// raylib provides the matrices, the vertex attributes and the maps of the material. Positions, normals and
// tangents are moved to world space, where the lights and the camera are given.
//
// An animated mesh (skinned is 1) is first moved by its bones: each vertex by the weighted matrices of up to
// four bones, from the pose the renderer gave (boneMatrices, GPU skinning).

// MaxShaderBones in SkeletonPose.hpp
#define MAX_BONES 128

in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexTangent;
in vec4 vertexColor;
in vec4 vertexBoneIndices;
in vec4 vertexBoneWeights;

uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;
uniform mat4 boneMatrices[MAX_BONES];
uniform int skinned;

out vec2 fragTexCoord;
out vec4 fragColor;
out vec3 fragPosition;
out vec3 fragNormal;
out vec4 fragTangent;

void main()
{
    vec4 position = vec4(vertexPosition, 1.0);
    vec3 normal = vertexNormal;
    vec3 tangent = vertexTangent.xyz;
    if (skinned == 1) {
        ivec4 bones = ivec4(vertexBoneIndices);
        mat4 skin = boneMatrices[bones.x] * vertexBoneWeights.x + boneMatrices[bones.y] * vertexBoneWeights.y +
                    boneMatrices[bones.z] * vertexBoneWeights.z + boneMatrices[bones.w] * vertexBoneWeights.w;
        position = skin * position;
        normal = mat3(skin) * normal;
        tangent = mat3(skin) * tangent;
    }

    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
    fragPosition = vec3(matModel * position);
    fragNormal = normalize(mat3(matNormal) * normal);
    fragTangent = vec4(mat3(matModel) * tangent, vertexTangent.w);
    gl_Position = mvp * position;
}
