# Lucky Seven: project overview

JUCE 9 (CMake + Ninja, fetched via FetchContent) VST3 + Standalone convolution reverb for Apple Silicon, built around the Samplicity Bricasti M7 quad-channel IRs. Licensed AGPLv3.

## Layout
- `Source/PluginProcessor.*`: parameters (predelay, decay %, lowCut, highCut, mix %, blend dB, outputMode Mix/Blend/Send), state (selected IR id, browser view), mix laws (Mix: wet = m, dry = sqrt(1 - m^2); Blend: dry 1, wet dB; Send: wet only). Default program "Chambers/17 Sunset Chamber".
- `Source/PluginEditor.*`: top bar (Segment 7 logo, program pill with prev/next + favourite heart, MAP/LIST tabs, ⋯ menu), browser, bottom strip of five knobs and the Mix/Blend/Send buttons. Controls take the current category colour.
- `Source/dsp/ReverbEngine.*`: predelay, then two NonUniform convolvers (head 4096: from-left and from-right IRs, summed for true stereo), 12 dB/oct SVF low/high cut on the wet signal, smoothed dry/wet gains. Sleeps once the input has been silent for longer than the tail.
- `Source/dsp/IrLoader.*`: background thread. Reads the IR, caps it at 30 s, normalises energy, trims below -96 dB, analyses it, and on file or decay changes (80 ms debounce) runs `shortenDecay` and installs the result.
- `Source/dsp/DecayShaper.*`: STFT (2048/512, Hann) per-bin exponential envelope scaling each frequency's T60 by the decay ratio. Shortening only.
- `Source/ir/IrAnalysis.*`: per-octave T60 (Schroeder, T30 with T20 fallback), mid T60, brightness (energy-weighted log frequency after 50 ms), onset.
- `Source/ir/IrLibrary.*`: scanning. Samplicity quad names map to categories 1-6 and merge 44.1/48 kHz variants; L/R split files are skipped. Other WAV/AIFF/FLAC files get their category from the first subfolder (index >= 100, "User" for loose files). Quad channel order is LL, LR, RL, RR.
- `Source/ir/IrCatalogue.*`: SharedResourcePointer shared by all instances. Holds the scan, background analysis with a JSON cache (`~/Library/Application Support/Lucky Seven/analysis.json`), global settings (IR root, favourites) in `Lucky Seven.settings`. The default root is `~/Music/Lucky Seven/IRs`, created automatically.
- `Source/ui/`: `SoundMapView` (rank-blended log axes, relaxation layout, legend filters incl. favourites), `ListBrowserView` (search, All results, Favourites, heart column), `BrowserPanel`, `EmptyStateView` (first run), `HeartButton`, `LookAndFeel`, `Theme` (pastel category palette: equal saturation and brightness, hues spread round the wheel).
- `Tests/`: juce::UnitTest via CTest. `RealLibraryTests` run only when `LUCKY_SEVEN_IR_ROOT` is set.
- `tools/Benchmark.cpp` (CPU per instance), `tools/PluginProbe.cpp` (measure any plugin's mix law).

## Notes
- The IRs are git-ignored (`IRs/`). On Tom's machine `~/Music/Lucky Seven/IRs` is a symlink to the repo's `IRs/`.
- Decay lengthening was considered and deferred: the library already spans 0.26-8.6 s, and a generated tail would dilute the Bricasti character. If revisited, prefer per-band envelope plus a synthesised noise tail matched to the measured late decay.
- JUCE gotchas: `getCurrentIRSize()` reports 1 before the real IR loads; SliderAttachment replaces `textFromValueFunction`, so set it after attaching.
