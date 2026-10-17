# Race audio

Race sound uses car banks, original control data and native mixing. Engine sound, effects, music transport and timing have separate evidence. Agreement of control commands does not certify the final waveform, spatialization or host audio latency.

## Evidence and limitations

### Camera cuts and music transitions

Camera displacement is used for Doppler only within one continuous shot.
`003B5E80 +0x58..0xBC` restarts the car voices on a discontinuity;
`00398B70 +0x90..0xB0` restarts ambience and restores reverb depth to
`0x0FFF`. TV and Dive cameras now publish their shot boundaries, and the
PC bridge consumes each cut once, including changes between driving and
external views. It no longer interprets the jump as camera velocity.
`race_audio_camera_cuts` injects a 1000-unit jump and checks neutral Doppler
and restart, then compares registers and PCM at one, two and four ticks per
draw. Its negative treats the jump as continuous motion. This is transport
coverage, not a complete original-waveform comparison.

Menu music fades run on the audio device's sample clock. The UI must retain
the outgoing producer until all fade samples have played, including while
an incoming PCM stream is being prepared. Previously, its next update could
stop the source immediately. An unchanged menu music request also survives
an internal loading overlay. `audio_backend` checks that a 200 ms fade
remains active after 30 ms and finishes; its negative prematurely stops it.
No extra duration or universal track crossfade is invented.

Normal race pause follows `0038B428 -> 0033C3E8`: the car, ambient and
refuelling sounds stop. The independent music stream is retained. The
world's paused update (`00389638 -> 0038B2B0`) continues handling the pause
movie; this is separate from the menu scripts' explicit `pauseStream`.

The [development record](notes/RACE_AUDIO.md) retains source addresses,
experiments, positive and negative cases, and historical limitations.
Consult [Fidelity](FIDELITY.md) for each comparison's grade and scope.
Earlier entries may be superseded by later results.

## Menu MIDI continuity

The Arcade and GT Mode MIDI transport is now shared between the outer UI
update and the presentation pump used by script `Sync`/`panIn`. Previously,
only PCM streams were serviced during those waits: a requested MIDI group
could remain silent until the entire animation returned. `keepMusic` calls
start the original sound service synchronously (`0023E3D8 +0xAC` through
`004616E8`), and the same-group check in `004616E8 +0x30..+0x58` retains an
already-playing sequence. Native waits must preserve that distinction.

The app owns the transport state; both paths use it, so repeated requests
and internal loading overlays retain the source and its sample position.
An authored fade still retains its producer until the device consumes the
last fade sample. Fresh music remains gated behind the loading overlay;
this change adds no arbitrary fade or universal crossfade.

The decoder's readiness threshold now matches the SDL transport's existing
8,192-frame preroll. Publishing after only 4,096 frames could immediately
queue another 4,096 frames of silence when the source was attached. This
buffer is PC transport headroom, not a PS2 timing constant.

`menu_music_transport` exercises the actual app transport with disc MIDI
and deterministic menu/loader/device boundaries. It checks publication
inside waits without an outer update, unchanged-source/sample continuity,
loading gates, completed fades and readiness for single-track and playlist
sources. The two negative cases deliberately restore deferred publication
and the insufficient prefix. Native synthesis/prefix and SDL fade tests
remain separate. Real executable checks reached GT Mode Home and Garage
with isolated profiles; idle Arcade/GT Mode captures were also inspected
for digital silence. These regressions do not identify every possible
intermittent cut or certify timing/waveform against a PS2 recording.
