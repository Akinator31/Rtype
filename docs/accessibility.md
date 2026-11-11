# Accessibility

This page lists the accessibility goals of the project, then what the game offers today.

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
| **Physical and motor** | Every action can be bound to any key or gamepad input, with several inputs per action. The Controls page, from the Settings page, lets the player choose the key of each action, warns when a key is taken by another action, and puts the defaults back. | [Input](architecture/input.md#choosing-the-keys) |
| **Physical and motor** | Gamepad support, with analog sticks and an adjustable dead zone for worn sticks. | [Input](architecture/input.md#bindings) |
| **Physical and motor** | Controls work on any keyboard layout: keys are physical positions, so WASD is ZQSD on AZERTY, and every key is shown as it is printed on the keyboard of the player. | [Input](architecture/input.md#keys-and-buttons) |
| **Audio** | Separate volumes for the music and the sound effects, on top of a master volume. The Settings page sets the music and effects volumes, kept from one run to the next; the master volume is still to be added to it. | [Audio](architecture/audio.md#volumes) |
| **Visual** | The game keeps its aspect ratio and scales to any window size, without being cropped or stretched. | [Rendering](architecture/rendering.md#logical-resolution) |
| **Visual** | Colour filters for the three kinds of colour blindness (protanopia, deuteranopia, tritanopia), applied to the whole image, chosen on the Settings page and kept from one run to the next. | [Rendering](architecture/rendering.md#colour-filters), [Settings](architecture/settings.md#the-settings-page) |
| **Visual** | High-contrast filter for players with low vision, chosen on the Settings page. | [Rendering](architecture/rendering.md#colour-filters) |
| **Visual** | Simulation filters showing the game as colour-blind players see it, for the team to check that sprites stay distinguishable. | [Rendering](architecture/rendering.md#colour-filters) |
| **Physical and motor** | Every menu can be used with the keyboard only, a gamepad only, or the mouse; holding a direction repeats it. | [Scenes and interface](architecture/scenes-and-interface.md#navigation) |
| **Visual** | The focused widget is shown three ways: a frame, another colour, and a bigger size, so that it is not told by colour only. | [Scenes and interface](architecture/scenes-and-interface.md#menus) |
| **Visual** | Each ship casts an aura in the colour of its side (the player, its allies, the Bydos), whose three colours the player chooses on the Settings page, or turns off. | [Auras](architecture/client-game.md#auras) |
| **Visual** | The death screen tells the lives left with pictures and the respawn with a countdown, not only with colours. | [Death screen](architecture/client-game.md#death-screen) |
| **Audio** | The kills and the deaths of the game are told as lines in a corner of the screen, not only by sounds; the player who talks is shown in the HUD. | [Events](architecture/client-game.md#events), [Voice](architecture/voice.md#in-the-hud-and-the-pause-menu) |
| **Audio** | Push-to-talk, voice activity or no voice at all, with mute and deafen keys: no player has to talk or to hear the others. | [Voice](architecture/voice.md#the-talk-modes) |
| **Mental and cognitive** | The interface is in English or in French, switched at once on the Settings page. | [Localization](architecture/localization.md) |
| **Mental and cognitive** | The game can be paused at any time offline; a note under the Settings page explains the row in focus. | [Settings](architecture/settings.md#the-settings-page) |
| **Physical and motor** | In VR, the player stays seated, and turns by steps (snap turn) rather than smoothly, to avoid motion sickness. | [VR](architecture/vr.md#the-vr-page) |

## In this documentation

- Light and dark themes, both with sufficient contrast.
- Full keyboard navigation and built-in search.
- A heading structure that screen readers can follow.
- Every image has an alternative text, and every diagram is followed by a sentence summarising it.
