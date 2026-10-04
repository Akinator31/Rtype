#pragma once

#include "rtype/render/Handle.hpp"

#include <filesystem>
#include <memory>
#include <string>

namespace rtype::render
{

class Window;

/// Handle of a texture loaded by the ResourceManager.
using TextureId = Handle<struct TextureTag>;
/// Handle of a font loaded by the ResourceManager.
using FontId = Handle<struct FontTag>;

/// Loads the graphics resources of the game (textures, fonts) and frees them when it is destroyed.
///
/// Each file is loaded once: loading the same path again returns the same handle. The game refers to
/// resources by their handle only, which keeps raylib out of the rest of the code.
///
/// Graphics resources live in the window's graphics context, so the manager needs an open window and must
/// be destroyed before it. Declare the manager after the window:
/// @code
/// Window window;
/// ResourceManager resources(window, "assets");
/// const TextureId player = resources.loadTexture("sprites/player.png");
/// @endcode
class ResourceManager
{
private:
    struct Impl;
    std::unique_ptr<Impl> _impl;

    // The renderer reads the loaded resources to draw them
    friend class Renderer;

public:
    /// Creates an empty manager.
    /// @param window The open window. It must outlive the manager.
    /// @param assetsDirectory Directory that the paths given to the load functions are relative to.
    ResourceManager(const Window &window, std::filesystem::path assetsDirectory);
    ~ResourceManager();

    ResourceManager(const ResourceManager &) = delete;
    ResourceManager &operator=(const ResourceManager &) = delete;
    ResourceManager(ResourceManager &&) noexcept;
    ResourceManager &operator=(ResourceManager &&) noexcept;

    /// Loads an image file (PNG, BMP, QOI...) as a texture, unless it is already loaded.
    /// @param path Path of the file, relative to the assets directory, for instance "sprites/player.png".
    /// @throws std::runtime_error if the file is missing or is not a supported image.
    [[nodiscard]] TextureId loadTexture(const std::string &path);

    /// Loads a font file (TTF, OTF...) at the given size, unless it is already loaded at this size.
    /// @param path Path of the file, relative to the assets directory, for instance "fonts/main.ttf".
    /// @param size Height of the characters in pixels. Text drawn at this size is the sharpest.
    /// @throws std::runtime_error if the file is missing or is not a supported font.
    [[nodiscard]] FontId loadFont(const std::string &path, int size = 32);
};

} // namespace rtype::render
