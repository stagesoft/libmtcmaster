<!--
***
SPDX-FileCopyrightText: 2026 Stagelab Coop SCCL
SPDX-License-Identifier: GPL-3.0-or-later
***
-->

# libmtcmaster

**Current release: v0.1** — see [CHANGELOG.md](./CHANGELOG.md).

**C++17 shared library that generates MIDI Time Code over ALSA, with a C FFI layer and Python bindings.**

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)

* **Source / issues:** [stagesoft/libmtcmaster](https://github.com/stagesoft/libmtcmaster) on GitHub

`libmtcmaster` is a C++17 shared library developed for the **CueMS** (Cue Management System)
family. It drives a MIDI Time Code master clock over ALSA, managing a real-time
`SCHED_RR`-scheduled thread that emits MTC quarter-frame messages at the correct sub-millisecond
interval for frame rates of 24, 25, 29, and 30 fps. The library exposes its full control surface
via a thin C FFI layer (`interface.h`) that makes it embeddable from any language with a C
foreign-function interface, and ships a ready-to-use Python `ctypes` wrapper.

It is composed of:

* **`MtcMaster_class`** — C++17 class that inherits `RtMidiOut` and owns the playback thread,
  timing loop, and time-code arithmetic.
* **`interface`** — `extern "C"` binding layer that exposes constructor, destructor, and all
  control functions as opaque-pointer C calls.
* **`python/`** — Python `ctypes` wrapper (`MtcSender`) that loads `libmtcmaster.so` at runtime
  and maps the C FFI to a Pythonic API.
* **`test_app/`** — Standalone interactive terminal application that exercises the C++ API
  directly and serves as an integration reference.

---

## Table of Contents

- [Overview](#overview)
- [Architecture](#architecture)
  - [MtcMaster\_class — Core Library Module](#mtcmaster_class--core-library-module)
  - [interface — C FFI Layer](#interface--c-ffi-layer)
  - [python/ — Python Bindings](#python--python-bindings)
  - [test\_app/ — Interactive Test Application](#test_app--interactive-test-application)
  - [Threading and Process Model](#threading-and-process-model)
  - [Key Data Structures](#key-data-structures)
- [Core Concepts](#core-concepts)
- [Design Goals](#design-goals)
- [API Documentation](#api-documentation)
  - [C++ Class API](#c-class-api)
  - [C FFI API](#c-ffi-api)
  - [Python Bindings API](#python-bindings-api)
- [Installation](#installation)
  - [Build from Source](#build-from-source)
  - [Debian Package](#debian-package)
- [Usage](#usage)
  - [C++ Usage](#c-usage)
  - [Python Usage](#python-usage)
- [Development](#development)
- [Contributors](#contributors)
- [Release Notes](#release-notes)
- [Future Developments](#future-developments)
  - [Automated Test Suite and Coverage](#automated-test-suite-and-coverage)
  - [CI/CD Workflow](#cicd-workflow)
  - [Documentation Site](#documentation-site)
  - [Packaging and Debian Integration](#packaging-and-debian-integration)
  - [Target Badge Set](#target-badge-set)
- [Copyright Notice](#copyright-notice)
- [License](#license)

---

[↑ Back to Table of Contents](#table-of-contents)

## Overview

`libmtcmaster` models the MTC playback pipeline as a single-threaded loop running under
real-time Linux scheduling. The diagram below shows the complete data and control path from
caller to ALSA MIDI output:

```text
Caller (C++ / C FFI / Python ctypes)
          │
          │  play() / pause() / stop() / setTime()
          ▼
    ┌──────────────────────────────────────────────────────┐
    │                   MtcMaster                          │
    │                                                      │
    │  setTime() ──► fillMtcTimeVector() ──► sendMtcPosition()
    │                       (Full Frame SysEx, 10 bytes)   │
    │                                                      │
    │  play() ──► spawn thread [SCHED_RR, priority 99]     │
    │                │                                     │
    │         ┌──────▼──────────────────────────────┐      │
    │         │      threadedMethod()               │      │
    │         │  sendMtcPosition() [initial seek]   │      │
    │         │  loop while playing:                │      │
    │         │    for byteType = 0 .. 7:           │      │
    │         │      build F1 <nibble> message      │      │
    │         │      RtMidiOut::sendMessage()       │      │
    │         │      nanosleep(frameFreq / 4)       │      │
    │         │    fillMtcTimeVector(mtcTime)       │      │
    │         └─────────────────────────────────────┘      │
    │                                                      │
    │  stop()  ──► setPlaying(false) + wait on mtx         │
    └──────────────────────────────────────────────────────┘
                          │
                          ▼
              RtMidiOut::sendMessage()
                          │
                          ▼
              ALSA MIDI Out port ("MTCPort", port 0)
                          │
                          ▼
              MIDI receiver (hardware / software)
```

**Layer responsibilities:**

* **Caller layer** — constructs an `MtcMaster` (or uses the C FFI / Python wrapper), drives
  transport commands, and queries or adjusts the current timecode position.
* **MtcMaster** — owns the playback state machine, timecode arithmetic, the real-time thread,
  the mutex that synchronises caller and thread, and the ALSA `RtMidiOut` connection.
* **threadedMethod** — the real-time playback loop. Sends one 10-byte Full Frame SysEx at the
  start of each play/seek operation, then cycles through 8 quarter-frame messages per two
  video frames, sleeping between each using `nanosleep` for sub-millisecond accuracy.
* **ALSA MIDI out** — the kernel MIDI driver delivers the byte stream to whatever receiver has
  subscribed to the port (hardware sequencer, virtual MIDI cable, DAW, etc.).

---

[↑ Back to Table of Contents](#table-of-contents)

## Architecture

### MtcMaster\_class — Core Library Module

**Files:** `MtcMaster_class.h`, `MtcMaster_class.cpp`

`MtcMaster` is the sole public C++ class and the heart of the library. It inherits from
`RtMidiOut`, so every `RtMidi` output method (`openPort`, `closePort`, `sendMessage`, …) is
available alongside the transport controls defined here. The constructor selects an ALSA MIDI
API, opens port 0 under the name `"MTCPort"`, sets the frame-rate bits, and opens the log
file at `/run/log/libmtcmaster.log`.

Public members and methods derived strictly from the header and implementation:

| Symbol | Kind | Role |
|---|---|---|
| `MtcMaster(api, clientName, setUpFR)` | constructor | Opens ALSA port 0, initialises frame-rate bits, opens the log file. |
| `~MtcMaster()` | destructor | Closes the log file with a timestamped "finished" entry. |
| `play()` | method | Toggles playback: if stopped, spawns the real-time thread; if playing, sets the `playing` flag to false and spin-waits for the thread to release `mtx`. |
| `pause()` | method | Alias for `play()` — acts as a toggle, stopping or starting the thread identically. |
| `stop()` | method | Halts the thread (clears `playing`, spin-waits on `mtx`), then resets `mtcTime` to 0 via `setTime(0)`. |
| `setTime(nanos)` | method | Seeks to an absolute nanosecond position: pauses if playing, stores the new time, calls `fillMtcTimeVector`, sends a Full Frame SysEx, then resumes if it was playing. |
| `getTime()` | method | Returns `mtcTime` in nanoseconds. |
| `sendMtcPosition()` | method | Constructs and sends a 10-byte MIDI Full Frame SysEx (`F0 7F 7F 01 01 HH MM SS FF F7`) encoding the current `mtcTimeVector` and frame-rate bits. |
| `fillMtcTimeVector(nanos)` | method | Converts a nanosecond offset into `[frames, seconds, minutes, hours]` components and stores them in `mtcTimeVector`. |
| `mtcTimeVectorString()` | method | Returns the current position as the human-readable string `"HHh:MMm:SSs:FFf"`. |
| `getApiString()` | method | Returns a human-readable MIDI API name (`"Linux ALSA"`, `"MacOSX Core"`, `"Unix Jack"`, etc.) from `RtMidi::getCurrentApi()`. |
| `getPlaying()` | inline | Returns the current value of the static `playing` flag. |
| `setPlaying(bool)` | inline | Writes the static `playing` flag; used internally to signal the thread. |
| `getFrameRate()` | inline | Returns the current `FrameRate` enum value. |
| `setFrameRate(FrameRate)` | inline | Overwrites `currentFrameRate`; does not automatically update `currentFRBits` — call before constructing a new session. |
| `getMtcTime()` | inline | Returns `mtcTime` directly (duplicate of `getTime()`; provided for convenience). |
| `subtractNanos(diff)` | method | Decrements `mtcTime` by `diff` (clamps to 0), then calls `setTime(mtcTime)` — triggers pause/seek/resume. |
| `addNanos(diff)` | method | Increments `mtcTime` by `diff` (wraps to 0 at 24 h = 86 400 000 000 000 ns), then calls `setTime(mtcTime)`. |
| `instanceCount` | static member | Counts the number of currently live `MtcMaster` instances; incremented in the constructor, decremented in the destructor. |
| `mtcTimeVector` | public member | `vector<unsigned char>` of length 4: `[0]` = frames, `[1]` = seconds, `[2]` = minutes, `[3]` = hours. Updated by `fillMtcTimeVector`. |

Private implementation highlights:

* **`threadedMethod()`** — The real-time playback loop. Acquires `mtx`, emits a Full Frame
  SysEx, then enters the main `while (playing)` loop. Each iteration sends 8 quarter-frame
  messages (byte types 0–7), computing `nextQuarterToSend` as an accumulating timestamp and
  sleeping via `nanosleep` between messages. After the loop the method adjusts `mtcTime` by
  +2 frames to account for partial frame rounding, then releases `mtx`.
* **`setScheduling(thread, policy, priority)`** — Calls `pthread_setschedparam` on the thread's
  native handle to apply `SCHED_RR` at priority 99. Logs and throws `RtMidiError::THREAD_ERROR`
  on failure.
* **`logTime()`** — Writes the current wall-clock timestamp (`%F %T` format) to the log stream.

---

### interface — C FFI Layer

**Files:** `interface.h`, `interface.cpp`

Thin `extern "C"` wrapper that erases the C++ type system and exposes `MtcMaster` through an
opaque `void*` handle. This layer makes `libmtcmaster.so` loadable from any language with a C
FFI (Python `ctypes`, Ruby `ffi`, Julia `ccall`, etc.).

| Symbol | Role |
|---|---|
| `MTCSender_create()` | Heap-allocates a default `MtcMaster()` and returns it as `void*`. |
| `MTCSender_release(void*)` | Deletes the `MtcMaster` via `static_cast<MtcMaster*>`. |
| `MTCSender_openPort(void*, unsigned int, const char*)` | Calls `RtMidiOut::openPort(n, name)` on the wrapped instance. |
| `MTCSender_play(void*)` | Delegates to `MtcMaster::play()`. |
| `MTCSender_stop(void*)` | Delegates to `MtcMaster::stop()`. |
| `MTCSender_pause(void*)` | Delegates to `MtcMaster::pause()`. |
| `MTCSender_setTime(void*, uint64_t)` | Delegates to `MtcMaster::setTime(nanos)`. |

> **Note:** `MTCSender_create()` internally calls `MtcMaster()` which already opens port 0 as
> `"MTCPort"` in the constructor. Calling `MTCSender_openPort` immediately afterwards (as the
> Python wrapper does) opens an additional port on the same `RtMidiOut` object. Callers relying
> on the C FFI should be aware that port 0 is opened unconditionally on construction.

---

### python/ — Python Bindings

**Files:** `python/mtcsender.py`, `python/mtcsender_test.py`

`MtcSender` is a `ctypes`-based Python class that loads `libmtcmaster.so` from a path relative
to the repository root and delegates every call to the C FFI layer.

| Symbol | Role |
|---|---|
| `MtcSender(fps, port, portname)` | Loads `libmtcmaster.so`, calls `MTCSender_create()`, then `MTCSender_openPort(port, portname)`. `fps` is stored but **not yet forwarded to the library** (see Known Limitations). |
| `play()` | Calls `MTCSender_play`. |
| `pause()` | Calls `MTCSender_pause`. |
| `stop()` | Calls `MTCSender_stop`. |
| `settime(seconds)` | Converts seconds → nanoseconds and calls `MTCSender_setTime`. |
| `settime_frames(frames)` | Converts frame count → nanoseconds using the stored `fps` and calls `MTCSender_setTime`. |
| `settime_nanos(nanos)` | Calls `MTCSender_setTime` directly. |
| `__del__` | Calls `MTCSender_release` to free the native object. |

**Known limitations:**
- The `fps` constructor argument is stored as `self.fps` and used only for `settime_frames`
  conversion. It is **not** passed to `MtcMaster` — the C++ library always runs at FR_25
  (25 fps) regardless of the value provided in Python.
- The library path is resolved as `pathlib.Path.cwd().parent.parent / "libmtcmaster/libmtcmaster.so"`,
  which assumes the working directory is `python/` at the time of import.

`python/mtcsender_test.py` is an integration script that exercises play, pause, `settime`,
`settime_frames`, `settime_nanos`, stop, and object destruction in sequence.

---

### test\_app/ — Interactive Test Application

**Files:** `test_app/mtcmaster_test_app.cpp`, `test_app/mtcmaster_test_app.h`,
`test_app/ScreenMan_class.cpp`, `test_app/ScreenMan_class.h`, `test_app/Makefile`

A self-contained terminal application that links directly against the uninstalled
`MtcMaster_class` objects. It uses ANSI escape codes (via `ScreenMan`) to display the current
MTC position and accepts single-key commands for transport control and seek.

| Symbol | Role |
|---|---|
| `main()` | Initialises `MtcMaster`, enters a keyboard-driven event loop, dispatches `play`, `pause`, `stop`, and seek commands. |
| `ScreenMan` | Minimal terminal helper using ANSI escape codes: `clrScr()`, `mvCursor(row, col)`, `printAt(row, col, str)`. |

Key bindings exercised by the test app:

| Key | Action |
|---|---|
| `P` / `p` | Toggle play / pause |
| `S` / `s` | Stop (reset to 00h:00m:00s:00f) |
| `1`–`9` | Add *n* × 60 s to the current position |
| `-` | Subtract 60 s from the current position |
| Any other key | Print current MTC position |
| `Esc` | Exit |

---

### Threading and Process Model

`libmtcmaster` is single-process and uses one auxiliary thread per playback session:

```text
Main thread (caller)
  │
  │  play() ──────────────────────────────────────────────────────────┐
  │                                                                   │
  │                                          threadedMethod [detached]│
  │                                          ┌───────────────────────┐│
  │                                          │  mtx.lock()           ││
  │                                          │  sendMtcPosition()    ││
  │  stop() / play() (pause) ◄─── spin on   │  while (playing):     ││
  │  while(!mtx.try_lock())       mtx ──────►│    8 × F1 <nibble>   ││
  │  mtx.unlock()                            │    nanosleep(q/4)     ││
  │                                          │  adjust mtcTime +2f   ││
  │                                          │  mtx.unlock()         ││
  │                                          └───────────────────────┘│
  └───────────────────────────────────────────────────────────────────┘
```

* The playback thread is spawned with `std::thread(&MtcMaster::threadedMethod, this)` and
  immediately detached after its scheduling policy is set.
* `pthread_setschedparam` applies `SCHED_RR` at priority 99, the highest real-time round-robin
  priority available on Linux without a kernel patch.
* The `playing` flag and the `mtx` mutex are both `static` class members, so they are shared
  across all instances. Only one instance should be active at a time.
* The caller signals stop/pause by clearing `playing` and then spin-waits via
  `while (!mtx.try_lock())` until the thread releases `mtx` at the end of its loop — this is a
  deliberate busy-wait to avoid scheduler latency in the synchronisation path.
* Log writes (`logFileStream`) happen in the main thread (constructor/destructor) and in the
  real-time thread (timeout warnings) — the log stream is not mutex-protected; writes are
  coarse-grained and the overlap window is narrow in practice.

---

### Key Data Structures

| Name | Type | Description |
|---|---|---|
| `mtcTimeVector` | `vector<unsigned char>` | Four-element vector `[frames, seconds, minutes, hours]` derived from `mtcTime`. Used to build both quarter-frame nibbles and the Full Frame SysEx. |
| `mtcTime` | `uint64_t` | Master timecode position in nanoseconds. Continuously incremented by `frameFreq / 4` in the playback loop. |
| `currentFrameRate` | `FrameRate` | Active frame rate enum; determines the nanosecond-per-frame divisor (`1e9 / currentFrameRate`). |
| `currentFRBits` | `unsigned char` | SMPTE type bits (`0x00`/`0x20`/`0x40`/`0x60`) ORed into the hours byte of MTC messages. |
| `playing` | `static bool` | Global playback flag. The thread loops while this is true; setting it false stops the loop. |
| `mtx` | `static std::mutex` | Synchronisation mutex: held by the thread during playback, used by the caller as a completion signal. |
| `instanceCount` | `static unsigned short` | Reference counter for live `MtcMaster` objects. |
| `logFileStream` | `std::ofstream` | Append-mode log file at `/run/log/libmtcmaster.log`. |

**`FrameRate` enum:**

```cpp
enum FrameRate { FR_24 = 24, FR_25 = 25, FR_29 = 29, FR_30 = 30 };
```

**`MidiStatusCodes` enum** (selected values relevant to MTC):

| Constant | Value | Role |
|---|---|---|
| `MIDI_STATUS_TIME_CODE` | `0xF1` | First byte of every MTC quarter-frame message. |
| `MIDI_STATUS_SYSEX` | `0xF0` | Start of Full Frame SysEx. |
| `MIDI_STATUS_SYSEX_END` | `0xF7` | End of Full Frame SysEx. |

---

[↑ Back to Table of Contents](#table-of-contents)

## Core Concepts

* **MIDI Time Code (MTC)** — A MIDI-wire protocol that encodes SMPTE timecode as a stream of
  two-byte quarter-frame messages. Eight consecutive quarter frames together carry one full
  `HH:MM:SS:FF` position, so a full update takes two video frames.
* **Quarter-frame message** — A two-byte MIDI message (`0xF1 <data>`) where the data byte
  encodes a 3-bit message type (0–7, selecting which nibble of the timecode to send) and a
  4-bit data nibble. Eight of these messages fully encode `hours + frame-rate bits`, `minutes`,
  `seconds`, and `frames`.
* **Full Frame SysEx** — A 10-byte Real-Time Universal System Exclusive message
  (`F0 7F 7F 01 01 HH MM SS FF F7`) that delivers the complete timecode position in a single
  packet. Sent once at the start of playback and after every seek so receivers can lock
  immediately without waiting for eight quarter-frame cycles.
* **Frame rate** — MTC supports 24, 25, 29.97 (encoded as 29), and 30 fps. The frame rate
  determines the SMPTE type bits and the nanosecond-per-frame divisor used in the timing loop.
  In `libmtcmaster` this is set at construction time via the `FrameRate` enum.
* **`nanosTime` / `mtcTime`** — All internal time tracking uses nanoseconds as the base unit,
  converted to `[frames, seconds, minutes, hours]` only when building MIDI messages.
* **SCHED_RR scheduling** — The playback thread runs under Linux's Round-Robin real-time
  scheduler at priority 99. This minimises jitter from OS scheduling interference and is
  essential for maintaining MTC accuracy at the sub-millisecond level required by professional
  equipment.
* **Opaque-pointer FFI** — The C binding layer wraps `MtcMaster*` as `void*`, allowing any C-
  compatible FFI runtime to control the library without exposing C++ ABI or vtable details.

---

[↑ Back to Table of Contents](#table-of-contents)

## Design Goals

* **Real-time timing accuracy** — The playback thread runs at `SCHED_RR` priority 99 and uses
  `nanosleep` for sub-millisecond sleep precision. The loop logs "TIME OUT!!!" events to the log
  file whenever it falls behind schedule, making timing regressions observable.
* **Language-agnostic embedding** — The `extern "C"` interface layer deliberately erases C++
  ABI details, making the shared object loadable from Python, Ruby, Julia, or any other language
  with a C FFI, without requiring bindings generators or C++ knowledge on the caller side.
* **Minimal runtime footprint** — The library has one external runtime dependency (`librtmidi`);
  all timing is handled with POSIX APIs (`nanosleep`, `pthread_setschedparam`). No event loop,
  no heap allocation in the hot path.
* **Single-class, single-responsibility design** — The entire MTC generation logic is
  encapsulated in `MtcMaster`. There is no separate scheduler, no plugin system, and no
  configuration file — construction arguments and method calls are the complete interface.
* **Transparent seek semantics** — `setTime`, `subtractNanos`, and `addNanos` all follow the
  same contract: pause if playing, seek, send a Full Frame SysEx so receivers re-lock
  immediately, then resume. This prevents partial-frame artefacts on connected devices.
* **Portable MIDI back-end** — `MtcMaster` accepts the `RtMidi::Api` constructor argument,
  allowing callers to select ALSA, JACK, CoreMIDI, or the dummy back-end at runtime without
  recompiling.
* **Observable state** — `instanceCount`, `getPlaying()`, `getMtcTime()`, and
  `mtcTimeVectorString()` give callers unambiguous read access to every piece of runtime state
  without exposing internal storage directly.

---

[↑ Back to Table of Contents](#table-of-contents)

## API Documentation

### C++ Class API

Include `MtcMaster_class.h` and link against `libmtcmaster.so`.

#### Constructor

```cpp
MtcMaster(
    RtMidi::Api    api        = LINUX_ALSA,
    const string&  clientName = "MtcMaster",
    FrameRate      setUpFR    = FR_25
);
```

Opens ALSA port 0 as `"MTCPort"` and opens the log file `/run/log/libmtcmaster.log` in
append mode. Throws `RtMidiError` (and calls `exit(EXIT_FAILURE)`) if no MIDI port is
available, or `system_error` (and calls `exit(EXIT_FAILURE)`) if the log directory does not
exist.

**Parameters:**

| Parameter | Default | Description |
|---|---|---|
| `api` | `LINUX_ALSA` | RtMidi back-end selection. |
| `clientName` | `"MtcMaster"` | ALSA client name visible in `aconnect -l`. |
| `setUpFR` | `FR_25` | Initial frame rate (24, 25, 29, or 30 fps). |

#### Destructor

```cpp
~MtcMaster();
```

Writes a timestamped "MTC Master finished" entry and closes the log file. Does not
automatically stop a running playback thread — callers must call `stop()` before
destroying the object.

#### Transport control

```cpp
void play();
```
Starts playback if stopped (spawns the `SCHED_RR` thread). Pauses playback if already
running (sets `playing = false`, spin-waits on `mtx` until the thread exits the loop).
Acts as a toggle; `pause()` is an alias.

```cpp
void pause();
```
Alias for `play()`. Identical behaviour.

```cpp
void stop();
```
Halts the playback thread (same mechanism as `play()` when running) and then seeks to
`t = 0` via `setTime(0)`.

#### Position control

```cpp
void setTime(uint64_t nanos = 0);
```
Seeks to the absolute nanosecond position `nanos`. If playing, pauses first, seeks, sends
a Full Frame SysEx, then resumes. If not playing, seeks and sends the Full Frame SysEx
without starting the thread.

```cpp
uint64_t getTime(void);
```
Returns `mtcTime` (the current playback position in nanoseconds).

```cpp
uint64_t getMtcTime(void);   // inline
```
Equivalent to `getTime()`.

```cpp
void subtractNanos(const uint64_t diff);
```
Decrements position by `diff` nanoseconds (clamped to 0). Calls `setTime` internally,
so the Full Frame SysEx is sent and playback is resumed if it was running.

```cpp
void addNanos(const uint64_t diff);
```
Increments position by `diff` nanoseconds. Wraps to 0 if the result would exceed 24 hours
(86 400 000 000 000 ns). Calls `setTime` internally.

#### State accessors

```cpp
bool      getPlaying(void);           // inline
void      setPlaying(bool status);    // inline — use with care; prefer play()/stop()
FrameRate getFrameRate(void);         // inline
void      setFrameRate(FrameRate FR); // inline — takes effect on the next play() call
```

#### Diagnostic helpers

```cpp
string mtcTimeVectorString(void);
```
Returns the current position as `"HHh:MMm:SSs:FFf"` (zero-padded to two digits per field),
derived from the current `mtcTimeVector`.

```cpp
string getApiString(void);
```
Returns a human-readable name for the active `RtMidi::Api` (`"Linux ALSA"`, `"MacOSX Core"`,
`"Unix Jack"`, `"Windows MM"`, `"RtMidi Dummy"`, or `"Unspecified"`).

```cpp
void sendMtcPosition(void);
```
Sends a 10-byte MIDI Full Frame SysEx encoding the current `mtcTimeVector` and frame-rate
bits. Called automatically by `setTime`; can also be called directly to re-broadcast the
position without changing it.

```cpp
void fillMtcTimeVector(uint64_t nanos);
```
Populates `mtcTimeVector[0..3]` = `[frames, seconds, minutes, hours]` from `nanos`.

#### Public data members

```cpp
vector<unsigned char> mtcTimeVector; // [frames, seconds, minutes, hours]
static unsigned short instanceCount; // count of live MtcMaster objects
```

#### `FrameRate` enum

```cpp
enum FrameRate { FR_24 = 24, FR_25 = 25, FR_29 = 29, FR_30 = 30 };
```

---

### C FFI API

Declared in `interface.h`. All functions follow the opaque-pointer `void*` pattern. Link
against `libmtcmaster.so` and include `interface.h`.

```c
void*  MTCSender_create(void);
void   MTCSender_release(void* mtcsender);
void   MTCSender_openPort(void* mtcsender, unsigned int portnumber, const char* portname);
void   MTCSender_play(void* mtcsender);
void   MTCSender_stop(void* mtcsender);
void   MTCSender_pause(void* mtcsender);
void   MTCSender_setTime(void* mtcsender, uint64_t nanos);
```

| Function | Description |
|---|---|
| `MTCSender_create()` | Allocates a new `MtcMaster` with default arguments (ALSA, FR_25). Returns the opaque handle. |
| `MTCSender_release(h)` | Frees the `MtcMaster` object. Must be called exactly once per handle. |
| `MTCSender_openPort(h, n, name)` | Opens MIDI output port number `n` under the given `name`. Port 0 is already opened in the constructor; this call opens an additional or replacement port. |
| `MTCSender_play(h)` | Delegates to `MtcMaster::play()`. |
| `MTCSender_stop(h)` | Delegates to `MtcMaster::stop()`. |
| `MTCSender_pause(h)` | Delegates to `MtcMaster::pause()`. |
| `MTCSender_setTime(h, nanos)` | Seeks to `nanos` nanoseconds. |

---

### Python Bindings API

The Python wrapper lives in `python/mtcsender.py`. It must be run from a working directory
where `../../libmtcmaster/libmtcmaster.so` resolves to the built shared library (i.e., from
inside the `python/` subdirectory with the library already built in the repo root).

```python
from mtcsender import MtcSender

sender = MtcSender(fps=25, port=0, portname="SLMTCPort")
```

| Method | Signature | Description |
|---|---|---|
| `__init__` | `(fps=25, port=0, portname="SLMTCPort")` | Loads `libmtcmaster.so`, creates the native object, opens the specified port. `fps` is stored locally for `settime_frames`; it is **not** forwarded to the C++ library (library always uses FR_25). |
| `play` | `()` | Calls `MTCSender_play`. |
| `pause` | `()` | Calls `MTCSender_pause`. |
| `stop` | `()` | Calls `MTCSender_stop`. |
| `settime` | `(seconds: float)` | Converts seconds to nanoseconds, calls `MTCSender_setTime`. |
| `settime_frames` | `(frames: int/float)` | Converts frames → nanoseconds using `self.fps`, calls `MTCSender_setTime`. |
| `settime_nanos` | `(nanos: int)` | Calls `MTCSender_setTime` directly with nanoseconds. |
| `__del__` | — | Calls `MTCSender_release` on the native handle. |

---

[↑ Back to Table of Contents](#table-of-contents)

## Installation

### Build from Source

**System dependencies (Debian/Ubuntu):**

```bash
sudo apt-get install -y build-essential librtmidi-dev
```

Ensure `/run/log/` exists and is writable by the user running the library, as `MtcMaster`
writes its log there:

```bash
sudo mkdir -p /run/log
sudo chmod a+w /run/log
```

**Build and install:**

```bash
git clone https://github.com/stagesoft/libmtcmaster.git
cd libmtcmaster
make
sudo make install     # installs to /usr/local/lib/ by default
sudo ldconfig
```

The `prefix` variable can be overridden:

```bash
sudo make install prefix=/usr
```

**Build output:**

```
libmtcmaster.so.0.1    # versioned shared object
libmtcmaster.so.0      # soname symlink → libmtcmaster.so.0.1
libmtcmaster.so        # unversioned symlink → libmtcmaster.so.0.1
```

### Debian Package

A Debian packaging branch is in development. When available:

```bash
git clone --branch debian/bookworm https://github.com/stagesoft/libmtcmaster.git
cd libmtcmaster
dpkg-buildpackage -us -uc
sudo dpkg -i ../libmtcmaster_*.deb
```

---

[↑ Back to Table of Contents](#table-of-contents)

## Usage

### C++ Usage

```cpp
#include "MtcMaster_class.h"

int main()
{
    // Construct — opens ALSA port 0 as "MTCPort", selects 25 fps
    MtcMaster master(RtMidi::Api::LINUX_ALSA, "MyApp", FR_25);

    // Start playback from t = 0
    master.play();

    // Seek forward by 2 minutes (120 seconds = 120 * 1e9 ns)
    master.addNanos(static_cast<uint64_t>(120e9));

    // Jump to 01h:00m:00s:00f = 3600 * 1e9 ns
    master.setTime(static_cast<uint64_t>(3600e9));

    // Check position
    std::cout << master.mtcTimeVectorString() << std::endl;  // "01h:00m:00s:00f"

    // Pause
    master.play();

    // Stop and reset to 0
    master.stop();

    return 0;
}
```

Compile against the installed library:

```bash
g++ -std=c++17 -o my_app my_app.cpp -lmtcmaster
```

Or against the build directory before installation:

```bash
g++ -std=c++17 -I /path/to/libmtcmaster -L /path/to/libmtcmaster \
    -o my_app my_app.cpp -lmtcmaster -Wl,-rpath,/path/to/libmtcmaster
```

### Python Usage

```bash
# From inside the python/ directory, with the library already built:
cd python
python3 mtcsender_test.py
```

Or from your own script (adjusting the library path as needed):

```python
import sys, pathlib
sys.path.insert(0, str(pathlib.Path(__file__).parent / "python"))

from mtcsender import MtcSender
import time

sender = MtcSender(fps=25, port=0, portname="SLMTCPort")

sender.play()
time.sleep(5)

sender.settime(60.0)       # seek to 1 minute
time.sleep(2)

sender.settime_frames(750) # seek to frame 750 (30 s at 25 fps)
time.sleep(2)

sender.stop()
del sender                  # releases the native object
```

---

[↑ Back to Table of Contents](#table-of-contents)

## Development

**Requirements:**

```bash
sudo apt-get install -y build-essential librtmidi-dev clang-format clang-tidy
```

**Build (debug, with AddressSanitizer):**

The `Makefile` enables `-fsanitize=address` in the default `LDFLAGS`:

```bash
make
```

**Clean rebuild:**

```bash
make clean && make
```

**Build the test application:**

```bash
cd test_app
make
./mtcmaster_test_app
```

**Format check (all tracked C++ files):**

```bash
clang-format --dry-run --Werror $(git ls-files '*.cpp' '*.h')
```

**Static analysis:**

```bash
clang-tidy -p . $(git diff --name-only main -- '*.cpp')
```

**Compiler flags in use:**

```
-Wall -Werror -Wextra -std=c++17 -O3 -fPIC -g -I /usr/include/rtmidi
-fsanitize=address
```

**Runtime log:**

The library writes to `/run/log/libmtcmaster.log` (append mode). Each session is separated
by a separator line and a timestamped "MTC Master Initialized" / "MTC Master finished" pair.
Timing overruns appear as `TIME OUT!!!` lines with the current time, the expected next-quarter
timestamp, and the overrun delta in nanoseconds.

---

[↑ Back to Table of Contents](#table-of-contents)

## Contributors

Contributions to `libmtcmaster` follow the full workflow described in
[CONTRIBUTORS.md](./CONTRIBUTORS.md). Key points:

* Non-trivial changes (any logic in `MtcMaster_class.cpp`, `interface.cpp`, or `python/`) require
  a spec document committed on the feature branch before a PR is opened.
* TDD is required for Tier 2 changes: write a failing test, confirm it fails, then implement.
* All commits must be signed off with `git commit -s` (DCO).
* PRs target `main`.
* Review is by **Ion Reguera** ([@ibiltari](https://github.com/ibiltari)) or
  **Adrià Masip** ([@backenv](https://github.com/backenv)).
* Every new source file must carry the SPDX header
  (`SPDX-FileCopyrightText: <year> Stagelab Coop SCCL` / `SPDX-License-Identifier: GPL-3.0-or-later`).

See [CONTRIBUTORS.md](./CONTRIBUTORS.md) for the complete contributing workflow.

---

[↑ Back to Table of Contents](#table-of-contents)

## Release Notes

See [CHANGELOG.md](./CHANGELOG.md) for the full history.

**v0.1 — 2026-05-31**

Initial public release of `libmtcmaster`. The library delivers a fully functional MIDI Time
Code master generator over ALSA, with a `SCHED_RR` real-time playback thread, sub-millisecond
`nanosleep`-based timing, and support for 24, 25, 29, and 30 fps frame rates. A C `extern "C"`
FFI layer (`interface.h`) enables embedding from any C-compatible runtime, and a Python
`ctypes` wrapper (`python/mtcsender.py`) provides a Pythonic interface used by the CueMS audio
and media players. The Makefile produces a versioned shared object (`libmtcmaster.so.0.1`) with
the standard SONAME symlink chain and supports `prefix`-based installation for Debian packaging.

---

[↑ Back to Table of Contents](#table-of-contents)

## Future Developments

The items below describe planned infrastructure that is **not yet implemented**. They are
documented here so that each component can be wired in incrementally without architectural
surprises.

### Automated Test Suite and Coverage

Currently `libmtcmaster` ships a manual integration test application (`test_app/`) and a
Python integration script (`python/mtcsender_test.py`) but has no automated test suite.

Planned work:
- Add a `tests/` directory with a lightweight C++ test harness (e.g. Catch2 or GoogleTest).
- Unit-test `fillMtcTimeVector` against known nanosecond→HH:MM:SS:FF conversions for all
  four frame rates.
- Unit-test `mtcTimeVectorString` formatting for edge cases (zero, 23:59:59:29, etc.).
- Integration-test `sendMtcPosition` output using the `RtMidi::RTMIDI_DUMMY` API to capture
  outgoing bytes without a physical MIDI interface.
- Wire `ctest` targets into a `CMakeLists.txt` (or a Makefile `test` target) so tests are
  runnable with a single command.

The coverage pipeline will use `lcov`/`gcov` and upload to Codecov. The activation step
(visiting `https://codecov.io/gh/stagesoft/libmtcmaster` and clicking "Activate") is a
one-time manual step that must be performed after the first successful CI run.

### CI/CD Workflow

Planned: `.github/workflows/tests.yml` triggered on push to `main` and on pull requests.

```yaml
# Sketch — not yet created
name: Tests
on:
  push:
    branches: [main]
  pull_request:
jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Install dependencies
        run: sudo apt-get install -y build-essential librtmidi-dev lcov
      - name: Build with coverage
        run: |
          make CXXFLAGS="--coverage" LDFLAGS="--coverage"
      - name: Run tests
        run: ctest --test-dir build --output-on-failure
      - name: Generate coverage report
        run: |
          lcov --capture --directory . --output-file coverage.info --ignore-errors inconsistent
          lcov --remove coverage.info '/usr/*' '*/tests/*' --output-file coverage.info
      - name: Upload coverage to Codecov
        uses: codecov/codecov-action@v4
        with:
          files: coverage.info
          fail_ci_if_error: false
        env:
          CODECOV_TOKEN: ${{ secrets.CODECOV_TOKEN }}
```

The Tests badge and Coverage badge below will become live once this workflow is committed
and the repository is activated on Codecov:

```
[![Tests](https://github.com/stagesoft/libmtcmaster/actions/workflows/tests.yml/badge.svg)](https://github.com/stagesoft/libmtcmaster/actions/workflows/tests.yml)
[![Coverage](https://codecov.io/gh/stagesoft/libmtcmaster/graph/badge.svg)](https://codecov.io/gh/stagesoft/libmtcmaster)
```

### Documentation Site

Planned: Doxygen-generated HTML API reference deployed to GitHub Pages via
`.github/workflows/docs.yml`.

The Doxygen run will parse `MtcMaster_class.h` and `interface.h` and produce a browsable
reference at `https://stagesoft.github.io/libmtcmaster/`. A `Doxyfile` with
`GENERATE_HTML = YES`, `EXTRACT_ALL = YES`, and `HAVE_DOT = YES` (Graphviz) is the target
configuration. The docs workflow will be kept as a separate concern from `tests.yml` so
that a documentation-only push does not trigger a full build-and-test cycle.

Once live, the Deploy API documentation badge will be:

```
[![Deploy API documentation](https://github.com/stagesoft/libmtcmaster/actions/workflows/docs.yml/badge.svg)](https://github.com/stagesoft/libmtcmaster/actions/workflows/docs.yml)
```

### Packaging and Debian Integration

Planned: a `debian/` packaging branch (`debian/bookworm`) producing:

- `libmtcmaster0` — the shared library package (`libmtcmaster.so.0.1` + SONAME symlinks).
- `libmtcmaster-dev` — headers and the unversioned `libmtcmaster.so` symlink for compile-time
  linking.
- A `debian/libmtcmaster.install` file mapping the build outputs to the correct `usr/lib`
  paths.
- The `Depends:` field will list `librtmidi6` (or the current Bookworm version).

Installation via the future Debian package:

```bash
sudo dpkg -i libmtcmaster0_0.1_amd64.deb
sudo dpkg -i libmtcmaster-dev_0.1_amd64.deb
```

### Target Badge Set

Once all the above pipelines are live, the full badge row will be:

```markdown
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![Tests](https://github.com/stagesoft/libmtcmaster/actions/workflows/tests.yml/badge.svg)](https://github.com/stagesoft/libmtcmaster/actions/workflows/tests.yml)
[![Coverage](https://codecov.io/gh/stagesoft/libmtcmaster/graph/badge.svg)](https://codecov.io/gh/stagesoft/libmtcmaster)
[![Deploy API documentation](https://github.com/stagesoft/libmtcmaster/actions/workflows/docs.yml/badge.svg)](https://github.com/stagesoft/libmtcmaster/actions/workflows/docs.yml)
```

---

[↑ Back to Table of Contents](#table-of-contents)

## Copyright Notice

`libmtcmaster` is copyright © 2026 Stagelab Coop SCCL and is free software: you can
redistribute it and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation, either version 3 of the License, or (at your
option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with this program.
If not, see <https://www.gnu.org/licenses/>.

---

[↑ Back to Table of Contents](#table-of-contents)

## License

`libmtcmaster` is licensed under the **GNU General Public License v3.0 or later**
([GPL-3.0-or-later](https://www.gnu.org/licenses/gpl-3.0)).

The full license text is in [LICENSE](./LICENSE).

SPDX identifier: `GPL-3.0-or-later`

**Runtime dependency licence note:** `libmtcmaster` links against **RtMidi**, which is
distributed under its own permissive MIT-style licence. RtMidi itself is not redistributed
in this repository — it is a system package (`librtmidi-dev`). Packagers incorporating
`libmtcmaster` into a distribution should verify that the RtMidi licence terms are met for
their distribution format.
