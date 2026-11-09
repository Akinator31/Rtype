#include "Game/HudSystem.hpp"

#include <algorithm>
#include <format>
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
/// Between the score and the experience.
constexpr float XpGap = 21.3f;

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
      _boxes(use<engine::Box>(caches)), _opacities(use<engine::Opacity>(caches))
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
        stretchShadows(hud);
        showPilot(hud, local, names, voice);
        showHealth(hud);
        showAllies(hud, local, names, voice);
        showPing(hud, ping);
    }
}

void HudSystem::stretchShadows(const Hud &hud)
{
    const float width = _metrics.visibleArea().width;
    _boxes.get(hud.topShadow).width = width;
    _boxes.get(hud.bottomShadow).width = width;
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
