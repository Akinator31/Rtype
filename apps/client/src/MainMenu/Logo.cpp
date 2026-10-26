#include "MainMenu/Logo.hpp"

#include "Assets.hpp"

namespace rtype::client
{

ecs::entity_t createLogo(engine::InterfaceBuilder &interface, render::ResourceManager &resources)
{
    const float height = logoWidth * logoPicture.height / logoPicture.width;
    return interface.sprite(resources.loadTexture(RTYPE_IMAGE_LOGO, render::TextureFiltering::Smooth),
                            logoPicture, logoWidth, height, {engine::AnchorPoint::Center, 0.0f, 0.0f});
}

} // namespace rtype::client
