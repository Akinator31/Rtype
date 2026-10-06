# Accessibility

!!! warning "Draft"

    This page lists the accessibility goals of the project. Each item is updated with what is actually
    implemented.

## In the game

| Category | Planned features |
|---|---|
| **Physical and motor** | Remappable controls, gamepad support, adjustable game speed, auto-fire option |
| **Visual** | Colour-blind friendly palettes, player ships distinguishable by shape and not only by colour, scalable UI text, high-contrast mode |
| **Audio** | Visual cues for important sounds, separate volume controls for music and effects |
| **Mental and cognitive** | Difficulty levels, pause at any time, short and clear menus, tutorial |

## Implemented so far

| Category | Feature | Where |
|---|---|---|
| **Physical and motor** | Every action can be bound to any key or gamepad input, with several inputs per action. The engine supports it; the options menu is still to be done. | [Input](architecture/input.md#actions) |
| **Physical and motor** | Gamepad support, with analog sticks and an adjustable dead zone for worn sticks. | [Input](architecture/input.md#bindings) |
| **Physical and motor** | Controls work on any keyboard layout: keys are physical positions, so WASD is ZQSD on AZERTY. | [Input](architecture/input.md#keys-and-buttons) |
| **Visual** | The game keeps its aspect ratio and scales to any window size, without being cropped or stretched. | [Rendering](architecture/rendering.md#logical-resolution) |

## In this documentation

- Light and dark themes, both with sufficient contrast.
- Full keyboard navigation and built-in search.
- A heading structure that screen readers can follow.
- Every image has an alternative text, and every diagram is followed by a sentence summarising it.
