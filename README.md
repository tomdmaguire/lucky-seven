# Lucky Seven

A free true-stereo convolution reverb (VST3) built around the Bricasti M7 impulse responses, with a decay control, a sound map for browsing programs, and quick ways to blend reverb in.

## Features

- **Sound map**: every program plotted by decay time and brightness, coloured by category. Click a dot to load it, use the arrow keys to move to neighbours (Shift+←/→ jumps to the nearest shorter or longer program with a similar tone), and click legend entries to filter by category or favourites. While audio plays, ripples spread from the current program and fade over its decay time.
- **List view** with search across all categories, favourites, and A–Z browsing.
- **Decay**: shortens any program's tail while keeping its frequency-dependent character (highs still die faster than lows).
- **Pre-delay**, plus **Low Cut** (20 Hz–1 kHz) and **High Cut** (1–20 kHz) at 12 dB/oct on the reverb only.
- **Output modes**:
  - **Mix**: dry/wet balance that keeps the dry signal near full level until the top of the knob.
  - **Blend**: dry stays at 0 dB and the knob sets the reverb level in dB, like a dry chain plus a 100% wet reverb in an Ableton rack.
  - **Send**: 100% wet, for return tracks.
- **A/B comparison**: two slots each remember the program and every control; click A or B in the top bar to switch.
- **Resizable window** from 75% to 200%: drag the corner or pick a size from the **⋯** menu.
- **True stereo**: separate left- and right-input responses, as captured from the hardware.
- **Your own IRs**: any WAV, AIFF or FLAC in the IR folder is picked up; each subfolder becomes a category.
- **Light on CPU**: about 2–3% of one core per instance for the longest programs at 44.1/48 kHz on Apple Silicon, and no processing at all once the input has been silent for longer than the tail. At 88.2/96 kHz the cost roughly triples, so use a buffer of 256 samples or more there.

## Installing

Download the latest zip from the [Releases page](https://github.com/tomdmaguire/lucky-seven/releases). It contains both formats, built for Apple Silicon and Intel Macs:

- `Lucky Seven.vst3` goes in `~/Library/Audio/Plug-Ins/VST3`
- `Lucky Seven.component` (Audio Unit, for Logic and GarageBand) goes in `~/Library/Audio/Plug-Ins/Components`

Lucky Seven isn't signed with an Apple developer certificate, so macOS blocks it the first time. Unblock it once in Terminal:

```sh
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/"Lucky Seven.vst3"
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/"Lucky Seven.component"
```

Or, if your DAW reports it as blocked, open **System Settings → Privacy & Security** and click **Allow Anyway**. Then rescan plugins in your DAW.

## Getting the impulse responses

Lucky Seven does not ship with any impulse responses. Download the free **Bricasti M7 impulse response archive** from [Samplicity's downloads page](https://www.samplicity.com/downloads/) and unzip the whole archive into:

```
~/Music/Lucky Seven/IRs
```

The folder is created the first time the plugin opens. To use a different folder, open the **⋯** menu at the top right and choose **Choose IR folder...**. After adding files, choose **Rescan IR folder** from the same menu or reopen the plugin window.

The Samplicity IRs are offered free of charge and as-is. Please don't redistribute them, and don't bundle them with this plugin or any other product.

## Building

Requirements: macOS 13 or newer, Xcode (with the command line tools selected) and CMake 3.25 or newer. Ninja is optional but faster. The build is universal (Apple Silicon and Intel).

```sh
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

JUCE is fetched automatically by CMake. The build copies the VST3 and AU into your plug-in folders. A standalone app is built alongside them for quick testing.

Pushing a tag such as `v1.0.1` builds, tests and publishes a release zip through GitHub Actions.

### Tests and tools

```sh
ctest --test-dir build                                    # unit tests
LUCKY_SEVEN_IR_ROOT="$HOME/Music/Lucky Seven/IRs" \
  build/LuckySevenTests_artefacts/Release/LuckySevenTests  # also checks the real Bricasti library
build/LuckySevenBench_artefacts/Release/LuckySevenBench <ir.wav> [decay ratio] [block size]
build/LuckySevenProbe_artefacts/Release/LuckySevenProbe <plugin.vst3> <parameter name>
build/LuckySevenSnapshot <out.png> [list|active|timing|150%]  # render the UI to an image
build/LuckySevenSnapshot --check-state                        # A/B and save/restore check
```

`LuckySevenBench` measures CPU cost per instance. `LuckySevenProbe` hosts any VST3 and measures how its mix knob sets dry and wet levels.

## Licence

Lucky Seven is released under the [GNU Affero General Public License v3.0](LICENSE). It is built with [JUCE](https://juce.com), which is used here under the AGPLv3.

Lucky Seven is an independent project and is not affiliated with Bricasti Design or Samplicity.
