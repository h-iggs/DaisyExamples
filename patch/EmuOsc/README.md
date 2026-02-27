# EmuOsc

## Author

Ben Sergentanis

## Description

This build now embeds the `reSID` SID chip emulator and plays a mono SID
voice from incoming USB MIDI note on/off events on the Daisy Patch platform.

Note: `reSID` is GPL-licensed. If you distribute firmware built with it, that
license applies to the combined distribution.

[Source Code](https://github.com/electro-smith/DaisyExamples/tree/master/patch/PolyOsc)

## Controls
| Control | Description | Comment |
| --- | --- | --- |
| Ctrl 1 | Pulse Width | Affects the SID pulse waveform |
| Ctrl 2 | Attack | Voice 1 attack time |
| Ctrl 3 | Release | Voice 1 release time |
| Ctrl 4 | Volume | SID master volume |
| Encoder | Waveform | Cycle through Triangle, Saw, Pulse, and Noise |
| USB MIDI Note On/Off | SID pitch/gate | Plays and releases the SID voice |
| Audio Out 1-2 | SID + input passthrough | Stereo output |
| Audio Out 3-4 | SID monitor | Dry SID signal |
