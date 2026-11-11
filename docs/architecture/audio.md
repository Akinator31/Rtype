# Audio

The audio library (`libs/audio`, namespace `rtype::audio`) plays the sound effects and the music of the client,
the voices of the other players of a game online, and gives what the output plays, the reference of the echo
cancellation of the microphone.
Like the render library, it wraps raylib: its public headers contain no raylib type.

| Class         | Role                                                                              |
| ------------- | --------------------------------------------------------------------------------- |
| `AudioDevice` | Opens the audio output and closes it when destroyed. Only one can exist.          |
| `AudioPlayer` | Loads sounds and music once, plays them, and holds the music and effects volumes. |
| `VoiceStreams` | Plays the voices of the other players, one stream each ([The voices](#the-voices)). Only one can exist. |
| `OutputLoopback` | Gives what the output plays, all mixed, for the echo cancellation ([What is played](#what-is-played)). Only one can exist. |

```cpp
Window window;
Engine engine;
AudioDevice device;                                           // after the window, see below
AudioPlayer audio(device, applicationDirectory() / "assets",
                  engine.getThreads().getAssetsWorker());

const SoundId shot = audio.loadSound("sounds/shot.wav");
const MusicId stage = audio.loadMusic("music/stage1.ogg");
audio.playMusic(stage);

while (!window.shouldClose()) {
    audio.update();                                           // once per frame, for the music
    if (controls.wasPressed(Action::Shoot)) {
        audio.play(shot);
    }
}
```

## The audio device

`AudioDevice` opens the default audio output in its constructor and closes it in its destructor (RAII). The
`AudioPlayer` takes it as a constructor parameter: declare the player after the device, so that sounds are
freed before the output closes.

**No sound is not an error.** On a computer without sound card, in a virtual machine or in the CI, the output
cannot be opened: a warning is logged and the game keeps running silently, with every sound ignored. Audio can
also be disabled on purpose:

```cpp
AudioDevice device({.enabled = false});   // tests, servers, or a "no sound" option
```

!!! tip "Create it after the window"

    raylib's messages are redirected to the [log](logging.md) when the `Window` is created. An `AudioDevice`
    created before the window prints raylib's messages about the audio output directly to the console.

## Sound effects and music

|                  | Sound effects                                    | Music                                                                        |
| ---------------- | ------------------------------------------------ | ---------------------------------------------------------------------------- |
| Typical use      | Shots, explosions, menu sounds                   | Background music of a stage                                                  |
| Loading          | Fully decoded in memory                          | Streamed from the file while it plays                                        |
| At the same time | Several sounds, and the same sound several times | One music at a time                                                          |
| Functions        | `loadSound()`, `play()`                          | `loadMusic()`, `playMusic()`, `stopMusic()`, `pauseMusic()`, `resumeMusic()` |

Each file is loaded once: loading the same path again returns the same handle (`SoundId`, `MusicId`), like
textures in the render library. Both use the same `ResourceCache`, from the [core library](index.md#libraries).

`loadSoundAsync()` and `loadMusicAsync()` return shared futures. Call `finishPendingLoads()` on the audio owner's thread each frame, then use the handle once its future is ready. Sound files are decoded on the engine's assets worker; audio buffers are created on the main thread. Pass `engine.getThreads().getAssetsWorker()` to the player constructor. See [ADR 0022](../decisions/0022-engine-thread-manager.md).

### Playing a sound several times at once

With raylib, playing a sound that is already playing restarts it: fast shots would cut each other. Each sound
effect therefore has **4 voices**, which share the same samples in memory. `play()` uses a free voice, or
restarts the oldest one if all four are busy.

`play()` takes optional settings:

```cpp
audio.play(explosion, {.volume = 0.8f, .pitch = 0.9f, .pan = -0.5f});
```

| Setting  | Effect                                                                                                                                   |
| -------- | ---------------------------------------------------------------------------------------------------------------------------------------- |
| `volume` | Volume of this sound, between 0 and 1, multiplied by the effects volume.                                                                 |
| `pitch`  | Speed and pitch: 1 is unchanged, 2 is one octave higher. Small random variations (0.9 to 1.1) keep a repeated sound from being tiresome. |
| `pan`    | Position from left (-1) to right (1), 0 being centred: an explosion on the left of the screen can be heard on the left.                  |

### Streaming the music

A music is too long to be decoded in memory at once: it is read from its file in small parts while it plays.
`update()` reads the next part; it must be called **once per frame**, otherwise the music stutters, then stops.

## In the engine

In the client, no system calls the `AudioPlayer`, as none calls the `Renderer`: a system gives a
`SoundEffect` (`rtype/engine/Component/Sound.hpp`: a `SoundId` and its `PlaySettings`) to an entity, and the
engine's `SoundSystem` plays it at its next update, then removes it from the entity. The `SoundSystem` also calls
`AudioPlayer::update()`, once per frame, for the music.

```cpp
AudioDevice device;
AudioPlayer audio(device, applicationDirectory() / "assets");   // before the engine: it outlives the system
Engine engine;
engine.getEcs().registerSystem<SoundSystem>(audio);             // after the systems that give sounds

ecs.addComponent<SoundEffect>(shot, audio.loadSound("sounds/shot.wav"), PlaySettings{.volume = 0.5f});
```

The engine links the audio library, which is why it stays client-only with the rendering and the input. The
shots and the explosions of the ships ([A game in the client](client-game.md#sounds)) and the buttons of the
menus ([Scenes and interface](scenes-and-interface.md#navigation)) use it.

## Volumes

| Volume  | Set with                          | Applies to                            |
| ------- | --------------------------------- | ------------------------------------- |
| Master  | `AudioDevice::setMasterVolume()`  | Everything                            |
| Music   | `AudioPlayer::setMusicVolume()`   | The music, immediately                |
| Effects | `AudioPlayer::setEffectsVolume()` | The sound effects played from then on |

All volumes are between 0 and 1, and clamped to this range. Separate music and effects volumes are one of the
[accessibility](../accessibility.md) features: some players need the effects louder than the music to follow
the game, others are disturbed by the music. In the options menu, _Effects volume_ is changed with
++arrow-left++ and ++arrow-right++.

## Supported formats

| Format                  | Supported                              |
| ----------------------- | -------------------------------------- |
| WAV, OGG, MP3, QOA      | Yes                                    |
| XM, MOD (tracker music) | Yes                                    |
| FLAC                    | No: disabled in raylib's default build |

**WAV** suits short sound effects (no decoding cost), **OGG** suits music (small files, good quality, free
format).

## The voices

`VoiceStreams` is the `voice::Playback` of the client ([Voice](voice.md#the-devices)): one `AudioStream` of
raylib per slot, 48 kHz mono, signed 16 bits, mixed by raylib with the music and the effects, each at the volume
of its player (`setVolume()`, from 0 to 2, under the master volume), from a side (`setPan()`, from -1 on the left
to 1 on the right, in the middle by default, kept for a stream not open yet): in a VR headset, each voice is heard
from the side of the board the ship of its player is on ([VR](vr.md#the-sound)).

```cpp
const AudioDevice device;
VoiceStreams voices(device);                      // destroyed before the device
voice::GameVoice voice(session, microphone, voices);
```

- **Fed without a wait.** `write()`, on the game loop, puts the voice decoded in a ring buffer of 200 ms; the
  callback of the stream, on the audio thread of raylib, takes what is there and plays silence for the rest
  ([Threads](threads.md#the-audio-threads)). A stream 200 ms behind drops what does not fit.
- **PCM, not Opus.** Whatever carried a voice, the game or Discord, it is played here: one mixer, one volume
  per player, and one reference for the echo cancellation.
- **One at a time.** The callbacks of raylib take no pointer: each slot has its own function, which finds the
  streams in a variable of `VoiceStreams.cpp`. A second `VoiceStreams` throws, as a second `AudioDevice`.
- **Without an audio output** (`AudioDevice::isAvailable()` false), it is not `available()` and plays nothing:
  the player has no voice.
- **At 48 kHz.** raylib is compiled with `AUDIO_DEVICE_SAMPLE_RATE=48000` (`libs/CMakeLists.txt`): it mixes at
  the rate of the voice, and miniaudio converts when the device runs at another one. The voices are played
  without being resampled.

## What is played

`OutputLoopback` is the `voice::Loopback` of the client ([Voice](voice.md#the-sound-processing)): what the
output plays, the voices, the music and the effects, taken where raylib has mixed them, and given to the echo
cancellation of the microphone as its reference.

```cpp
const AudioDevice device;
OutputLoopback loopback(device);                  // destroyed before the device
voice::GameVoice voice(session, microphone, voices, &loopback);
```

- **Open with the microphone.** `GameVoice` opens it when it opens the microphone, and closes it with it:
  `open()` attaches a processor to the mix (`AttachAudioMixedProcessor()`), `close()` detaches it.
- **Before the master volume.** The processor sees the mix of raylib, which miniaudio scales by the master
  volume afterwards: the echo canceller learns that gain with the room.
- **In mono, without a wait.** The callback, on the audio thread of raylib, mixes the two channels down to one,
  as signed 16 bits, into a ring buffer of one second that `read()` empties on the game loop
  ([Threads](threads.md#the-audio-threads)); what does not fit is dropped.
- **One at a time**, as `VoiceStreams`: the processor of raylib takes no pointer. Without an audio output,
  `open()` fails: nothing is played, there is no echo.

## Testing

`tests/audio.cpp` tests the library with audio disabled, so that the tests give the same result with or
without a sound card: volumes, single device, handles of sounds loaded twice, missing files, and the state of
the music. Missing files are reported even when audio is disabled, so that they are noticed on every
computer. The voice is tested with a fake `voice::Playback` and a fake `voice::Loopback`
([Voice](voice.md#testing)): `VoiceStreams` and `OutputLoopback` need an audio output.

## Limits and next steps

- **No spatial sound** beyond left-right panning. For VR, sounds positioned in 3D around the player would help;
  raylib does not provide it, so it would need another approach.
- **The voices** are played here, but the voice chat itself is apart, in `libs/voice` ([Voice](voice.md)); each
  is in the middle, without a pan: a voice placed from its ship was tried and left out.
- **No fading** between two musics: `playMusic()` switches at once.
- The effects volume does not change the sounds already playing, which are short.
