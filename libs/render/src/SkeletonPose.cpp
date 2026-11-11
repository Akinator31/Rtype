#include "SkeletonPose.hpp"

#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace rtype::render::detail
{

namespace
{

Transform interpolate(const Transform &first, const Transform &second, float amount)
{
    return {Vector3Lerp(first.translation, second.translation, amount),
            QuaternionSlerp(first.rotation, second.rotation, amount),
            Vector3Lerp(first.scale, second.scale, amount)};
}

float divideOrZero(float value, float divisor)
{
    return divisor == 0.0f ? 0.0f : value / divisor;
}

Vector3 divideOrZero(Vector3 value, Vector3 divisor)
{
    return {divideOrZero(value.x, divisor.x), divideOrZero(value.y, divisor.y),
            divideOrZero(value.z, divisor.z)};
}

// The scale `added` has over `reference`, applied to `scale`: `added` itself where `reference` is 0
float addScale(float scale, float reference, float added)
{
    return reference == 0.0f ? added : scale * (added / reference);
}

// raylib builds the bones of a frame in the space of the model from their parent, in order, and skips a bone
// whose parent comes after it (BuildPoseFromParentJoints())
bool hasParentBefore(const ModelSkeleton &skeleton, int bone)
{
    const int parent = skeleton.bones[bone].parent;
    return parent >= 0 && parent < bone;
}

// Each bone in the space of its parent: the reverse of how raylib builds the frames
BonePoses toParentSpace(const BonePoses &poses, const ModelSkeleton &skeleton)
{
    BonePoses local = poses;
    for (int bone = 0; bone < skeleton.boneCount; ++bone) {
        if (!hasParentBefore(skeleton, bone)) {
            continue;
        }
        const Transform &parent = poses[skeleton.bones[bone].parent];
        const Quaternion unturn = QuaternionInvert(parent.rotation);
        const Transform &pose = poses[bone];
        local[bone] = {divideOrZero(Vector3RotateByQuaternion(
                                        Vector3Subtract(pose.translation, parent.translation), unturn),
                                    parent.scale),
                       QuaternionMultiply(unturn, pose.rotation), divideOrZero(pose.scale, parent.scale)};
    }
    return local;
}

// Each bone in the space of the model, as raylib builds the frames
BonePoses toModelSpace(const BonePoses &local, const ModelSkeleton &skeleton)
{
    BonePoses poses = local;
    for (int bone = 0; bone < skeleton.boneCount; ++bone) {
        if (!hasParentBefore(skeleton, bone)) {
            continue;
        }
        const Transform &parent = poses[skeleton.bones[bone].parent];
        const Transform &pose = local[bone];
        poses[bone] = {Vector3Add(Vector3RotateByQuaternion(Vector3Multiply(pose.translation, parent.scale),
                                                            parent.rotation),
                                  parent.translation),
                       QuaternionMultiply(parent.rotation, pose.rotation),
                       Vector3Multiply(pose.scale, parent.scale)};
    }
    return poses;
}

bool isPoseOf(const BonePoses &poses, const ModelSkeleton &skeleton)
{
    return skeleton.bones != nullptr && poses.size() == static_cast<std::size_t>(skeleton.boneCount);
}

} // namespace

BonePoses samplePose(const ModelAnimation &animation, float frame)
{
    const int count = animation.keyframeCount;
    if (count <= 0 || animation.keyframePoses == nullptr) {
        return {};
    }
    const float looped = std::fmod(std::max(frame, 0.0f), static_cast<float>(count));
    const int current = std::min(static_cast<int>(looped), count - 1);
    const int next = (current + 1) % count;
    const float amount = looped - static_cast<float>(current);
    BonePoses poses(static_cast<std::size_t>(animation.boneCount));
    for (int bone = 0; bone < animation.boneCount; ++bone) {
        poses[bone] =
            interpolate(animation.keyframePoses[current][bone], animation.keyframePoses[next][bone], amount);
    }
    return poses;
}

BonePoses blendPoses(const BonePoses &first, const BonePoses &second, float amount)
{
    if (first.size() != second.size()) {
        return second;
    }
    BonePoses poses(second.size());
    for (std::size_t bone = 0; bone < poses.size(); ++bone) {
        poses[bone] = interpolate(first[bone], second[bone], amount);
    }
    return poses;
}

BonePoses addPose(const BonePoses &pose, const ModelAnimation &added, float frame,
                  const ModelSkeleton &skeleton)
{
    const BonePoses first = samplePose(added, 0.0f);
    if (!isPoseOf(pose, skeleton) || !isPoseOf(first, skeleton)) {
        return pose;
    }
    const BonePoses base = toParentSpace(pose, skeleton);
    const BonePoses reference = toParentSpace(first, skeleton);
    const BonePoses moved = toParentSpace(samplePose(added, frame), skeleton);
    BonePoses local(base.size());
    for (std::size_t bone = 0; bone < local.size(); ++bone) {
        const Transform &from = reference[bone];
        const Transform &to = moved[bone];
        local[bone] = {Vector3Add(base[bone].translation, Vector3Subtract(to.translation, from.translation)),
                       QuaternionMultiply(base[bone].rotation,
                                          QuaternionMultiply(QuaternionInvert(from.rotation), to.rotation)),
                       {addScale(base[bone].scale.x, from.scale.x, to.scale.x),
                        addScale(base[bone].scale.y, from.scale.y, to.scale.y),
                        addScale(base[bone].scale.z, from.scale.z, to.scale.z)}};
    }
    return toModelSpace(local, skeleton);
}

void applyBonePoses(const ::Model &model, BonePoses &poses)
{
    if (!isPoseOf(poses, model.skeleton)) {
        return;
    }
    // A single frame, which raylib poses the model in
    Transform *frame = poses.data();
    ModelAnimation animation{};
    animation.boneCount = model.skeleton.boneCount;
    animation.keyframeCount = 1;
    animation.keyframePoses = &frame;
    UpdateModelAnimation(model, animation, 0.0f);
}

bool isSkinned(const ::Mesh &mesh)
{
    return mesh.boneIndices != nullptr && mesh.boneWeights != nullptr;
}

void setBoneMatrices(const ::Shader &shader, const ::Model &model)
{
    const int location = shader.locs != nullptr ? shader.locs[SHADER_LOC_MATRIX_BONETRANSFORMS] : -1;
    if (location < 0 || model.boneMatrices == nullptr || model.skeleton.boneCount <= 0) {
        return;
    }
    rlEnableShader(shader.id);
    rlSetUniformMatrices(location, model.boneMatrices, std::min(model.skeleton.boneCount, MaxShaderBones));
}

} // namespace rtype::render::detail
