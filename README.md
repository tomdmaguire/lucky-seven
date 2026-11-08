# Lucky Seven

A free true-stereo convolution reverb (VST3) built around the Bricasti M7 impulse responses, with a decay control, a sound map for browsing programs, and quick ways to blend reverb in.

## Features

- **Sound map**: every program plotted by decay time and brightness, coloured by category. Click a dot to load it, use the arrow keys to move to neighbours, and click legend entries to filter by category or favourites.
- **List view** with search across all categories, favourites, and A–Z browsing.
- **Decay**: shortens any program's tail while keeping its frequency-dependent character (highs still die faster than lows).
- **Pre-delay**, plus **Low Cut** (20 Hz–1 kHz) and **High Cut** (1–20 kHz) at 12 dB/oct on the reverb only.
- **Output modes**:
  - **Mix**: dry/wet balance that keeps the dry signal near full level until the top of the knob.
  - **Blend**: dry stays at 0 dB and the knob sets the reverb level in dB, like a dry chain plus a 100% wet reverb in an Ableton rack.
  - **Send**: 100% wet, for return tracks.
- **True stereo**: separate left- and right-input responses, as captured from the hardware.
- **Your own IRs**: any WAV, AIFF or FLAC in the IR folder is picked up; each subfolder becomes a category.
- **Light on CPU**: about 2–3% of one core per instance for the longest programs on Apple Silicon, and no processing at all once the input has been silent for longer than the tail.

## Getting the impulse responses

Lucky Seven does not ship with any impulse responses. Download the free **Bricasti M7 impulse response archive** from [Samplicity's downloads page](https://www.samplicity.com/downloads/) and unzip the whole archive into:

```
~/Music/Lucky Seven/IRs
```

The folder is created the first time the plugin opens. To use a different folder, open the **⋯** menu at the top right and choose **Choose IR folder...**. After adding files, choose **Rescan IR folder** from the same menu or reopen the plugin window.

The Samplicity IRs are offered free of charge and as-is. Please don't redistribute them, and don't bundle them with this plugin or any other product.

## Building

Requirements: macOS on Apple Silicon, Xcode (with the command line tools selected) and CMake 3.25 or newer. Ninja is optional but faster.

```sh
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

JUCE is fetched automatically by CMake. The build copies `Lucky Seven.vst3` into `~/Library/Audio/Plug-Ins/VST3`. A standalone app is built alongside it for quick testing.

### Tests and tools

```sh
ctest --test-dir build                                    # unit tests
LUCKY_SEVEN_IR_ROOT="$HOME/Music/Lucky Seven/IRs" \
  build/LuckySevenTests_artefacts/Release/LuckySevenTests  # also checks the real Bricasti library
build/LuckySevenBench_artefacts/Release/LuckySevenBench <ir.wav> [decay ratio] [block size]
build/LuckySevenProbe_artefacts/Release/LuckySevenProbe <plugin.vst3> <parameter name>
```

`LuckySevenBench` measures CPU cost per instance. `LuckySevenProbe` hosts any VST3 and measures how its mix knob sets dry and wet levels.

## Licence

Lucky Seven is released under the [GNU Affero General Public License v3.0](LICENSE). It is built with [JUCE](https://juce.com), which is used here under the AGPLv3.

Lucky Seven is an independent project and is not affiliated with Bricasti Design or Samplicity.
