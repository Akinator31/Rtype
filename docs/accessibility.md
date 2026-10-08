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
| **Physical and motor** | Every action can be bound to any key or gamepad input, with several inputs per action. The engine supports it; the menu to change the controls is still to be done. | [Input](architecture/input.md#actions) |
| **Physical and motor** | Gamepad support, with analog sticks and an adjustable dead zone for worn sticks. | [Input](architecture/input.md#bindings) |
| **Physical and motor** | Controls work on any keyboard layout: keys are physical positions, so WASD is ZQSD on AZERTY. | [Input](architecture/input.md#keys-and-buttons) |
| **Audio** | Separate volumes for the music and the sound effects, on top of a master volume. The options menu sets the effects volume; the music and master volumes are still to be added to it. | [Audio](architecture/audio.md#volumes) |
| **Visual** | The game keeps its aspect ratio and scales to any window size, without being cropped or stretched. | [Rendering](architecture/rendering.md#logical-resolution) |
| **Visual** | Colour filters for the three kinds of colour blindness (protanopia, deuteranopia, tritanopia), applied to the whole image, chosen in the options menu. | [Rendering](architecture/rendering.md#colour-filters) |
| **Visual** | High-contrast filter for players with low vision. | [Rendering](architecture/rendering.md#colour-filters) |
| **Visual** | Simulation filters showing the game as colour-blind players see it, for the team to check that sprites stay distinguishable. | [Rendering](architecture/rendering.md#colour-filters) |
| **Physical and motor** | Every menu can be used with the keyboard only, a gamepad only, or the mouse; holding a direction repeats it. | [Scenes and interface](architecture/scenes-and-interface.md#navigation) |
| **Visual** | The focused widget is shown three ways: a frame, another colour, and a bigger size, so that it is not told by colour only. | [Scenes and interface](architecture/scenes-and-interface.md#menus) |

## In this documentation

- Light and dark themes, both with sufficient contrast.
- Full keyboard navigation and built-in search.
- A heading structure that screen readers can follow.
- Every image has an alternative text, and every diagram is followed by a sentence summarising it.
