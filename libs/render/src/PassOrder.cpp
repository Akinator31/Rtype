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
    if (current == Pass::Board) {
        throw std::logic_error("A board is already started: call endBoard() first");
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
    if (_current == Pass::Board) {
        throw std::logic_error("The board of the eye is not ended: call endBoard() first");
    }
    if (_current != Pass::Eye) {
        throw std::logic_error("No eye to end: call beginEye() first");
    }
    _current = Pass::None;
}

void PassOrder::beginBoard()
{
    if (_current == Pass::Board) {
        refuseStart(_current);
    }
    if (_current != Pass::Eye) {
        throw std::logic_error("A board is drawn in an eye: call beginEye() first");
    }
    _current = Pass::Board;
}

void PassOrder::endBoard()
{
    if (_current != Pass::Board) {
        throw std::logic_error("No board to end: call beginBoard() first");
    }
    _current = Pass::Eye;
}

void PassOrder::requireFrame() const
{
    if (_current == Pass::Eye) {
        throw std::logic_error(
            "Drawing for a frame in an eye: draw it on a board, or between beginFrame() and "
            "endFrame()");
    }
    if (_current != Pass::Frame && _current != Pass::Board) {
        throw std::logic_error("Drawing outside of a frame: call beginFrame() first");
    }
}

void PassOrder::requireEye() const
{
    if (_current == Pass::Board) {
        throw std::logic_error("Drawing for an eye on its board: call endBoard() first");
    }
    if (_current != Pass::Eye) {
        throw std::logic_error("Drawing for an eye outside of one: call beginEye() first");
    }
}

} // namespace rtype::render
