#include "Vr/VrView.hpp"

#include "Assets.hpp"
#include "Vr/CockpitLook.hpp"
#include "rtype/log/Logger.hpp"

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

render::ImageId VrView::board()
{
    if (!_board) {
        _board = _renderer.createImage(BoardResolution);
    }
    return *_board;
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
    _scene = std::move(scene);
    animateCockpit(elapsed);
}

void VrView::drawEye(int eye, const xr::EyeView &view)
{
    _renderer.beginEye(eye, toRenderEye(view));
    // Space first, at infinity: everything else is in front of it
    if (_scene.sky) {
        _renderer.drawSky(_scene.sky->texture, _scene.sky->tint);
    }
    if (_board) {
        _renderer.drawImageQuad(*_board, boardTransform(_layout, _renderer.gameSize()));
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
    _renderer.endEye();
}

} // namespace rtype::client
