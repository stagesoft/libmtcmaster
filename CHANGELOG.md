<!--
SPDX-FileCopyrightText: 2026 Stagelab Coop SCCL
SPDX-License-Identifier: GPL-3.0-or-later
-->

# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [0.1] — 2026-05-31

Initial public development release of `libmtcmaster`. This entry consolidates all commits
since the repository was created.

### Added

- **`MtcMaster` C++17 class** (`MtcMaster_class.h`, `MtcMaster_class.cpp`) — core shared
  library providing a full MIDI Time Code master generator. Inherits `RtMidiOut` (RtMidi).
  Supports 24, 25, 29, and 30 fps via the `FrameRate` enum. Sends MTC quarter-frame messages
  and Full Frame SysEx from a real-time `SCHED_RR` pthread at priority 99.
- **Transport controls** — `play()`, `pause()` (alias for `play()`), and `stop()`. `play()`
  acts as a toggle: starts a detached real-time thread when stopped, or clears the `playing`
  flag and spin-waits on the control mutex when running.
- **Position controls** — `setTime(nanos)` (absolute seek with pause/seek/resume and Full
  Frame SysEx broadcast), `subtractNanos(diff)`, `addNanos(diff)` (relative seek, wraps at
  24 h). All three follow the same pause-seek-sendFullFrame-resume contract.
- **Diagnostic helpers** — `mtcTimeVectorString()` (`"HHh:MMm:SSs:FFf"` format),
  `getApiString()` (human-readable RtMidi back-end name), `getTime()` / `getMtcTime()`
  (current nanosecond position).
- **Run-time logging** — append-mode log at `/run/log/libmtcmaster.log` with timestamped
  "Initialized" / "finished" entries and per-quarter "TIME OUT!!!" warnings when the real-time
  loop falls behind schedule.
- **C FFI layer** (`interface.h`, `interface.cpp`) — `extern "C"` opaque-pointer bindings:
  `MTCSender_create`, `MTCSender_release`, `MTCSender_openPort`, `MTCSender_play`,
  `MTCSender_stop`, `MTCSender_pause`, `MTCSender_setTime`. Added `openPort` in response to
  RtMidi back-end requirements (see commit `fbd84d5`).
- **Python bindings** (`python/mtcsender.py`) — `ctypes`-based `MtcSender` class wrapping
  the C FFI. Provides `play`, `pause`, `stop`, `settime(seconds)`, `settime_frames(frames)`,
  `settime_nanos(nanos)`. Loads `libmtcmaster.so` via a relative path from the `python/`
  working directory.
- **Python integration test** (`python/mtcsender_test.py`) — exercises the full transport
  and seek cycle.
- **Interactive test application** (`test_app/`) — terminal-based C++ application with
  keyboard-driven transport and seek commands; uses ANSI escape codes via `ScreenMan`.
- **`Makefile`** — builds `libmtcmaster.so.0.1` with SONAME symlinks (`libmtcmaster.so.0`,
  `libmtcmaster.so`). Supports `prefix`-based installation (`make install`) for Debian
  packaging. Compiler flags: `-Wall -Werror -Wextra -std=c++17 -O3 -fPIC -g`; linker flags
  include `-fsanitize=address` by default.
- **`LICENSE`** — GNU General Public License v3.0.

### Changed

- **`MTCSender_openPort` added to C FFI** — `interface.h` and `interface.cpp` updated to
  expose `RtMidiOut::openPort` as `MTCSender_openPort`; `python/mtcsender.py` updated to call
  it after `MTCSender_create`. Root cause: RtMidi now requires an explicit `openPort` call
  before `sendMessage` on some back-ends (commit `fbd84d5`).
- **ALSA MIDI back-end wired in** — `MtcMaster_class.cpp` updated to use `RtMidi::LINUX_ALSA`
  explicitly; minor internal cleanups (commit `e7ff9db`).
- **Makefile install target** — switched to `install -m 644` and `$(prefix)` variable for
  Debian-compatible package generation (commit `23932c7`).
- **Makefile symlink creation** — corrected the SONAME symlink creation order to produce valid
  `ldconfig`-compatible chains (commit `2567e46`).
- **`make clean`** — extended to remove all binary artefacts, including the versioned shared
  object (commit `2ba356a`).

### Removed

- **Ruby FFI files** — experimental Ruby bindings removed from the repository as they were
  superseded by the Python `ctypes` wrapper (commit `f55b816`).

### Notes

- The `fps` constructor argument of `MtcSender` (Python) is stored locally and used for
  `settime_frames` conversion but is **not forwarded** to the C++ library. The C++ library
  always runs at FR_25 (25 fps) unless `setFrameRate` is called directly. This is documented
  as a known limitation and is a candidate for a future Tier 2 contribution.
- The library always opens ALSA port 0 as `"MTCPort"` in the `MtcMaster` constructor. The
  C FFI `MTCSender_openPort` call then opens an additional or replacement port on the same
  `RtMidiOut` object. Callers using the C FFI directly should be aware of this double-open
  behaviour.
- `playing` and `mtx` are `static` class members, shared across all `MtcMaster` instances.
  Only one instance should be active at a time.
- The log directory `/run/log/` must exist before the first `MtcMaster` construction;
  failure to open the log file causes `exit(EXIT_FAILURE)`.

[0.1]: https://github.com/stagesoft/libmtcmaster/releases/tag/v0.1
