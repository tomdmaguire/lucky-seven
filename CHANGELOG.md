# Changelog

## Unreleased

- Lucky Seven now appears under Lucky Labs in your DAW's plug-in list. Rescan plug-ins after updating. Existing sessions still load it.
- High Cut is now Damping. Instead of filtering the whole reverb evenly, it makes high frequencies die away faster, so the start of the reverb darkens a little and the tail a lot. It goes down to 250 Hz for really dark reverbs. Saved sessions, including A/B slots, keep their setting, which now sets the damping frequency.
- While you turn Decay or Damping, the part of the knob's arc you haven't heard yet shows in grey; the sound updates as soon as you pause, without letting go of the mouse.
- The arrow keys always browse the map or list on screen: no need to click it first, even after switching views or while turning a knob.
- Switching programs is around three times faster.
- Tab switches between the map and the list.
- In the list view, the column you're navigating shows a full highlight and the other is dimmed, so ←/→ visibly move between categories and programs.
- Arrow keys on the map are now the whole story: ←/→ for shorter or longer, ↑/↓ for brighter or darker, with a hint in the search bar (the arrows browse the list view too). Shift+←/→ is gone; the plain arrows already keep a similar tone.

## v1.2.0

- New Width knob: narrows the reverb from its natural stereo image down to mono, leaving the dry signal untouched.
- Simpler wording on the screen shown when no IRs are found.

## v1.1.2

- Much smoother CPU use at small buffer sizes: the long reverb tail is now computed on a background thread, so no single audio block carries a burst of work. At 48 kHz with a 64-sample buffer the heaviest blocks drop from over 100% of the available time to around 5%, whatever the IR length. The sound is unchanged and no latency is added.
- Hovering a dot on the map shows its details instantly, with the keyboard hint set apart.
- The map uses more of the window, with less empty space above and below it.
- Click the Lucky Seven logo to jump back to the map.
- Consistent cursors: the pointing hand now only appears over map dots, legend entries, the logo and the program name.

(v1.1.1 was tagged but never released.)

## v1.1.0

- Several IR folders: add, remove and reveal them from the ··· menu under IR folders. The folder picker can now open macOS packages such as Ableton's Factory Packs.
- Works with Ableton's Convolution Reverb pack: left/right file pairs play as one true-stereo reverb, numbered folder names are tidied ("05 Halls" becomes "Halls"), and categories with the same name merge across folders.
- More accurate decay times for IRs recorded with a hiss floor or a leading click; hiss tails are trimmed on load.
- Search now works on the map as well as the list, together with the category filters, and the legend shows match counts.
- The map legend wraps onto more rows when there are many categories.

## v1.0.1

- New installer: download Lucky-Seven.pkg, open it, and choose VST3 and/or AU. No Terminal step needed.
- One-click IR download: on first launch, click Download Bricasti IRs to fetch Samplicity's free Bricasti M7 archive and set it up automatically, with progress and cancel. Also in the ··· menu.
- Fix: the window no longer reopens at 75% of the size you left it at.
- Clearer install instructions, linking the exact Samplicity archive.

## v1.0.0

First release.

- True-stereo convolution reverb for the Samplicity Bricasti M7 impulse responses, plus any WAV, AIFF or FLAC IRs you add (each subfolder becomes a category).
- Sound map: every program placed by decay time and brightness, with category and favourites filters, ripples and a glow that follow the audio, and arrow-key navigation (Shift+←/→ jumps to the nearest shorter or longer program with a similar tone).
- List view with search across categories, favourites and A–Z browsing.
- Decay control that shortens any program while keeping its frequency-dependent character.
- Pre-delay, Low Cut (20 Hz–1 kHz) and High Cut (1–20 kHz) on the reverb only.
- Mix, Blend (dry fixed at 0 dB, reverb level in dB) and Send (100% wet) output modes.
- A/B comparison of two complete settings.
- Resizable window from 75% to 200%.
- VST3 and AU, Apple Silicon and Intel.
