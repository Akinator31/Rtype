#include "ShipSelector/ShipSelectorSystem.hpp"

#include "Game/GameView.hpp"
#include "MainMenu/Space.hpp"
#include "rtype/game/Rules.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <numbers>
#include <utility>

namespace rtype::client
{

namespace
{

/// Degrees the ship turns for each pixel the pointer moves.
constexpr float turnPerPixel = 0.4f;

/// How fast the carousel glides: the part of the way left covered per second, so it slows down as it arrives.
constexpr float glideSpeed = 9.0f;

/// Distance between two ships, in world units, and how the ships that are not in the middle look: their size
/// and their brightness, as a part of those of the ship in the middle.
constexpr float shipSpacing = 3.6f;
constexpr float sideScale = 0.62f;
constexpr float sideBrightness = 0.28f;

/// A locked ship is dark: it can be looked at, not flown.
constexpr render::Color lockedTint{110, 112, 125, 255};

/// The dot of the ship looked at is a long bright line, the others short and dim.
constexpr float dotWidth = 22.0f;
constexpr float litDotWidth = 54.0f;
constexpr render::Color dotColor{255, 255, 255, 70};
constexpr render::Color litDotColor{255, 255, 255, 230};

/// The take-off, in seconds from the click: the interface fades out while the ships on the sides slide away
/// and the pedestal sinks; the ship turns to its profile and rises a little, draws back with its nose up,
/// then speeds off to the right and levels off, a little turned towards the camera as in the game. The
/// camera follows it, then pulls far back while it narrows, until it sees it as the game does, without
/// perspective.
constexpr float interfaceFade = 0.45f;
constexpr float sidesEnd = 0.6f;
constexpr float sinkStart = 0.15f;
constexpr float sinkEnd = 1.1f;
constexpr float turnEnd = 0.8f;
constexpr float recoilEnd = 1.05f;
constexpr float cruiseStart = 1.9f;
constexpr float levelStart = 1.4f;
constexpr float levelEnd = 2.3f;
constexpr float followEnd = 2.4f;
constexpr float dollyStart = 2.4f;
constexpr float dollyEnd = 3.4f;
/// Once the game has loaded, and the ship has launched, its sky shows behind the stars of the hangar, which
/// fade out in this time, while the light becomes the one of the game.
constexpr float skyStart = 1.3f;
constexpr float skyFade = 0.9f;
/// The game should have opened this long after it was launched: the hangar closes.
constexpr float lostAfter = 20.0f;

/// How far the ships on the sides go, in spacings, the pedestal sinks, the ship rises and draws back, in
/// world units, and how much its nose goes up, in degrees; then how fast it flies, in world units per second.
constexpr float sidesSpread = 2.0f;
constexpr float pedestalSink = 4.5f;
constexpr float liftHeight = 0.3f;
constexpr float recoilLength = 0.35f;
constexpr float climbAngle = 10.0f;
constexpr float cruiseSpeed = 7.0f;
/// The flame grows from the small to the big one in this time, in seconds, and back.
constexpr float flameFade = 0.4f;

/// The ship seen from the side, its nose to the right (+x). The ship models point their nose to +x: a
/// quarter turn around their vertical axis points it to +z, as computeRotationTowards() expects.
constexpr float profileYaw = 0.0f;
constexpr float noseToFront = -90.0f;

/// The field of view of the camera as it catches up with the ship, in degrees, and how much further it pulls
/// back, narrowing as much: from there, the ship looks as without perspective.
constexpr float followFieldOfView = 24.0f;
constexpr float dollyFactor = 25.0f;

/// The stars of the hangar are drawn in front of the layers of the sky of the game while they fade out.
constexpr int hangarSkyOrder = 100;

[[nodiscard]] float mix(float from, float to, float amount)
{
    return from + ((to - from) * amount);
}

/// How far `time` is between `start` and `end`: 0 before, 1 after.
[[nodiscard]] float phase(float time, float start, float end)
{
    return std::clamp((time - start) / (end - start), 0.0f, 1.0f);
}

/// Slow at both ends.
[[nodiscard]] float smooth(float amount)
{
    return amount * amount * (3.0f - (2.0f * amount));
}

[[nodiscard]] unsigned char alphaOf(float opacity)
{
    return static_cast<unsigned char>(std::lround(std::clamp(opacity, 0.0f, 1.0f) * 255.0f));
}

[[nodiscard]] render::Color shade(render::Color color, float brightness)
{
    const auto scale = [brightness](unsigned char channel) {
        return static_cast<unsigned char>(std::clamp(static_cast<float>(channel) * brightness, 0.0f, 255.0f));
    };
    return {scale(color.r), scale(color.g), scale(color.b), color.a};
}

[[nodiscard]] render::Color mix(render::Color from, render::Color to, float amount)
{
    const auto channel = [amount](unsigned char first, unsigned char second) {
        return static_cast<unsigned char>(mix(static_cast<float>(first), static_cast<float>(second), amount));
    };
    return {channel(from.r, to.r), channel(from.g, to.g), channel(from.b, to.b), channel(from.a, to.a)};
}

[[nodiscard]] render::Vector3 mix(render::Vector3 from, render::Vector3 to, float amount)
{
    return {mix(from.x, to.x, amount), mix(from.y, to.y, amount), mix(from.z, to.z, amount)};
}

[[nodiscard]] render::Light mix(const render::Light &from, const render::Light &to, float amount)
{
    return {mix(from.direction, to.direction, amount), mix(from.color, to.color, amount),
            mix(from.ambient, to.ambient, amount), mix(from.rimDirection, to.rimDirection, amount),
            mix(from.rim, to.rim, amount)};
}

/// The world of the game seen in the hangar: its y points down, and its camera looks the other way.
[[nodiscard]] render::Vector3 fromGame(render::Vector3 direction)
{
    return {direction.x, -direction.y, -direction.z};
}

/// The light of the game, in the world of the hangar.
[[nodiscard]] render::Light getGameLight()
{
    render::Light light = GameLight;
    light.direction = fromGame(light.direction);
    light.rimDirection = fromGame(light.rimDirection);
    return light;
}

/// Where the nose of the ship points, turned around its vertical axis by `yaw`, its nose up by `pitch`, in
/// degrees.
[[nodiscard]] render::Vector3 getNose(float yaw, float pitch)
{
    const float y = toRadians(yaw);
    const float p = toRadians(pitch);
    return {std::cos(p) * std::cos(y), std::sin(p), -std::cos(p) * std::sin(y)};
}

/// Where the nose and the top of the ship point: turned around its vertical axis by `yaw`, its nose up by
/// `pitch`, rolled around its length by `roll`, its top towards the camera, in degrees.
[[nodiscard]] Rotation orient(float yaw, float pitch, float roll)
{
    const float y = toRadians(yaw);
    const float p = toRadians(pitch);
    const render::Vector3 nose = getNose(yaw, pitch);
    const render::Vector3 top{-std::sin(p) * std::cos(y), std::cos(p), std::sin(p) * std::sin(y)};
    return computeRotationTowards(nose, rotate(top, {nose, roll}));
}

} // namespace

ShipSelectorSystem::ShipSelectorSystem(ecs::ComponentCaches &caches, const engine::Scenes &scenes,
                                       const engine::Controls &controls, const input::InputSource &input,
                                       const engine::Metrics &metrics)
    : _scenes(scenes), _controls(controls), _input(input), _metrics(metrics),
      _carousels(use<ShipCarousel>(caches)), _models(use<engine::Model>(caches)),
      _animations(use<engine::ModelAnimation>(caches)), _cameras(use<engine::Camera>(caches)),
      _lights(use<engine::Light>(caches)), _backdrops(use<engine::Backdrop>(caches)),
      _boxes(use<engine::Box>(caches)), _opacities(use<engine::Opacity>(caches)),
      _focused(use<engine::Focused>(caches)), _rows(use<engine::SettingRow>(caches))
{
}

bool ShipSelectorSystem::isRowFocused() const
{
    for (ecs::entity_t entity = 0; entity < _focused.size(); ++entity) {
        if (_focused.has(entity) && _rows.has(entity) && _scenes.receivesInput(entity)) {
            return true;
        }
    }
    return false;
}

void ShipSelectorSystem::readControls(ecs::entity_t entity, const ShipCarousel &carousel)
{
    if (!_scenes.receivesInput(entity) || isRowFocused()) {
        return;
    }
    if (_controls.wasPressed(engine::Control::Left)) {
        carousel.selection->step(-1);
    }
    if (_controls.wasPressed(engine::Control::Right)) {
        carousel.selection->step(1);
    }
}

void ShipSelectorSystem::turnShip(ecs::entity_t entity, ShipCarousel &carousel)
{
    const float pointerX = _input.mousePosition().x;
    const bool held = _scenes.receivesInput(entity) && !carousel.selection->departing &&
                      _input.isMouseButtonDown(input::MouseButton::Left);
    if (held && carousel.dragging) {
        carousel.yaws[carousel.selection->focused] += (pointerX - carousel.lastPointerX) * turnPerPixel;
    }
    carousel.dragging = held;
    carousel.lastPointerX = pointerX;
}

void ShipSelectorSystem::glide(ShipSelection &selection, float elapsed)
{
    const auto count = static_cast<float>(shipCount);
    // The shortest way to the ship looked at, which may cross the end of the catalog
    float way = static_cast<float>(selection.focused) - selection.slide;
    way -= count * std::round(way / count);
    selection.slide += way * std::min(1.0f, glideSpeed * elapsed);
    selection.slide -= count * std::floor(selection.slide / count);
}

void ShipSelectorSystem::placeShips(const ShipCarousel &carousel)
{
    const auto &catalog = getShipCatalog();
    const float slide = carousel.selection->slide;
    for (std::size_t index = 0; index < carousel.ships.size(); ++index) {
        engine::Model &model = _models.get(carousel.ships[index]);
        const float weight = getShipWeight(index, slide);
        model.position.x = getShipOffset(index, slide) * shipSpacing;
        model.scale = catalog[index].modelScale * mix(sideScale, 1.0f, weight);
        model.spinAngle = carousel.yaws[index];
        const render::Color base = carousel.selection->isLocked(index) ? lockedTint : render::colors::White;
        model.tint = shade(base, mix(sideBrightness, 1.0f, weight));
    }
}

void ShipSelectorSystem::lightDots(const ShipCarousel &carousel)
{
    for (std::size_t index = 0; index < carousel.dots.size(); ++index) {
        const ecs::entity_t dot = carousel.dots[index];
        if (!_boxes.has(dot)) {
            continue;
        }
        const float weight = getShipWeight(index, carousel.selection->slide);
        engine::Box &box = _boxes.get(dot);
        box.width = mix(dotWidth, litDotWidth, weight);
        box.color = mix(dotColor, litDotColor, weight);
    }
}

void ShipSelectorSystem::fadeTexts(const ShipCarousel &carousel)
{
    const float opacity = carousel.selection->getShownClarity() * getInterfaceOpacity(carousel);
    for (const ecs::entity_t text : carousel.fading) {
        if (_opacities.has(text)) {
            _opacities.get(text).value = opacity;
        }
    }
}

float ShipSelectorSystem::getInterfaceOpacity(const ShipCarousel &carousel)
{
    return carousel.departure ? 1.0f - phase(*carousel.departure, 0.0f, interfaceFade) : 1.0f;
}

void ShipSelectorSystem::depart(ShipCarousel &carousel, float elapsed)
{
    const std::size_t picked = carousel.selection->focused;
    const ecs::entity_t ship = carousel.ships[picked];
    if (!carousel.departure) {
        carousel.departure = 0.0f;
        carousel.launchPad = _models.get(ship).position;
        carousel.flightPosition = carousel.launchPad;
        carousel.departureYaw = carousel.yaws[picked];
        if (_models.has(carousel.pedestal)) {
            carousel.pedestalHeight = _models.get(carousel.pedestal).position.y;
        }
        if (_cameras.has(carousel.camera)) {
            carousel.hangarView = _cameras.get(carousel.camera).view;
        }
        if (_lights.has(carousel.camera)) {
            carousel.hangarLight = _lights.get(carousel.camera).light;
        }
        if (_animations.has(ship)) {
            engine::switchModelAnimation(_animations.get(ship), carousel.animations[picked].move, flameFade);
        }
    } else {
        *carousel.departure += elapsed;
    }
    const float time = *carousel.departure;

    const float interface = getInterfaceOpacity(carousel);
    for (const ecs::entity_t entity : carousel.interface) {
        _opacities.emplace(entity, interface);
    }
    clearPad(carousel, time);
    flyShip(carousel, time, elapsed);
    followShip(carousel, time);
    openSky(carousel, elapsed);
    launch(carousel, time, elapsed);
}

void ShipSelectorSystem::clearPad(const ShipCarousel &carousel, float time)
{
    // They shrink away, so that the camera does not meet them again as it follows the ship
    const float away = smooth(phase(time, 0.0f, sidesEnd));
    for (std::size_t index = 0; index < carousel.ships.size(); ++index) {
        if (index == carousel.selection->focused) {
            continue;
        }
        engine::Model &model = _models.get(carousel.ships[index]);
        model.position.x *= 1.0f + (sidesSpread * away);
        model.scale *= 1.0f - away;
        model.tint = shade(model.tint, 1.0f - away);
        model.tint.a = alphaOf(1.0f - away);
    }
    if (_models.has(carousel.pedestal)) {
        // Slowly at first, then faster: it drops out of the screen
        const float sink = phase(time, sinkStart, sinkEnd);
        engine::Model &pedestal = _models.get(carousel.pedestal);
        pedestal.position.y = carousel.pedestalHeight - (pedestalSink * sink * sink);
        pedestal.tint.a = alphaOf(1.0f - sink);
        if (sink >= 1.0f) {
            pedestal.scale = 0.0f;
        }
    }
}

void ShipSelectorSystem::flyShip(ShipCarousel &carousel, float time, float elapsed)
{
    const std::size_t picked = carousel.selection->focused;
    const ecs::entity_t ship = carousel.ships[picked];
    engine::Model &model = _models.get(ship);
    // The shortest way to its profile, from wherever the player left it; its nose goes up as it draws back,
    // then down as it levels off, rolling towards the camera as the game shows the ships
    const float turn = smooth(phase(time, 0.0f, turnEnd));
    const float profile = carousel.departureYaw + std::remainder(profileYaw - carousel.departureYaw, 360.0f);
    const float recoil = smooth(phase(time, turnEnd, recoilEnd));
    const float level = smooth(phase(time, levelStart, levelEnd));
    const float pitch = climbAngle * (recoil - level);
    const float yaw = mix(carousel.departureYaw, profile, turn);
    const Rotation rotation = orient(yaw, pitch, ShipTilt * level);
    model.spinAngle = noseToFront;
    model.rotationAxis = rotation.axis;
    model.rotationAngle = rotation.angle;

    if (time < recoilEnd) {
        carousel.flightPosition = {carousel.launchPad.x - (recoilLength * recoil),
                                   carousel.launchPad.y + (liftHeight * turn), carousel.launchPad.z};
    } else {
        // Along its nose, faster and faster up to its cruise
        const float speed = cruiseSpeed * smooth(phase(time, recoilEnd, cruiseStart));
        carousel.flightPosition = carousel.flightPosition + (getNose(yaw, pitch) * (speed * elapsed));
    }
    model.position = carousel.flightPosition;

    // As it levels off: the little flame of the game, its trails hidden as the game hides them
    if (time >= levelStart && time - elapsed < levelStart) {
        model.hiddenMeshes = getShipCatalog()[picked].trailMeshes;
        if (_animations.has(ship)) {
            engine::ModelAnimation &animation = _animations.get(ship);
            engine::switchModelAnimation(animation, carousel.animations[picked].idle, flameFade);
            animation.blendedAnimation = carousel.animations[picked].move;
            animation.blendAmount = ShipFlame;
        }
    }
}

render::Camera ShipSelectorSystem::computeArrivalView(const ShipCarousel &carousel, float time) const
{
    const std::size_t picked = carousel.selection->focused;
    const ShipEntry &entry = getShipCatalog()[picked];
    const engine::Model &model = _models.get(carousel.ships[picked]);
    // The height the game will see around the ship (ArrivalZoom), at the scale of the hangar: the ship is as
    // long on the screen as in the game
    const render::Rectangle area = _metrics.visibleArea();
    const float gameLength = game::Rules{}.shipHalfWidth * 2.0f;
    const float height = entry.hull.width * model.scale * area.height * ArrivalZoom / gameLength;
    const float width = height * area.width / area.height;
    // The centre of its hull at the same place of the screen as in the game, where y points down
    const render::Vector2 centre{entry.hull.x + (entry.hull.width / 2.0f),
                                 entry.hull.y + (entry.hull.height / 2.0f)};
    const render::Vector3 hull =
        model.position + render::Vector3{centre.x * model.scale, -centre.y * model.scale, 0.0f};
    const render::Vector3 target =
        hull - render::Vector3{(ArrivalScreen.x - 0.5f) * width, (0.5f - ArrivalScreen.y) * height, 0.0f};

    const float near = height / (2.0f * std::tan(toRadians(followFieldOfView) / 2.0f));
    const float distance = near * mix(1.0f, dollyFactor, smooth(phase(time, dollyStart, dollyEnd)));
    render::Camera view;
    view.position = target + render::Vector3{0.0f, 0.0f, distance};
    view.target = target;
    if (time >= dollyEnd) {
        view.fieldOfView = height;
        view.projection = render::Projection::Orthographic;
    } else {
        view.fieldOfView = toDegrees(2.0f * std::atan(height / (2.0f * distance)));
    }
    return view;
}

void ShipSelectorSystem::followShip(const ShipCarousel &carousel, float time)
{
    const float follow = smooth(phase(time, recoilEnd, followEnd));
    if (!_cameras.has(carousel.camera) || follow <= 0.0f) {
        return;
    }
    const render::Camera arrival = computeArrivalView(carousel, time);
    render::Camera &view = _cameras.get(carousel.camera).view;
    if (follow >= 1.0f) {
        view = arrival;
        return;
    }
    const render::Camera &hangar = carousel.hangarView;
    view.position = mix(hangar.position, arrival.position, follow);
    view.target = mix(hangar.target, arrival.target, follow);
    view.fieldOfView = mix(hangar.fieldOfView, arrival.fieldOfView, follow);
}

void ShipSelectorSystem::openSky(ShipCarousel &carousel, float elapsed)
{
    if (!carousel.skyAge) {
        const bool ready = !carousel.isArrivalReady || carousel.isArrivalReady();
        if (*carousel.departure < skyStart || !ready) {
            return;
        }
        if (carousel.openGameSky) {
            carousel.openGameSky();
        }
        // The stars of the hangar in front of the sky of the game, until they have faded out
        if (_backdrops.has(carousel.sky)) {
            _backdrops.get(carousel.sky).order = hangarSkyOrder;
        }
        carousel.skyAge = 0.0f;
    } else {
        *carousel.skyAge += elapsed;
    }
    const float age = *carousel.skyAge;
    carousel.selection->arrivalSky = age;
    if (_backdrops.has(carousel.sky)) {
        _backdrops.get(carousel.sky).tint.a = alphaOf(1.0f - phase(age, 0.0f, skyFade));
    }
    if (_lights.has(carousel.camera)) {
        _lights.get(carousel.camera).light =
            mix(carousel.hangarLight, getGameLight(), smooth(phase(age, 0.0f, skyFade)));
    }
}

void ShipSelectorSystem::launch(ShipCarousel &carousel, float time, float elapsed)
{
    if (carousel.launched) {
        *carousel.launched += elapsed;
        if (*carousel.launched >= lostAfter && carousel.onLost) {
            std::exchange(carousel.onLost, nullptr)();
        }
        return;
    }
    // The ship flies in the sky of the game, seen as the game will see it: the game can open on it
    if (time < dollyEnd || !carousel.skyAge || *carousel.skyAge < skyFade) {
        return;
    }
    carousel.launched = 0.0f;
    if (carousel.onDeparted) {
        std::exchange(carousel.onDeparted, nullptr)();
    }
}

void ShipSelectorSystem::update(float elapsed)
{
    for (ecs::entity_t entity = 0; entity < _carousels.size(); ++entity) {
        if (!_carousels.has(entity)) {
            continue;
        }
        ShipCarousel &carousel = _carousels.get(entity);
        readControls(entity, carousel);
        turnShip(entity, carousel);
        glide(*carousel.selection, elapsed);
        placeShips(carousel);
        lightDots(carousel);
        if (carousel.selection->departing) {
            depart(carousel, elapsed);
        }
        fadeTexts(carousel);
    }
}

} // namespace rtype::client
