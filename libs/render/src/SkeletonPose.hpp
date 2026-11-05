#pragma once

// Private to the render library: the poses of the skeletons of animated models, computed bone by bone, for
// the poses raylib does not make by itself, such as an animation added over two others.

#include <raylib.h>

#include <vector>

namespace rtype::render::detail
{

// The transform of each bone of a skeleton in the space of the model, as raylib keeps the frames of the
// animations.
using BonePoses = std::vector<Transform>;

// The pose at a frame of an animation, between its two nearest frames. The animation loops. None for an
// animation without frames.
[[nodiscard]] BonePoses samplePose(const ModelAnimation &animation, float frame);

// A pose between two poses of a skeleton: `amount` goes from 0 for the first one to 1 for the second one.
[[nodiscard]] BonePoses blendPoses(const BonePoses &first, const BonePoses &second, float amount);

// Adds to a pose what an animation changes at a frame from its first frame, bone by bone, each in the space
// of its parent: a bone the animation does not move keeps its pose. A bone at a scale of 0 in the first frame
// takes the scale of the frame. The pose is left as it is if the animation is not made for this skeleton.
[[nodiscard]] BonePoses addPose(const BonePoses &pose, const ModelAnimation &added, float frame,
                                const ModelSkeleton &skeleton);

// Moves the vertices of a model to a pose of its skeleton. Nothing changes if the pose is not one of this
// skeleton.
void applyBonePoses(const ::Model &model, BonePoses &poses);

} // namespace rtype::render::detail
