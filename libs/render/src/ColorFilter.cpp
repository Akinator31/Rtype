#include "rtype/render/ColorFilter.hpp"

namespace rtype::render
{

std::string_view colorFilterName(ColorFilter filter)
{
    switch (filter) {
    case ColorFilter::None:
        return "None";
    case ColorFilter::Protanopia:
        return "Protanopia";
    case ColorFilter::Deuteranopia:
        return "Deuteranopia";
    case ColorFilter::Tritanopia:
        return "Tritanopia";
    case ColorFilter::SimulateProtanopia:
        return "Simulate protanopia";
    case ColorFilter::SimulateDeuteranopia:
        return "Simulate deuteranopia";
    case ColorFilter::SimulateTritanopia:
        return "Simulate tritanopia";
    case ColorFilter::HighContrast:
        return "High contrast";
    }
    return "Unknown";
}

} // namespace rtype::render
