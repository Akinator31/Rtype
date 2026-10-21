#pragma once

#include "rtype/render/RenderContext.hpp"
#include "rtype/render/Renderer.hpp"

#include <deque>
#include <functional>
#include <future>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace rtype::render
{

using RenderCommands = std::vector<std::function<void(Renderer &)>>;

class RenderFrames
{
    std::mutex _mutex;
    std::optional<RenderCommands> _next;

public:
    void publish(RenderCommands commands)
    {
        std::lock_guard lock(_mutex);
        _next = std::move(commands);
    }

    [[nodiscard]] std::optional<RenderCommands> take()
    {
        std::lock_guard lock(_mutex);
        return std::exchange(_next, std::nullopt);
    }
};

class RenderQueries
{
    std::mutex _mutex;
    std::deque<std::function<void()>> _pending;
    bool _stopped = false;

public:
    template <typename Function>
    auto run(Function function) -> std::invoke_result_t<Function>
    {
        using Result = std::invoke_result_t<Function>;
        auto task = std::make_shared<std::packaged_task<Result()>>(std::move(function));
        auto future = task->get_future();
        {
            std::lock_guard lock(_mutex);
            if (_stopped) {
                throw std::runtime_error("Render queries stopped");
            }
            _pending.emplace_back([task] { (*task)(); });
        }
        return future.get();
    }

    void drain()
    {
        std::deque<std::function<void()>> pending;
        {
            std::lock_guard lock(_mutex);
            pending.swap(_pending);
        }
        for (auto &task : pending) {
            task();
        }
    }

    void stop()
    {
        std::lock_guard lock(_mutex);
        _stopped = true;
        _pending.clear();
    }
};

class CommandRecorder : public RenderContext
{
    Renderer &_renderer;
    RenderQueries &_queries;
    RenderCommands _commands;

public:
    CommandRecorder(Renderer &renderer, RenderQueries &queries) : _renderer(renderer), _queries(queries) {}

    [[nodiscard]] RenderCommands take() { return std::exchange(_commands, {}); }

    [[nodiscard]] Vector2 gameSize() const override
    {
        return _queries.run([this] { return _renderer.gameSize(); });
    }
    [[nodiscard]] Rectangle visibleArea() const override
    {
        return _queries.run([this] { return _renderer.visibleArea(); });
    }
    [[nodiscard]] Vector2 windowToGame(Vector2 position) const override
    {
        return _queries.run([this, position] { return _renderer.windowToGame(position); });
    }
    [[nodiscard]] Vector2 textureSize(TextureId texture) const override
    {
        return _queries.run([this, texture] { return _renderer.textureSize(texture); });
    }
    [[nodiscard]] Vector2 measureText(FontId font, const std::string &text, float size) const override
    {
        return _queries.run([this, font, text, size] { return _renderer.measureText(font, text, size); });
    }

    void drawTexture(TextureId texture, Vector2 position, Color tint) override
    {
        _commands.emplace_back([=](Renderer &renderer) { renderer.drawTexture(texture, position, tint); });
    }
    void drawTexture(TextureId texture, Rectangle source, Rectangle destination, float rotation,
                     Color tint) override
    {
        _commands.emplace_back(
            [=](Renderer &renderer) { renderer.drawTexture(texture, source, destination, rotation, tint); });
    }
    void drawRectangle(Rectangle rectangle, Color color) override
    {
        _commands.emplace_back([=](Renderer &renderer) { renderer.drawRectangle(rectangle, color); });
    }
    void drawCircle(Vector2 center, float radius, Color color) override
    {
        _commands.emplace_back([=](Renderer &renderer) { renderer.drawCircle(center, radius, color); });
    }
    void drawRectangleOutline(Rectangle rectangle, float thickness, Color color) override
    {
        _commands.emplace_back(
            [=](Renderer &renderer) { renderer.drawRectangleOutline(rectangle, thickness, color); });
    }
    void drawRoundedRectangle(Rectangle rectangle, float radius, Color left, Color right) override
    {
        _commands.emplace_back(
            [=](Renderer &renderer) { renderer.drawRoundedRectangle(rectangle, radius, left, right); });
    }
    void drawRoundedRectangleOutline(Rectangle rectangle, float radius, float thickness, Color color) override
    {
        _commands.emplace_back([=](Renderer &renderer) {
            renderer.drawRoundedRectangleOutline(rectangle, radius, thickness, color);
        });
    }
    void drawCircleOutline(Vector2 center, float radius, float thickness, Color color) override
    {
        _commands.emplace_back(
            [=](Renderer &renderer) { renderer.drawCircleOutline(center, radius, thickness, color); });
    }
    void drawLine(Vector2 start, Vector2 end, float thickness, Color color) override
    {
        _commands.emplace_back([=](Renderer &renderer) { renderer.drawLine(start, end, thickness, color); });
    }
    void drawTriangle(Vector2 first, Vector2 second, Vector2 third, Color color) override
    {
        _commands.emplace_back(
            [=](Renderer &renderer) { renderer.drawTriangle(first, second, third, color); });
    }
    void drawModel(ModelId model, const Camera &camera, const Light &light, Vector3 position,
                   Vector3 rotationAxis, float rotationAngle, float scale, Color tint,
                   float spinAngle) override
    {
        _commands.emplace_back([=](Renderer &renderer) {
            renderer.drawModel(model, camera, light, position, rotationAxis, rotationAngle, scale, tint,
                               spinAngle);
        });
    }
    void animateModel(ModelId model, int animation, float time) override
    {
        _commands.emplace_back([=](Renderer &renderer) { renderer.animateModel(model, animation, time); });
    }
    void blendModelAnimations(ModelId model, int from, float fromTime, int to, float toTime,
                              float amount) override
    {
        _commands.emplace_back([=](Renderer &renderer) {
            renderer.blendModelAnimations(model, from, fromTime, to, toTime, amount);
        });
    }
    void drawText(FontId font, const std::string &text, Vector2 position, float size, Color color) override
    {
        _commands.emplace_back(
            [=](Renderer &renderer) { renderer.drawText(font, text, position, size, color); });
    }
};

} // namespace rtype::render
