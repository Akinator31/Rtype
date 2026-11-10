#include "rtype/render/PassOrder.hpp"

#include <stdexcept>

namespace rtype::render
{

namespace
{

// Why a pass cannot start: the one that is not ended yet
[[noreturn]] void refuseStart(Pass current)
{
    if (current == Pass::Frame) {
        throw std::logic_error("A frame is already started: call endFrame() first");
    }
    throw std::logic_error("An eye is already started: call endEye() first");
}

} // namespace

void PassOrder::beginFrame()
{
    if (_current != Pass::None) {
        refuseStart(_current);
    }
    _current = Pass::Frame;
}

void PassOrder::endFrame()
{
    if (_current != Pass::Frame) {
        throw std::logic_error("No frame to end: call beginFrame() first");
    }
    _current = Pass::None;
}

void PassOrder::beginEye()
{
    if (_current != Pass::None) {
        refuseStart(_current);
    }
    _current = Pass::Eye;
}

void PassOrder::endEye()
{
    if (_current != Pass::Eye) {
        throw std::logic_error("No eye to end: call beginEye() first");
    }
    _current = Pass::None;
}

void PassOrder::requireFrame() const
{
    if (_current == Pass::Eye) {
        throw std::logic_error("Drawing for a frame in an eye: draw it between beginFrame() and endFrame()");
    }
    if (_current != Pass::Frame) {
        throw std::logic_error("Drawing outside of a frame: call beginFrame() first");
    }
}

void PassOrder::requireEye() const
{
    if (_current != Pass::Eye) {
        throw std::logic_error("Drawing for an eye outside of one: call beginEye() first");
    }
}

} // namespace rtype::render
