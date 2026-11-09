# Lucky Seven

Find the right reverb in seconds. Lucky Seven is a free true-stereo convolution reverb for macOS (VST3 and AU). It comes with 134 Bricasti M7 reverbs, works with any impulse-response library, and adds a decay control, a sound map for browsing, and quick ways to blend the reverb in.

Website: https://tomdmaguire.github.io/lucky-seven/

## Features

- **Sound map**: every program plotted by decay time and brightness, coloured by category. Click a dot to load it, use the arrow keys to move to neighbours (Shift+←/→ jumps to the nearest shorter or longer program with a similar tone), and click legend entries to filter by category or favourites. The search box narrows it further. While audio plays, ripples spread from the current program and fade over its decay time.
- **List view** with search across all categories, favourites, and A–Z browsing.
- **Decay**: shortens any program's tail while keeping its frequency-dependent character (highs still die faster than lows).
- **Pre-delay**, plus **Low Cut** (20 Hz–1 kHz) and **High Cut** (1–20 kHz) at 12 dB/oct on the reverb only.
- **Width**: narrows the reverb from the IR's natural stereo image down to mono, so it can sit behind its source instead of spreading across the mix. The dry signal is untouched.
- **Output modes**:
  - **Mix**: dry/wet balance that keeps the dry signal near full level until the top of the knob.
  - **Blend**: dry stays at 0 dB and the knob sets the reverb level in dB, like a dry chain plus a 100% wet reverb in an Ableton rack.
  - **Send**: 100% wet, for return tracks.
- **A/B comparison**: two slots each remember the program and every control; click A or B in the top bar to switch.
- **Resizable window** from 75% to 200%: drag the corner or pick a size from the **⋯** menu.
- **True stereo**: separate left- and right-input responses, as captured from the hardware.
- **Your own IRs**: add any number of folders of WAV, AIFF or FLAC impulse responses (**··· → IR folders → Add a folder…**). Each subfolder becomes a category, numbered folder names are tidied, left/right file pairs play in true stereo, and recordings with a hiss floor are measured and trimmed correctly. Works with Ableton's Convolution Reverb pack: add `~/Music/Ableton/Factory Packs/Convolution Reverb/IRs`.
- **Light on CPU**: the long tail is computed on a background thread, so the audio thread uses about 1% of a core per instance with no bursts, even at a 64-sample buffer and with 30-second IRs. Instances also stop processing once the input has been silent for longer than the tail.

## Installing

Download [**Lucky-Seven.pkg**](https://github.com/tomdmaguire/lucky-seven/releases/latest/download/Lucky-Seven.pkg) and open it. It installs the VST3 and Audio Unit (Apple Silicon and Intel) into `/Library/Audio/Plug-Ins`.

Lucky Seven isn't signed with a paid Apple developer certificate, so macOS won't open the installer the first time. Click **Done**, then go to **System Settings → Privacy & Security**, scroll down and click **Open Anyway**.

A zip for installing by hand is also on the [releases page](https://github.com/tomdmaguire/lucky-seven/releases/latest); its `INSTALL.txt` explains the steps.

## The Bricasti M7 set

The first time you open Lucky Seven, click **Download Bricasti IRs**. It fetches Samplicity's free captures of the Bricasti M7 (134 programs) straight from Samplicity and sets everything up. To do it by hand instead, download Samplicity's free Bricasti M7 archive, [`Samplicity - Bricasti IRs version 2023-10.zip`](https://cdn.samplicity.com/downloads/Samplicity%20-%20Bricasti%20IRs%20version%202023-10.zip) (964 MB). If that link stops working, it's the **Bricasti M7 impulse response archive** at the very bottom of [Samplicity's downloads page](https://www.samplicity.com/downloads/). Unzip it and move the whole folder into:

```
~/Music/Lucky Seven/IRs
```

The folder is created the first time the plugin opens. To add more folders, open the **⋯** menu at the top right and choose **IR folders → Add a folder...**. After adding files, choose **IR folders → Rescan all folders** or reopen the plugin window.

The Samplicity IRs are free and offered as-is. They're downloaded from Samplicity rather than bundled, so please don't redistribute them with this plugin or any other product.

## Building

Requirements: macOS 13 or newer, Xcode (with the command line tools selected) and CMake 3.25 or newer. Ninja is optional but faster. The build is universal (Apple Silicon and Intel).

```sh
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

JUCE is fetched automatically by CMake. The build copies the VST3 and AU into your plug-in folders. A standalone app is built alongside them for quick testing.

Pushing a tag such as `v1.2.0` builds, tests and publishes the installer (`Lucky-Seven.pkg`) and a zip through GitHub Actions.

### Tests and tools

```sh
ctest --test-dir build                                    # unit tests
LUCKY_SEVEN_IR_ROOT="$HOME/Music/Lucky Seven/IRs" \
  build/LuckySevenTests_artefacts/Release/LuckySevenTests  # also checks the real Bricasti library
build/LuckySevenBench_artefacts/Release/LuckySevenBench <ir.wav> [decay ratio] [block size] [host rate]
build/LuckySevenProbe_artefacts/Release/LuckySevenProbe <plugin.vst3> <parameter name>
build/LuckySevenProbe_artefacts/Release/LuckySevenProbe <plugin.vst3> --timing [block size]
build/LuckySevenSnapshot <out.png> [list|active|timing|search:<text>|mapsearch:<text>|hover:<id>|150%]
build/LuckySevenSnapshot --check-state                        # A/B and save/restore check
build/LuckySevenSnapshot --check-keys                         # keyboard navigation check
```

`LuckySevenBench` measures CPU cost per instance. `LuckySevenProbe` hosts any VST3 and measures how its mix knob sets dry and wet levels, or with `--timing` its per-block CPU cost.

## Licence

Lucky Seven is released under the [GNU Affero General Public License v3.0](LICENSE). It is built with [JUCE](https://juce.com), which is used here under the AGPLv3.

Lucky Seven is an independent project and is not affiliated with Bricasti Design or Samplicity.
