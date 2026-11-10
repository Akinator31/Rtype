#include "Vr/VrView.hpp"

#include "Assets.hpp"
#include "Vr/CockpitLook.hpp"
#include "rtype/log/Logger.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <exception>
#include <stdexcept>
#include <string>
#include <utility>

namespace rtype::client
{

render::EyeView toRenderEye(const xr::EyeView &eye)
{
    render::EyeView view;
    view.view.values = eye.view.values;
    view.projection.values = eye.projection.values;
    view.position = {eye.position.x, eye.position.y, eye.position.z};
    view.width = eye.width;
    view.height = eye.height;
    return view;
}

VrView::VrView(render::Renderer &renderer, render::ResourceManager &resources, VrLayout layout)
    : _renderer(renderer), _resources(resources), _layout(layout)
{
}

namespace
{

render::Color grey(float brightness)
{
    const auto channel = static_cast<unsigned char>(std::lround(std::clamp(brightness, 0.0f, 1.0f) * 255.0f));
    return {channel, channel, channel, 255};
}

} // namespace

render::ImageId VrView::board()
{
    if (!_board) {
        _board = _renderer.createImage(BoardResolution);
    }
    return *_board;
}

render::ImageId VrView::panel()
{
    if (!_panel) {
        _panel = _renderer.createImage(PanelResolution);
    }
    return *_panel;
}

render::ImageId VrView::screen(std::size_t index)
{
    std::optional<render::ImageId> &image = _screenImages.at(index);
    if (!image) {
        image = _renderer.createImage(CockpitScreens.at(index).resolution);
    }
    return *image;
}

void VrView::findCockpit()
{
    if (_cockpit || !_loading || _loading->wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
        return;
    }
    render::ModelId model;
    try {
        model = _loading->get();
    } catch (const std::exception &error) {
        // The board floats alone, as before the cockpit: still playable
        log::Logger("vr").warning("cannot load the cockpit, the board floats alone: {}", error.what());
        _loading.reset();
        return;
    }
    Cockpit cockpit{.model = model, .transparent = {}, .allBut = {}, .screens = -1};
    const std::vector<render::ModelMesh> meshes = _resources.getModelMeshes(model);
    cockpit.transparent = farToNear(meshes, CockpitTransparentMeshes, CockpitEyePoint);
    for (const int shown : cockpit.transparent) {
        std::vector<int> hidden;
        for (int index = 0; std::cmp_less(index, meshes.size()); ++index) {
            if (index != shown) {
                hidden.push_back(index);
            }
        }
        cockpit.allBut.push_back(std::move(hidden));
    }
    try {
        cockpit.screens = _resources.getModelAnimation(model, std::string(CockpitScreensAnimation));
    } catch (const std::out_of_range &) {
        cockpit.screens = -1;
    }
    // The screens of the artist show the HUD, in their frame and with their glow
    for (std::size_t index = 0; index < CockpitScreens.size(); ++index) {
        try {
            _renderer.setModelMaterialImage(model, CockpitScreens[index].material, screen(index));
        } catch (const std::out_of_range &error) {
            log::Logger("vr").warning("a screen of the cockpit shows nothing: {}", error.what());
        }
    }
    _cockpit = std::move(cockpit);
}

void VrView::animateCockpit(float elapsed)
{
    if (!_cockpit || !_scene.playing || _cockpit->screens < 0) {
        return;
    }
    _screensTime += elapsed * CockpitScreensSpeed;
    const float duration = _resources.getModelAnimationDuration(_cockpit->model, _cockpit->screens);
    if (duration > 0.0f) {
        _screensTime = std::fmod(_screensTime, duration);
    }
    // In steps: the model keeps its pose, and is not posed again, until the next one
    const float step = std::floor(_screensTime * CockpitScreensRate) / CockpitScreensRate;
    _renderer.animateModel(_cockpit->model, _cockpit->screens, step);
}

void VrView::update(float elapsed, VrScene scene)
{
    if (!_loading) {
        _loading = _resources.loadModelAsync(RTYPE_MODEL_COCKPIT);
    }
    findCockpit();
    if (scene.scenes != _scene.scenes) {
        _fade.start();
    }
    _fade.update(elapsed);
    const float step = elapsed / PanelUnfoldTime;
    _openness = std::clamp(scene.panel ? _openness + step : _openness - step, 0.0f, 1.0f);
    _scene = std::move(scene);
    animateCockpit(elapsed);
}

void VrView::drawFrames(const std::function<void(engine::Surface)> &draw)
{
    // The HUD only during a game: its screens are seen only in the cockpit
    if (_cockpit && _scene.playing) {
        for (std::size_t index = 0; index < CockpitScreens.size(); ++index) {
            const CockpitScreen &shown = CockpitScreens[index];
            _renderer.beginFrame(screen(index), {0.0f, 0.0f, shown.size.x, shown.size.y},
                                 render::colors::Black);
            draw(shown.surface);
            _renderer.endFrame();
        }
    }
    if (_fade.holding()) {
        return;
    }
    if (_openness > 0.0f) {
        const render::Vector2 gameSize = _renderer.gameSize();
        const render::Rectangle area{0.0f, 0.0f, gameSize.x, gameSize.y};
        _renderer.beginFrame(panel(), area, _scene.playing ? PanelBackground : PanelGlass);
        draw(engine::Surface::Panel);
        _renderer.drawRectangleOutline(area, 4.0f, PanelEdge);
        _renderer.endFrame();
    }
    _renderer.beginFrame(board());
    draw(engine::Surface::Board);
    _renderer.endFrame();
}

engine::Surface VrView::toastSurface() const
{
    return _openness > 0.5f ? engine::Surface::Panel : engine::Surface::Board;
}

std::optional<PanelSurface> VrView::panelSurface() const
{
    // Open enough to be read: the pointer would not know where to go on a panel that still moves
    constexpr float Open = 0.99f;
    if (!_panel || _openness < Open || _fade.holding()) {
        return std::nullopt;
    }
    const render::Vector2 gameSize = _renderer.gameSize();
    return PanelSurface{.transform = panelTransform(_layout, gameSize, _openness),
                        .bend = panelLook(_layout, _openness, 1.0f).bend,
                        .gameSize = gameSize};
}

void VrView::drawHands(std::span<const HandLook> hands)
{
    for (const HandLook &hand : hands) {
        if (!hand.shown) {
            continue;
        }
        _renderer.drawBox(hand.controller, ControllerBody, ControllerEdge);
        if (!hand.ray) {
            continue;
        }
        const render::Color light = hand.onWidget ? RayOnWidget : hand.pointing ? RayPointing : RayIdle;
        _renderer.drawBeam(hand.from, hand.to, hand.onWidget ? RayWidth * 1.5f : RayWidth, light);
        if (hand.dot && hand.pointing) {
            _renderer.drawDot(*hand.dot, hand.onWidget ? DotRadius * 1.4f : DotRadius, light);
        }
    }
}

void VrView::drawEye(int eye, const xr::EyeView &view, std::span<const HandLook> hands)
{
    _renderer.beginEye(eye, toRenderEye(view));
    // Space first, at infinity: everything else is in front of it
    const float brightness = _fade.brightness();
    if (_scene.sky) {
        const render::Color tint = _scene.sky->tint;
        const render::Color fade = grey(brightness);
        _renderer.drawSky(_scene.sky->texture, {static_cast<unsigned char>(tint.r * fade.r / 255),
                                                static_cast<unsigned char>(tint.g * fade.g / 255),
                                                static_cast<unsigned char>(tint.b * fade.b / 255), tint.a});
    }
    if (_board) {
        _renderer.drawImageQuad(*_board, boardTransform(_layout, _renderer.gameSize()),
                                {.tint = grey(boardBrightness(_openness, brightness, _scene.playing))});
    }
    // In the menus, the board floats alone in the sky: the player sits in the cockpit for a game
    if (_cockpit && _scene.playing) {
        const CockpitPlacement placement = placeCockpit(CockpitEyePoint);
        // The opaque meshes hide what they cover, the board included; the emissive ones glow with the light
        // they emit, whatever the light of the scene
        _renderer.drawModel(_cockpit->model, _scene.light, placement.position, placement.rotationAxis,
                            placement.rotationAngle, 1.0f, render::colors::White, 0.0f,
                            _cockpit->transparent);
        // Then the transparent ones, from the farthest: the hologram, then the canopy
        for (const std::vector<int> &hidden : _cockpit->allBut) {
            _renderer.drawTransparentModel(_cockpit->model, _scene.light, placement.position,
                                           placement.rotationAxis, placement.rotationAngle, 1.0f, hidden);
        }
    }
    // Last, over the cockpit: nothing hides the menus
    if (_panel && _openness > 0.0f) {
        _renderer.drawImageQuad(*_panel, panelTransform(_layout, _renderer.gameSize(), _openness),
                                panelLook(_layout, _openness, brightness));
    }
    // The hands of the player, over the panel they point at
    drawHands(hands);
    _renderer.endEye();
}

} // namespace rtype::client
