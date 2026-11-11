#include "Game/HudSystem.hpp"

#include "Game/Radar.hpp"

#include <algorithm>
#include <format>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace rtype::client
{

namespace
{

// The places are those of the Figma design, 1440 wide, scaled to the 1920 of the game: by 4/3. Each part is
// placed by its centre, from the bottom of the screen

constexpr float Margin = 53.3f;

constexpr float LifeGap = 10.7f;
constexpr float LivesCentre = 146.7f;
constexpr float PilotCentre = 114.0f;
constexpr float ScoreCentre = 75.3f;
constexpr float HealthCentre = 38.0f;
/// Between the score and the experience, and between the hit points and the weapons.
constexpr float XpGap = 21.3f;
/// Between the weapon in hand and the one put away.
constexpr float WeaponGap = 16.0f;

/// The line of the other player nearest to the ping, and the distance between two lines.
constexpr float AllyCentre = 92.7f;
constexpr float AllyStep = 21.3f;
/// Between the parts of the line of another player: its icon, its slot, its name and its score.
constexpr float AllyGap = 13.3f;
/// Between the slot of the local player and the icon of its voice.
constexpr float PilotVoiceGap = 10.7f;

/// The name of another pilot, lit while it talks.
constexpr render::Color AllyName{138, 144, 151, 255};
constexpr render::Color SpeakingName = render::colors::White;

constexpr float PingCentre = 56.7f;
/// Between the signal bars and the ping, and between two bars.
constexpr float PingGap = 13.3f;
constexpr float BarGap = 4.0f;
constexpr float SignalBottom = 48.0f;

constexpr render::Color BarLit = render::colors::White;
constexpr render::Color BarOff{58, 62, 67, 255};

// The screens of the cockpit (Figma, Série 17), each part placed by its centre from the top of the part of
// its screen the pilot sees (CockpitScreens): about square for the ship, taller than wide for the navigation

namespace ship
{
/// Everything from the left: the flight stick hides the bottom right of the screen.
constexpr float Margin = 24.0f;
constexpr float PilotCentre = 36.0f;
constexpr float VoiceGap = 12.0f;
constexpr float LivesCentre = 84.0f;
constexpr float LifeGap = 14.0f;
constexpr float ScoreCentre = 148.0f;
constexpr float XpCentre = 198.0f;
constexpr float HealthCentre = 286.0f;
} // namespace ship

namespace nav
{
constexpr float Margin = 16.0f;
constexpr float LabelCentre = 40.0f;
constexpr HudText Label{22.0f, 5.0f};
/// "ENDLESS" and "SANS FIN" fit in the width of the screen at this size.
constexpr float WaveCentre = 92.0f;
constexpr HudText Wave{42.0f, 4.0f};
constexpr float BossCentre = 150.0f;
constexpr HudText Boss{16.0f, 2.0f};
/// The kind of game on a line, the ping and its bars on the one under it.
constexpr float ModeCentre = 214.0f;
constexpr HudText Mode{22.0f, 3.0f};
constexpr float LightSize = 10.0f;
constexpr float ModeLeft = 34.0f;
constexpr float PingCentre = 262.0f;
/// Between the bars and the ping, and between two bars.
constexpr float PingGap = 16.0f;
constexpr float BarGap = 6.0f;
constexpr float SignalBottom = 278.0f;
constexpr render::Color Online{111, 227, 240, 255};
constexpr render::Color Solo{107, 113, 119, 255};
} // namespace nav

namespace aux
{
constexpr float Margin = 28.0f;
constexpr float TitleCentre = 38.0f;
constexpr HudText Title{20.0f, 5.0f};
constexpr float FirstRow = 100.0f;
constexpr float RowStep = 64.0f;
constexpr float SlotLeft = 60.0f;
constexpr float NameLeft = 100.0f;
} // namespace aux

namespace holo
{
constexpr float TitleCentre = 30.0f;
constexpr HudText Title{18.0f, 5.0f};
constexpr float BossOutline = 3.0f;
} // namespace holo

/// The title of the radar, the same in every language.
constexpr std::string_view RadarTitle = "RADAR";

} // namespace

std::string formatScore(std::uint32_t score)
{
    return std::format("{:07}", score);
}

std::string formatXp(const game::PlayerState &player)
{
    if (player.xp == 0 && player.streak < 2) {
        return {};
    }
    if (player.streak < 2) {
        return std::format("+{} XP", player.xp);
    }
    return std::format("+{} XP  x{}", player.xp, player.streak);
}

std::string formatWave(std::uint16_t wave, std::optional<std::uint16_t> count)
{
    if (count) {
        return std::format("{:02} / {:02}", wave, *count);
    }
    return std::format("{:02}", wave);
}

std::size_t countSignalBars(std::chrono::milliseconds ping)
{
    if (ping < HudSystem::FourBars) {
        return 4;
    }
    if (ping < HudSystem::ThreeBars) {
        return 3;
    }
    return ping < HudSystem::TwoBars ? 2 : 1;
}

HudSystem::HudSystem(ecs::ComponentCaches &caches, const GameSessionSystem &sessions,
                     const engine::Metrics &metrics)
    : _sessions(sessions), _metrics(metrics), _huds(use<Hud>(caches)), _anchors(use<engine::Anchor>(caches)),
      _texts(use<engine::Text>(caches)), _sprites(use<engine::Sprite>(caches)),
      _boxes(use<engine::Box>(caches)), _opacities(use<engine::Opacity>(caches)),
      _replicated(use<Replicated>(caches)), _positions(use<ecs::Position>(caches))
{
}

void HudSystem::update(float /*elapsed*/)
{
    const GameSession *session = _sessions.session();
    const std::uint8_t local = session != nullptr ? session->localSlot() : 0;
    const std::optional<std::chrono::milliseconds> ping = session != nullptr ? session->ping() : std::nullopt;
    const std::map<std::uint8_t, std::string> names =
        session != nullptr ? session->playerNames() : std::map<std::uint8_t, std::string>{};
    const std::optional<VoiceView> voice = session != nullptr ? session->voiceView() : std::nullopt;
    for (ecs::entity_t entity = 0; entity < _huds.size(); ++entity) {
        if (!_huds.has(entity)) {
            continue;
        }
        const Hud &hud = _huds.get(entity);
        const bool cockpit = _metrics.surfacesApart();
        applyScale(hud, cockpit ? CockpitHud : ScreenHud);
        showShadows(hud, cockpit);
        showPilot(hud, local, names, voice);
        showHealth(hud);
        showWeapons(hud, cockpit);
        showAllies(hud, local, names, voice);
        showPing(hud, ping);
        showCockpitOnly(hud, cockpit);
        showRadar(hud, cockpit, local);
        if (cockpit) {
            placeInCockpit(hud);
        }
    }
}

void HudSystem::applyScale(const Hud &hud, const HudScale &scale)
{
    const auto size = [this](ecs::entity_t entity, HudText look, engine::AnchorPoint point) {
        engine::Text &text = _texts.get(entity);
        text.size = look.size;
        text.spacing = look.spacing;
        _anchors.get(entity).point = point;
    };
    constexpr engine::AnchorPoint left = engine::AnchorPoint::BottomLeft;
    constexpr engine::AnchorPoint right = engine::AnchorPoint::BottomRight;
    size(hud.pilot, scale.pilot, left);
    size(hud.score, scale.score, left);
    size(hud.health, scale.health, left);
    size(hud.weapon, scale.health, left);
    size(hud.spareWeapon, scale.health, left);
    size(hud.xp, scale.xp, left);
    size(hud.ping, scale.ping, right);
    for (const HudAlly &ally : hud.allies) {
        size(ally.slot, scale.slot, right);
        size(ally.name, scale.ally, right);
        size(ally.score, scale.ally, right);
        engine::Sprite &icon = _sprites.get(ally.voice);
        icon.width = scale.allyVoice;
        icon.height = scale.allyVoice;
        _anchors.get(ally.voice).point = right;
    }
    for (const ecs::entity_t life : hud.lives) {
        engine::Sprite &sprite = _sprites.get(life);
        sprite.width = scale.life.x;
        sprite.height = scale.life.y;
        _anchors.get(life).point = left;
    }
    engine::Sprite &pilotVoice = _sprites.get(hud.pilotVoice);
    pilotVoice.width = scale.pilotVoice;
    pilotVoice.height = scale.pilotVoice;
    _anchors.get(hud.pilotVoice).point = left;
    for (std::size_t index = 0; index < hud.signal.size(); ++index) {
        engine::Box &bar = _boxes.get(hud.signal[index]);
        bar.width = scale.barWidth;
        bar.height = scale.bars.at(index);
        _anchors.get(hud.signal[index]).point = right;
    }
}

void HudSystem::showShadows(const Hud &hud, bool cockpit)
{
    const float width = _metrics.visibleArea().width;
    for (const ecs::entity_t shadow : {hud.topShadow, hud.bottomShadow}) {
        _boxes.get(shadow).width = width;
        _opacities.get(shadow).value = cockpit ? 0.0f : 1.0f;
    }
}

bool HudSystem::bossIsThere() const
{
    for (ecs::entity_t entity = 0; entity < _replicated.size(); ++entity) {
        if (_replicated.has(entity) && _replicated.get(entity).kind == game::EntityKind::BydoBoss) {
            return true;
        }
    }
    return false;
}

void HudSystem::showCockpitOnly(const Hud &hud, bool cockpit)
{
    const auto write = [this](ecs::entity_t entity, std::string text, HudText look) {
        engine::Text &shown = _texts.get(entity);
        shown.text = std::move(text);
        shown.size = look.size;
        shown.spacing = look.spacing;
    };
    engine::Box &light = _boxes.get(hud.modeLight);
    if (!cockpit) {
        for (const ecs::entity_t entity : {hud.waveLabel, hud.wave, hud.boss, hud.mode, hud.pilots}) {
            _texts.get(entity).text.clear();
        }
        light.width = 0.0f;
        light.height = 0.0f;
        return;
    }
    const HudWords &words = hud.look.words;
    const GameSession *session = _sessions.session();
    const std::optional<std::uint16_t> count = session != nullptr ? session->waveCount() : std::nullopt;
    const bool endless = session != nullptr && session->isEndless();
    const bool online = session != nullptr && session->isOnline();
    write(hud.waveLabel, endless ? std::format("{} · {}", words.wave, words.endless) : words.wave,
          nav::Label);
    write(hud.wave, formatWave(_sessions.wave(), endless ? std::nullopt : count), nav::Wave);
    write(hud.boss, bossIsThere() ? words.boss : std::string(), nav::Boss);
    write(hud.mode, online ? words.online : words.solo, nav::Mode);
    light.width = nav::LightSize;
    light.height = nav::LightSize;
    light.color = online ? nav::Online : nav::Solo;
    const bool alone = std::ranges::all_of(
        hud.allies, [this](const HudAlly &ally) { return _texts.get(ally.slot).text.empty(); });
    write(hud.pilots, alone ? words.alone : words.pilots, aux::Title);
}

void HudSystem::showRadar(const Hud &hud, bool cockpit, std::uint8_t local)
{
    using engine::AnchorPoint;
    engine::Text &title = _texts.get(hud.radarTitle);
    title.text = cockpit ? std::string(RadarTitle) : std::string();
    title.size = holo::Title.size;
    title.spacing = holo::Title.spacing;
    const float shown = cockpit ? 1.0f : 0.0f;
    engine::Box &frame = _boxes.get(hud.radarFrame);
    frame.width = RadarFrame.width;
    frame.height = RadarFrame.height;
    _opacities.get(hud.radarFrame).value = shown;
    pin(hud.radarFrame, AnchorPoint::TopLeft, RadarFrame.x, RadarFrame.y + (RadarFrame.height / 2.0f),
        RadarFrame.height);
    pin(hud.radarTitle, AnchorPoint::TopLeft, RadarFrame.x, holo::TitleCentre, holo::Title.size);
    // The vertical lines first, then the horizontal ones
    const std::size_t columns = hud.radarGrid.size() / 2 + 1;
    for (std::size_t index = 0; index < hud.radarGrid.size(); ++index) {
        const ecs::entity_t line = hud.radarGrid[index];
        engine::Box &box = _boxes.get(line);
        _opacities.get(line).value = shown;
        if (index + 1 < columns) {
            box.width = 1.0f;
            box.height = RadarFrame.height;
            const float x = RadarFrame.x +
                            (RadarFrame.width * static_cast<float>(index + 1) / static_cast<float>(columns));
            pin(line, AnchorPoint::TopLeft, x, RadarFrame.y + (RadarFrame.height / 2.0f), box.height);
        } else {
            const std::size_t row = index + 2 - columns;
            const std::size_t rows = hud.radarGrid.size() + 2 - columns;
            box.width = RadarFrame.width;
            box.height = 1.0f;
            const float y =
                RadarFrame.y + (RadarFrame.height * static_cast<float>(row) / static_cast<float>(rows));
            pin(line, AnchorPoint::TopLeft, RadarFrame.x, y, box.height);
        }
    }

    std::size_t used = 0;
    if (cockpit) {
        const render::Vector2 gameSize = _metrics.gameSize();
        for (ecs::entity_t entity = 0; entity < _replicated.size() && used < hud.radarBlips.size();
             ++entity) {
            if (!_replicated.has(entity) || !_positions.has(entity)) {
                continue;
            }
            const Replicated &replicated = _replicated.get(entity);
            const ecs::Position &position = _positions.get(entity);
            const std::optional<RadarBlip> blip =
                radarBlip(replicated.kind, replicated.owner, local, {position.x, position.y}, gameSize);
            if (!blip) {
                continue;
            }
            const ecs::entity_t shownBlip = hud.radarBlips[used++];
            engine::Box &box = _boxes.get(shownBlip);
            box.width = blip->size;
            box.height = blip->size;
            box.color = blip->color;
            box.outline = blip->outlined ? holo::BossOutline : 0.0f;
            _opacities.get(shownBlip).value = 1.0f;
            pin(shownBlip, AnchorPoint::TopLeft, blip->centre.x - (blip->size / 2.0f), blip->centre.y,
                blip->size);
        }
    }
    for (std::size_t index = used; index < hud.radarBlips.size(); ++index) {
        _opacities.get(hud.radarBlips[index]).value = 0.0f;
    }
}

void HudSystem::placeInCockpit(const Hud &hud)
{
    using engine::AnchorPoint;
    // The ship: the pilot and its voice, the lives, the score, the experience, the hit points
    const float pilotHeight = _texts.get(hud.pilot).size;
    pin(hud.pilot, AnchorPoint::TopLeft, ship::Margin, ship::PilotCentre, pilotHeight);
    pin(hud.pilotVoice, AnchorPoint::TopLeft, ship::Margin + measureWidth(hud.pilot) + ship::VoiceGap,
        ship::PilotCentre, _sprites.get(hud.pilotVoice).height);
    for (std::size_t index = 0; index < hud.lives.size(); ++index) {
        const engine::Sprite &life = _sprites.get(hud.lives[index]);
        pin(hud.lives[index], AnchorPoint::TopLeft,
            ship::Margin + (static_cast<float>(index) * (life.width + ship::LifeGap)), ship::LivesCentre,
            life.height);
    }
    pin(hud.score, AnchorPoint::TopLeft, ship::Margin, ship::ScoreCentre, _texts.get(hud.score).size);
    pin(hud.xp, AnchorPoint::TopLeft, ship::Margin, ship::XpCentre, _texts.get(hud.xp).size);
    pin(hud.health, AnchorPoint::TopLeft, ship::Margin, ship::HealthCentre, _texts.get(hud.health).size);
    const float weaponLeft = ship::Margin + measureWidth(hud.health) + XpGap;
    pin(hud.weapon, AnchorPoint::TopLeft, weaponLeft, ship::HealthCentre, _texts.get(hud.weapon).size);
    pin(hud.spareWeapon, AnchorPoint::TopLeft, weaponLeft + measureWidth(hud.weapon) + WeaponGap,
        ship::HealthCentre, _texts.get(hud.spareWeapon).size);

    // The navigation: the wave, the boss, then the kind of game, and under it the ping and its bars
    pin(hud.waveLabel, AnchorPoint::Top, 0.0f, nav::LabelCentre, nav::Label.size);
    pin(hud.wave, AnchorPoint::Top, 0.0f, nav::WaveCentre, nav::Wave.size);
    pin(hud.boss, AnchorPoint::Top, 0.0f, nav::BossCentre, nav::Boss.size);
    pin(hud.modeLight, AnchorPoint::TopLeft, nav::Margin, nav::ModeCentre, nav::LightSize);
    pin(hud.mode, AnchorPoint::TopLeft, nav::ModeLeft, nav::ModeCentre, nav::Mode.size);
    pin(hud.ping, AnchorPoint::TopRight, -nav::Margin, nav::PingCentre, _texts.get(hud.ping).size);
    const float right = nav::Margin + measureWidth(hud.ping) + nav::PingGap;
    for (std::size_t index = 0; index < hud.signal.size(); ++index) {
        const engine::Box &bar = _boxes.get(hud.signal[index]);
        const auto barsAfter = static_cast<float>(hud.signal.size() - 1 - index);
        pin(hud.signal[index], AnchorPoint::TopRight, -(right + (barsAfter * (bar.width + nav::BarGap))),
            nav::SignalBottom - (bar.height / 2.0f), bar.height);
    }

    // The sides: the title, then a line per other pilot, from the top
    const bool alone = std::ranges::all_of(
        hud.allies, [this](const HudAlly &ally) { return _texts.get(ally.slot).text.empty(); });
    if (alone) {
        pin(hud.pilots, AnchorPoint::Center, 0.0f, 0.0f, 0.0f);
    } else {
        pin(hud.pilots, AnchorPoint::TopLeft, aux::Margin, aux::TitleCentre, aux::Title.size);
    }
    for (std::size_t row = 0; row < hud.allies.size(); ++row) {
        const HudAlly &ally = hud.allies[row];
        const float centre = aux::FirstRow + (static_cast<float>(row) * aux::RowStep);
        pin(ally.voice, AnchorPoint::TopLeft, aux::Margin, centre, _sprites.get(ally.voice).height);
        pin(ally.slot, AnchorPoint::TopLeft, aux::SlotLeft, centre, _texts.get(ally.slot).size);
        pin(ally.name, AnchorPoint::TopLeft, aux::NameLeft, centre, _texts.get(ally.name).size);
        pin(ally.score, AnchorPoint::TopRight, -aux::Margin, centre, _texts.get(ally.score).size);
    }
}

void HudSystem::pin(ecs::entity_t entity, engine::AnchorPoint point, float x, float centre, float height)
{
    engine::Anchor &anchor = _anchors.get(entity);
    anchor.point = point;
    anchor.x = x;
    // From the top, the top of the entity; centred on its point otherwise
    anchor.y = point == engine::AnchorPoint::Center ? centre : centre - (height / 2.0f);
}

void HudSystem::showPilot(const Hud &hud, std::uint8_t slot, const std::map<std::uint8_t, std::string> &names,
                          const std::optional<VoiceView> &voice)
{
    const std::vector<game::PlayerState> &players = _sessions.players();
    const auto player = std::ranges::find(players, slot, &game::PlayerState::slot);
    // Before the first snapshot, the player has all its lives
    const std::size_t lives = player == players.end() ? hud.lives.size() : player->lives;
    for (std::size_t index = 0; index < hud.lives.size(); ++index) {
        engine::Sprite &sprite = _sprites.get(hud.lives[index]);
        sprite.texture = index < lives ? hud.look.life : hud.look.lostLife;
        const float x = Margin + (static_cast<float>(index) * (sprite.width + LifeGap));
        placeAbove(hud.lives[index], x, LivesCentre, sprite.height);
    }

    engine::Text &pilot = _texts.get(hud.pilot);
    const auto name = names.find(slot);
    pilot.text =
        name == names.end() ? std::format("{}P", slot + 1) : std::format("{}P · {}", slot + 1, name->second);
    placeAbove(hud.pilot, Margin, PilotCentre, pilot.size);
    showVoiceIcon(hud, hud.pilotVoice, voice ? voiceIconOf(voice->local) : VoiceIcon::None);
    const engine::Sprite &icon = _sprites.get(hud.pilotVoice);
    placeAbove(hud.pilotVoice, Margin + measureWidth(hud.pilot) + PilotVoiceGap, PilotCentre, icon.height);
    engine::Text &score = _texts.get(hud.score);
    score.text = formatScore(player == players.end() ? 0 : player->score);
    placeAbove(hud.score, Margin, ScoreCentre, score.size);
    engine::Text &xp = _texts.get(hud.xp);
    xp.text = player == players.end() ? std::string() : formatXp(*player);
    placeAbove(hud.xp, Margin + measureWidth(hud.score) + XpGap, ScoreCentre, xp.size);
}

void HudSystem::showHealth(const Hud &hud)
{
    engine::Text &health = _texts.get(hud.health);
    health.text = std::format("HP {}", _sessions.localHealth());
    placeAbove(hud.health, Margin, HealthCentre, health.size);
}

void HudSystem::showWeapons(const Hud &hud, bool cockpit)
{
    const GameSession *session = _sessions.session();
    const game::Loadout loadout = session != nullptr ? session->loadout() : game::Loadout{};
    const auto nameOf = [&hud](const std::string &weapon) {
        const auto name = hud.look.words.weapons.find(weapon);
        return name == hud.look.words.weapons.end() ? weapon : name->second;
    };
    engine::Text &weapon = _texts.get(hud.weapon);
    engine::Text &spare = _texts.get(hud.spareWeapon);
    if (loadout[1].empty()) {
        weapon.text.clear();
        spare.text.clear();
        return;
    }
    const std::size_t hand = _sessions.holdsSecondWeapon() ? 1 : 0;
    weapon.text = nameOf(loadout[hand]);
    spare.text = nameOf(loadout[1 - hand]);
    if (cockpit) {
        // Placed on the screen of the ship with the rest (placeInCockpit())
        return;
    }
    const float left = Margin + measureWidth(hud.health) + XpGap;
    placeAbove(hud.weapon, left, HealthCentre, weapon.size);
    placeAbove(hud.spareWeapon, left + measureWidth(hud.weapon) + WeaponGap, HealthCentre, spare.size);
}

void HudSystem::showAllies(const Hud &hud, std::uint8_t local,
                           const std::map<std::uint8_t, std::string> &names,
                           const std::optional<VoiceView> &voice)
{
    std::vector<game::PlayerState> others;
    std::ranges::copy_if(_sessions.players(), std::back_inserter(others),
                         [local](const game::PlayerState &player) { return player.slot != local; });
    const std::size_t shown = std::min(others.size(), hud.allies.size());
    for (std::size_t row = 0; row < hud.allies.size(); ++row) {
        const HudAlly &ally = hud.allies[row];
        engine::Text &slot = _texts.get(ally.slot);
        engine::Text &name = _texts.get(ally.name);
        engine::Text &score = _texts.get(ally.score);
        if (row >= shown) {
            slot.text.clear();
            name.text.clear();
            score.text.clear();
            showVoiceIcon(hud, ally.voice, VoiceIcon::None);
            continue;
        }
        const std::uint8_t player = others[row].slot;
        slot.text = std::format("P{}", player + 1);
        const auto known = names.find(player);
        name.text = known == names.end() ? std::string() : known->second;
        score.text = formatScore(others[row].score);

        VoiceIcon icon = VoiceIcon::None;
        if (voice) {
            const auto pilot = std::ranges::find(voice->others, player, &voice::Participant::slot);
            icon = pilot == voice->others.end() ? VoiceIcon::None : voiceIconOf(*pilot);
        }
        name.color = icon == VoiceIcon::Speaking ? SpeakingName : AllyName;
        showVoiceIcon(hud, ally.voice, icon);

        // The first line on top, the last one just above the ping; from the right: the score, the name, the
        // slot, the icon
        const float centre = AllyCentre + (static_cast<float>(shown - 1 - row) * AllyStep);
        float right = Margin;
        placeAbove(ally.score, -right, centre, score.size);
        right += measureWidth(ally.score) + AllyGap;
        if (!name.text.empty()) {
            placeAbove(ally.name, -right, centre, name.size);
            right += measureWidth(ally.name) + AllyGap;
        }
        placeAbove(ally.slot, -right, centre, slot.size);
        right += measureWidth(ally.slot) + AllyGap;
        placeAbove(ally.voice, -right, centre, _sprites.get(ally.voice).height);
    }
}

void HudSystem::showVoiceIcon(const Hud &hud, ecs::entity_t entity, VoiceIcon icon)
{
    const render::TextureId *texture = voiceIconTexture(hud.look.voice, icon);
    _opacities.get(entity).value = texture != nullptr ? 1.0f : 0.0f;
    if (texture != nullptr) {
        engine::Sprite &sprite = _sprites.get(entity);
        sprite.texture = *texture;
        sprite.tint = voiceIconColor(icon);
    }
}

void HudSystem::showPing(const Hud &hud, std::optional<std::chrono::milliseconds> ping)
{
    engine::Text &text = _texts.get(hud.ping);
    text.text = ping ? std::format("{} MS", ping->count()) : std::string();
    placeAbove(hud.ping, -Margin, PingCentre, text.size);

    const std::size_t lit = ping ? countSignalBars(*ping) : 0;
    const float right = Margin + measureWidth(hud.ping) + PingGap;
    for (std::size_t index = 0; index < hud.signal.size(); ++index) {
        const ecs::entity_t bar = hud.signal[index];
        engine::Box &box = _boxes.get(bar);
        box.color = index < lit ? BarLit : BarOff;
        _opacities.get(bar).value = ping ? 1.0f : 0.0f;
        const auto barsAfter = static_cast<float>(hud.signal.size() - 1 - index);
        place(bar, -(right + (barsAfter * (box.width + BarGap))), -SignalBottom);
    }
}

void HudSystem::place(ecs::entity_t entity, float x, float y)
{
    engine::Anchor &anchor = _anchors.get(entity);
    anchor.x = x;
    anchor.y = y;
}

void HudSystem::placeAbove(ecs::entity_t entity, float x, float centre, float height)
{
    place(entity, x, -(centre - (height / 2.0f)));
}

float HudSystem::measureWidth(ecs::entity_t entity) const
{
    const engine::Text &text = _texts.get(entity);
    return _metrics.measureText(text.font, text.text, text.size, text.spacing).x;
}

} // namespace rtype::client
