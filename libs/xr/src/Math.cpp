#include "rtype/xr/Math.hpp"

#include <cmath>

namespace rtype::xr
{

Matrix multiply(const Matrix &a, const Matrix &b)
{
    Matrix result;
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            float sum = 0.0f;
            for (int index = 0; index < 4; ++index) {
                sum += a.at(row, index) * b.at(index, column);
            }
            result.values[(column * 4) + row] = sum;
        }
    }
    return result;
}

Matrix toMatrix(const Pose &pose)
{
    const Quaternion &q = pose.orientation;
    const float xx = q.x * q.x;
    const float yy = q.y * q.y;
    const float zz = q.z * q.z;
    const float xy = q.x * q.y;
    const float xz = q.x * q.z;
    const float yz = q.y * q.z;
    const float wx = q.w * q.x;
    const float wy = q.w * q.y;
    const float wz = q.w * q.z;
    // Column by column: the axes x, y and z of the pose, then its position
    Matrix result;
    result.values[0] = 1.0f - (2.0f * (yy + zz));
    result.values[1] = 2.0f * (xy + wz);
    result.values[2] = 2.0f * (xz - wy);
    result.values[4] = 2.0f * (xy - wz);
    result.values[5] = 1.0f - (2.0f * (xx + zz));
    result.values[6] = 2.0f * (yz + wx);
    result.values[8] = 2.0f * (xz + wy);
    result.values[9] = 2.0f * (yz - wx);
    result.values[10] = 1.0f - (2.0f * (xx + yy));
    result.values[12] = pose.position.x;
    result.values[13] = pose.position.y;
    result.values[14] = pose.position.z;
    return result;
}

Matrix rigidInverse(const Matrix &matrix)
{
    // [R | t] inverted is [R^T | -R^T t]
    Matrix result;
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            result.values[(column * 4) + row] = matrix.at(column, row);
        }
        result.values[12 + row] =
            -((matrix.at(0, row) * matrix.values[12]) + (matrix.at(1, row) * matrix.values[13]) +
              (matrix.at(2, row) * matrix.values[14]));
    }
    return result;
}

Vector3 transformPoint(const Matrix &matrix, Vector3 point)
{
    const auto &m = matrix.values;
    return {(m[0] * point.x) + (m[4] * point.y) + (m[8] * point.z) + m[12],
            (m[1] * point.x) + (m[5] * point.y) + (m[9] * point.z) + m[13],
            (m[2] * point.x) + (m[6] * point.y) + (m[10] * point.z) + m[14]};
}

Matrix projection(const FieldOfView &fieldOfView, float nearest, float farthest)
{
    const float left = std::tan(fieldOfView.left);
    const float right = std::tan(fieldOfView.right);
    const float up = std::tan(fieldOfView.up);
    const float down = std::tan(fieldOfView.down);
    Matrix result{{}};
    result.values[0] = 2.0f / (right - left);
    result.values[5] = 2.0f / (up - down);
    result.values[8] = (right + left) / (right - left);
    result.values[9] = (up + down) / (up - down);
    result.values[10] = -(farthest + nearest) / (farthest - nearest);
    result.values[11] = -1.0f;
    result.values[14] = -(2.0f * farthest * nearest) / (farthest - nearest);
    return result;
}

} // namespace rtype::xr
