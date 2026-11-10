# Lessons

## "X-style" UI requests: get the reference before building
- **Mistake:** asked for an "Ableton style EQ" with one screenshot, I built a band dot that moved both corners at once with 24 dB/oct slopes. Ableton's actual model has Lo Cut / Hi Cut toggles, a handle that sits on the curve, separate frequency and width readouts, and gentle slopes.
- **Rule:** when a request names another product's UI, list the specific interactions I'm assuming (controls, what dragging does, readouts) and confirm them, or ask for screenshots of each state, before writing code.

## Async-loading test fixtures
- **Mistake:** waited for `Convolution::getCurrentIRSize() > 0`, but JUCE reports 1 (an identity impulse) before the real IR is installed, so the test ran against no IR.
- **Rule:** when waiting on async state, wait for the exact expected value (e.g. the IR length), not "non-zero".

## Plans must account for every earlier suggestion
- **Mistake:** I suggested softening the *high* cut, then High Cut became Damping and the plan quietly dropped that idea. The user remembered "softening the low cut" and couldn't find it in the plan, and my replies didn't clear up which control I'd meant.
- **Rule:** when a plan supersedes something I proposed earlier, say so in the plan by name ("dropped: 6 dB/oct high cut, replaced by Damping"). When the user misremembers a suggestion, first restate exactly what I said and about which control, then answer.

## Don't disturb a live DAW session
- **Mistake:** full-core builds starved the plugin's IR loader thread in the user's running Ableton set, so Damping seemed broken; I also nearly overwrote the installed plugin while Live had it loaded.
- **Rule:** while the user is testing in a DAW, build with `nice -n 19` and limited `-j`, build only test/tool targets, and install (full build) only after they have quit the host.

## JUCE Convolution swaps restart history
- **Mistake:** rebuilding the IR continuously while a knob moved made the early reverb pulse (tremolo): each `loadImpulseResponse` creates a fresh engine with no input history and crossfades to it.
- **Rule:** never swap JUCE Convolution IRs at a high rate; settle-debounce instead, or use a convolver that keeps its history across swaps (like `TailConvolver`).
