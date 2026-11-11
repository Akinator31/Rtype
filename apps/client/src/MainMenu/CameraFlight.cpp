#include "MainMenu/CameraFlight.hpp"

#include "MainMenu/Space.hpp"

#include <algorithm>
#include <cmath>

namespace rtype::client
{

namespace
{

[[nodiscard]] render::Vector3 mix(render::Vector3 from, render::Vector3 to, float amount)
{
    return from + ((to - from) * amount);
}

} // namespace

float easeInOut(float time)
{
    const float clamped = std::clamp(time, 0.0f, 1.0f);
    return clamped * clamped * (3.0f - (2.0f * clamped));
}

render::Camera mixViews(const render::Camera &from, const render::Camera &to, float amount)
{
    const render::Vector3 fromForward = normalize(from.target - from.position);
    const render::Vector3 toForward = normalize(to.target - to.position);
    render::Camera view = to;
    view.position = mix(from.position, to.position, amount);
    view.target = view.position + normalize(mix(fromForward, toForward, amount));
    view.up = normalize(mix(from.up, to.up, amount));
    view.fieldOfView = std::lerp(from.fieldOfView, to.fieldOfView, amount);
    return view;
}

render::Camera lookAtView(const render::Camera &view, render::Vector3 position, render::Vector3 point,
                          float turn)
{
    render::Camera placed = view;
    placed.position = position;
    // rotate() turns counterclockwise around its axis: to the left around the top of the view
    placed.target = position + rotate(normalize(point - position), {view.up, -turn});
    return placed;
}

render::Camera approachView(const render::Camera &view, render::Vector3 point, float distance, float turn)
{
    const render::Vector3 back = normalize(view.position - point);
    return lookAtView(view, point + (back * distance), point, turn);
}

} // namespace rtype::client
