#pragma once

// Private to the render library: the shader applying the colour filters when the frame is copied to the
// window.

#include "rtype/render/ColorFilter.hpp"

#include <raylib.h>

namespace rtype::render::detail
{

/// Owns the colour filter shader. If it cannot be compiled (an old graphics card without OpenGL 3.3, for
/// instance), a warning is logged and the filters are disabled: the game is drawn without them.
class FilterShader
{
private:
    Shader _shader{};
    int _modeLocation = -1;
    bool _available = false;

public:
    FilterShader();
    ~FilterShader();

    FilterShader(const FilterShader &) = delete;
    FilterShader &operator=(const FilterShader &) = delete;
    FilterShader(FilterShader &&) = delete;
    FilterShader &operator=(FilterShader &&) = delete;

    /// Starts drawing with the filter, if there is one to apply. Returns whether the shader is in use, so
    /// that the caller ends it with EndShaderMode().
    [[nodiscard]] bool begin(ColorFilter filter);
};

} // namespace rtype::render::detail
