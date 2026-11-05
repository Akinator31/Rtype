#include "FilterShader.hpp"

#include "rtype/log/Logger.hpp"
#include "rtype/shaders/ColorFilter.hpp"

namespace rtype::render::detail
{

namespace
{

// The shader identifies the filters by number, in the order of the enum: reordering the enum must update
// the shader too
static_assert(static_cast<int>(ColorFilter::None) == 0);
static_assert(static_cast<int>(ColorFilter::Protanopia) == 1);
static_assert(static_cast<int>(ColorFilter::Deuteranopia) == 2);
static_assert(static_cast<int>(ColorFilter::Tritanopia) == 3);
static_assert(static_cast<int>(ColorFilter::SimulateProtanopia) == 4);
static_assert(static_cast<int>(ColorFilter::SimulateDeuteranopia) == 5);
static_assert(static_cast<int>(ColorFilter::SimulateTritanopia) == 6);
static_assert(static_cast<int>(ColorFilter::HighContrast) == 7);

} // namespace

FilterShader::FilterShader() : _shader(LoadShaderFromMemory(nullptr, shaders::colorFilterFragment))
{
    _modeLocation = GetShaderLocation(_shader, "mode");
    _available = IsShaderValid(_shader) && _modeLocation >= 0;
    if (!_available) {
        log::Logger("render").warning("Cannot compile the colour filter shader: colour filters are disabled");
    }
}

FilterShader::~FilterShader()
{
    // Also frees what raylib allocated when the compilation failed
    UnloadShader(_shader);
}

bool FilterShader::begin(ColorFilter filter)
{
    if (!_available) {
        return false;
    }
    // The mode numbers follow the order of the enum
    const int mode = static_cast<int>(filter);
    SetShaderValue(_shader, _modeLocation, &mode, SHADER_UNIFORM_INT);
    BeginShaderMode(_shader);
    return true;
}

} // namespace rtype::render::detail
