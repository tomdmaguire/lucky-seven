# Lessons

## "X-style" UI requests: get the reference before building
- **Mistake:** asked for an "Ableton style EQ" with one screenshot, I built a band dot that moved both corners at once with 24 dB/oct slopes. Ableton's actual model has Lo Cut / Hi Cut toggles, a handle that sits on the curve, separate frequency and width readouts, and gentle slopes.
- **Rule:** when a request names another product's UI, list the specific interactions I'm assuming (controls, what dragging does, readouts) and confirm them, or ask for screenshots of each state, before writing code.

## Async-loading test fixtures
- **Mistake:** waited for `Convolution::getCurrentIRSize() > 0`, but JUCE reports 1 (an identity impulse) before the real IR is installed, so the test ran against no IR.
- **Rule:** when waiting on async state, wait for the exact expected value (e.g. the IR length), not "non-zero".
