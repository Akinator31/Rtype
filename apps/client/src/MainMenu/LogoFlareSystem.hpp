#pragma once

#include "MainMenu/Intro.hpp"
#include "MainMenu/LogoFlare.hpp"
#include "rtype/ecs/Component/ComponentCache.hpp"
#include "rtype/ecs/Component/ComponentCaches.hpp"
#include "rtype/ecs/System/System.hpp"
#include "rtype/engine/Component/Drawing.hpp"
#include "rtype/engine/Component/Layout.hpp"

namespace rtype::client
{

/// Plays the LogoFlares from the time of their Intro: writes their Anchor, Scale, Opacity and the rotation of
/// their Sprite, relative to the Anchor and the Scale of their logo.
///
/// Register it after the IntroSystem, which moves the logo, and before the AnchorSystem, which places the
/// flares from their Anchor.
class LogoFlareSystem : public ecs::System
{
    ecs::ComponentCache<LogoFlare> &_flares;
    ecs::ComponentCache<Intro> &_intros;
    ecs::ComponentCache<engine::Anchor> &_anchors;
    ecs::ComponentCache<engine::Scale> &_scales;
    ecs::ComponentCache<engine::Opacity> &_opacities;
    ecs::ComponentCache<engine::Sprite> &_sprites;

    [[nodiscard]] float getIntroTime(const LogoFlare &flare) const;
    void placeFlare(ecs::entity_t entity, const LogoFlare &flare);

public:
    explicit LogoFlareSystem(ecs::ComponentCaches &caches);

    void update(float elapsed) override;
};

} // namespace rtype::client
