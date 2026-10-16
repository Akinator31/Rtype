# Game engine built on the ECS: events, plus the client side connecting the ECS to the rendering, the input and
# the interface (menus as entities, see ADR 0007).
add_library(rtype_engine
    src/Component/Animation.cpp
    src/Component/Layout.cpp
    src/Component/Widget.cpp
    src/Control/Controls.cpp
    src/Engine/Engine.cpp
    src/Event/Event.cpp
    src/Interface/InterfaceBuilder.cpp
    src/Interface/MenuBuilder.cpp
    src/Interface/Metrics.cpp
    src/Interface/Navigation.cpp
    src/Rendering/SceneRenderer.cpp
    src/Rendering/ColliderDebugRenderer.cpp
    src/Scene/Scenes.cpp
    src/System/AnchorSystem.cpp
    src/System/BoundTextSystem.cpp
    src/System/ColumnLayoutSystem.cpp
    src/System/HitboxSystem.cpp
    src/System/InterfaceSystem.cpp
    src/System/ModelAnimationSystem.cpp
    src/System/SpinSystem.cpp
    src/System/ToastSystem.cpp
    src/System/TweenSystem.cpp
    src/System/WidgetTextSystem.cpp
)
add_library(rtype::engine ALIAS rtype_engine)
target_include_directories(rtype_engine PUBLIC include)
# Public: the public headers use their types
target_link_libraries(rtype_engine PUBLIC rtype::ecs rtype::input rtype::render PRIVATE rtype::physics rtype_options)
