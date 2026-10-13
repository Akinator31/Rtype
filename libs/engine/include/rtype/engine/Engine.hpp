#pragma once

/// @file
/// Everything of the client engine in one include, like rtype/ecs/Ecs.hpp for the ECS.
///
/// - Component/: the data attached to entities (drawing, animation, widgets, layout).
/// - System/: the systems working on them, one per file.
/// - Interface/: what the interface is built with (MenuBuilder, InterfaceBuilder, navigation controls, style,
///   metrics).
/// - Rendering/: the SceneRenderer, which draws the entities.
/// - Scene/: the stack of open scenes (home screen, game, pause menu...).
/// - Control/: the virtual gamepad the keyboard and the real gamepads are mapped onto.

#include "rtype/engine/Component/Animation.hpp"
#include "rtype/engine/Component/Drawing.hpp"
#include "rtype/engine/Component/Layout.hpp"
#include "rtype/engine/Component/Widget.hpp"
#include "rtype/engine/Control/Control.hpp"
#include "rtype/engine/Control/Controls.hpp"
#include "rtype/engine/Interface/InterfaceBuilder.hpp"
#include "rtype/engine/Interface/MenuBuilder.hpp"
#include "rtype/engine/Interface/Metrics.hpp"
#include "rtype/engine/Interface/Navigation.hpp"
#include "rtype/engine/Interface/Style.hpp"
#include "rtype/engine/Rendering/SceneRenderer.hpp"
#include "rtype/engine/Scene/Scenes.hpp"
#include "rtype/engine/System/AnchorSystem.hpp"
#include "rtype/engine/System/BoundTextSystem.hpp"
#include "rtype/engine/System/ColumnLayoutSystem.hpp"
#include "rtype/engine/System/HitboxSystem.hpp"
#include "rtype/engine/System/InterfaceSystem.hpp"
#include "rtype/engine/System/TweenSystem.hpp"
#include "rtype/engine/System/WidgetTextSystem.hpp"
