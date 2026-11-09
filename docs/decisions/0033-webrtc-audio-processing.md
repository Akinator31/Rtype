# 0033 — The echo, the noise and the gain of the microphone by the audio processing of WebRTC

| | |
|---|---|
| **Status** | Accepted |
| **Date** | 2026-11-08 |

## Context

Without a headset, the speakers of a player come back in its microphone, and the others hear themselves
([Voice](../architecture/voice.md#the-sound-processing)). Between the microphone and the encoder, the client
needs an echo canceller, whose reference is what the speakers play, a noise suppression and a gain control.

[ADR 0032](0032-voice-chat-in-the-game-session.md) named speexdsp and RNNoise for them, and the first version
of the step was built so: the echo canceller of speexdsp, its preprocessor for what is left of the echo and the
gain, RNNoise for the noise, and a measure of the delay of the echo of ours, since speexdsp is a filter only.
Tried by ear, the echo came through for a moment at each start, and each time the delay was measured again:
the filter of speexdsp learns slowly, and nothing suppresses what it has not learnt.

An echo canceller is more than its filter: the delay between what is played and the microphone, found and
followed when it moves; the echo while the filter learns; the echo of small speakers driven hard, which is not
linear; both ends talking at once.

## Options

| | Kept? | Why |
|---|---|---|
| A. speexdsp and RNNoise, the delay ours, made better (measured sooner, kept from one opening to the next) | No | Shorter moments of echo, but still a filter learning slowly, linear only, and the delay, the drift of the clocks and the double talk ours to write |
| B. **webrtc-audio-processing** 2.1 (the audio processing of WebRTC, as PulseAudio publishes it, used by PipeWire and PulseAudio) | **Yes** | AEC3, which finds and follows the delay, suppresses what its filter has not learnt and keeps the voice in double talk; its noise suppression, its gain control AGC2, a high-pass filter; BSD; under 1 % of a core |
| C. PJSIP (pjmedia) | No | A whole SIP stack around the same AEC3; GPL v2 or a paid license, and a GPL client could not link the Discord Social SDK |
| D. The echo cancellation of the system (PipeWire echo-cancel, the communications mode of Windows) | No | Set up by each player, out of our hands, not on every system |
| E. A middleware (Krisp, Vivox) | No | Proprietary, paid, or an external service: refused by ADR 0032 already |

## Decision

**B: the audio processing of WebRTC, alone, takes the echo, the noise and the varying level out of the
microphone**; speexdsp, RNNoise and the measure of the delay of ours are removed.

- Only the audio processing module is taken: not the transport, the encryption nor the signalling of WebRTC,
  which ADR 0032 refused (its option E). The voice still travels in the game session, in Opus.
- Built from its release archive by our CMake (`libs/voice/cmake/WebRtcAudioProcessing.cmake`): the sources its
  `meson.build` files list, so that meson is needed neither on Linux nor on Windows; abseil through CPM, at the
  release its meson wrap pins.
- The interface of the client does not change: `voice::SoundProcessing` and its two settings, noise suppression
  and gain control, the echo cancellation always on.

## Consequences

- The echo of the usual delays (20 to 100 ms) is cut from the first second; a long one (250 ms, a Bluetooth
  headset) passes for about 400 ms, a delay that moves for 1 to 1.5 s.
- Its noise suppression takes out the steady noises (a fan, a hum, a breath) less deeply than RNNoise did, and
  the short ones (keys, clicks) hardly: RNNoise may come back in front of it if the players miss it.
- The client takes webrtc-audio-processing (BSD) and abseil (Apache 2.0) instead of speexdsp and RNNoise; the
  list of the sources is to be read again at each update of webrtc-audio-processing.
- One header of WebRTC needs `<cstdint>` included before it with recent GCC: done by the build, the sources left
  as they are.
