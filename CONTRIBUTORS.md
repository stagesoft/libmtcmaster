<!--
SPDX-FileCopyrightText: 2026 Stagelab Coop SCCL
SPDX-License-Identifier: GPL-3.0-or-later
-->

# Contributing to libmtcmaster

Thank you for your interest in `libmtcmaster`. This library runs inside every CueMS player
node to drive the MIDI Time Code master clock — a timing regression here causes audible
artefacts at show time. These guidelines exist to protect that reliability while keeping the
project open to external contributions.

---

## Table of Contents

1. [Prerequisites](#1-prerequisites)
2. [Development Setup](#2-development-setup)
3. [Contribution Tiers](#3-contribution-tiers)
4. [Branch Naming](#4-branch-naming)
5. [Spec-First Requirement](#5-spec-first-requirement)
6. [TDD Workflow — Non-Negotiable](#6-tdd-workflow--non-negotiable)
7. [Commit Hygiene](#7-commit-hygiene)
8. [Developer Certificate of Origin (DCO)](#8-developer-certificate-of-origin-dco)
9. [Pull Request Requirements](#9-pull-request-requirements)
10. [Acceptance Criteria](#10-acceptance-criteria)
11. [Review Process](#11-review-process)
12. [Changelog Line](#12-changelog-line)
13. [Dependency Governance](#13-dependency-governance)
14. [License](#14-license)

---

## 1. Prerequisites

| Tool | Version | Notes |
|---|---|---|
| C++ compiler | g++ ≥ 12 (C++17) | clang++ ≥ 14 also supported |
| GNU Make | any recent | the build system used by this repository |
| librtmidi | any recent Debian package | `librtmidi-dev` provides headers and the shared object |
| Git | any recent | DCO sign-off required (see §8) |
| clang-format | any recent | required for lint gate |
| clang-tidy | any recent | required for static analysis gate |
| lcov / gcov | optional | only needed to produce coverage locally |
| Python ≥ 3.10 | optional | only needed to work on `python/` bindings |

System packages (Debian/Ubuntu):

```bash
sudo apt-get install -y \
  build-essential librtmidi-dev \
  clang-format clang-tidy lcov
```

The log directory must exist and be writable before any test that instantiates `MtcMaster`:

```bash
sudo mkdir -p /run/log && sudo chmod a+w /run/log
```

---

## 2. Development Setup

```bash
# Clone the repository
git clone https://github.com/stagesoft/libmtcmaster.git
cd libmtcmaster

# Build the shared library
make

# Build and run the interactive test application
cd test_app
make
./mtcmaster_test_app
```

**Clean rebuild:**

```bash
make clean && make
```

**Build with AddressSanitizer (already enabled in the default Makefile `LDFLAGS`):**

The default build already links `-fsanitize=address`. To run under ASan:

```bash
make
# Run any program that loads libmtcmaster.so — ASan reports are printed to stderr
```

**Format check (must pass before opening a PR):**

```bash
clang-format --dry-run --Werror $(git ls-files '*.cpp' '*.h')
```

**Static analysis on changed files:**

```bash
clang-tidy -p . $(git diff --name-only main -- '*.cpp')
```

**Python bindings (from the `python/` directory):**

```bash
cd python
python3 mtcsender_test.py
```

---

## 3. Contribution Tiers

The review requirements depend on what you change.

### Tier 1 — Trivial

No change to any file under `MtcMaster_class.cpp`, `interface.cpp`, or `python/` beyond a
single-line correction. Covers: README edits, comment corrections, adding a test for
already-shipped behaviour, Makefile packaging fixes, CI/CD config changes.

**Gates:** lint + CI green; one owner approval. No spec required.

### Tier 2 — Non-trivial

Any addition, modification, or deletion of logic in `MtcMaster_class.cpp`, `interface.cpp`,
`python/mtcsender.py`, or the `test_app/` source. Includes bug fixes that change branching
behaviour, new features, refactors, new class introductions, and changes to the C FFI
contract.

**Gates:** spec + plan + tasks committed on the branch; failing test before implementation;
CI green (tests + coverage); one mandatory owner approval.

---

## 4. Branch Naming

```
feat/NNN-short-description       ← new feature  (NNN = spec number, e.g. 001)
fix/NNN-short-description        ← bug fix referencing a spec or issue number
chore/short-description          ← non-production changes (CI, tooling, docs)
build/short-description          ← packaging / build-system changes
```

The `NNN` prefix links the branch to `specs/NNN-feature/` artifacts. Branches without a
valid prefix will not be merged.

---

## 5. Spec-First Requirement

For **Tier 2** changes, before opening a PR for review you MUST commit these files on your
feature branch:

```
specs/NNN-feature/spec.md      ← feature specification
specs/NNN-feature/plan.md      ← implementation plan
specs/NNN-feature/tasks.md     ← task list
```

`spec.md` must address at minimum:

- What behaviour changes and why.
- What invariants the change preserves (timing accuracy, FFI stability, thread-safety scope).
- How the change will be tested.

The PR description must link to the spec directory. A PR without it will be marked as a
draft and returned for pre-work.

If you are a first-time contributor unfamiliar with the spec format, open an issue first
and the maintainers will help you scope the work.

---

## 6. TDD Workflow — Non-Negotiable

`libmtcmaster` enforces Test-Driven Development for all Tier 2 changes. This is not a style
preference — it is required to protect the timing accuracy of a real-time audio/visual system.

The mandatory sequence:

```
1. Write a failing test that precisely describes the intended behaviour.
2. Confirm the test fails (run `make test` or `ctest` locally and record the failure).
3. Write the minimum production code required to make the test pass.
4. Refactor without changing observable behaviour, keeping all tests green.
```

Your git log on the feature branch MUST show this order. The PR template asks for the commit
SHA of your failing-test commit. Reviewers will check it.

> **Note:** A formal automated test suite (`tests/`, `ctest`) is planned but not yet in place
> (see the [README Future Developments](./README.md#future-developments) section). Until it
> exists, contributors must document the manual test procedure in their PR and provide
> evidence (log output, session transcript, or similar) that the failing state was observed
> before the implementation commit.

---

## 7. Commit Hygiene

`libmtcmaster` follows [Conventional Commits v1.0](https://www.conventionalcommits.org/en/v1.0.0/).

**Commit message format:**

```
<type>[optional scope]: <description>

[optional body]

[optional footer(s)]
Signed-off-by: Your Name <your@email.com>
```

Allowed types: `feat`, `fix`, `test`, `refactor`, `docs`, `chore`, `ci`, `perf`, `build`,
`patch`, `spec`.

Breaking changes: append `!` after the type and include a `BREAKING CHANGE:` footer.

Rules:

- Each commit MUST represent one logical change.
- Do not squash unrelated changes into a single commit.
- Force-pushing to `main` is forbidden. Amending published commits on shared branches is
  forbidden.
- Spec / plan / tasks commits use the `spec:` type prefix.

**Examples:**

```
feat(MtcMaster): add setFrameRate that reloads currentFRBits atomically

fix(threadedMethod): clamp mtcTime adjustment to prevent underflow on stop

test: add unit tests for fillMtcTimeVector at 24 and 30 fps

chore(Makefile): add test target wiring ctest

spec: add spec/001-fps-forwarding for Tier 2 fps-forwarding feature
```

---

## 8. Developer Certificate of Origin (DCO)

Every commit must carry a `Signed-off-by` line, asserting that you have the right to submit
the contribution under GPL-3.0, as per the
[Developer Certificate of Origin](https://developercertificate.org).

```bash
git commit -s -m "feat: add support for ..."
```

To add sign-off to all commits in a branch automatically, set:

```bash
git config --local format.signOff true
```

PRs that contain unsigned commits will not be merged.

---

## 9. Pull Request Requirements

Open your PR against `main`. Use the PR template — it contains the full acceptance
checklist.

Every PR MUST include in its description:

1. **Summary** — what changed and why (2–5 sentences).
2. **Changelog Line** — see §12.
3. **Spec links** (Tier 2 only) — link to `specs/NNN-feature/`.
4. **Failing-test commit SHA** (Tier 2 only) — the commit where the test suite was red
   before implementation began.
5. **Tick-path impact statement** (Tier 2 only) — does the change touch the real-time
   `threadedMethod` loop? If yes, explain why the change does not introduce heap allocation,
   blocking syscalls, or mutex contention in the hot path.
6. **Completed PR checklist** — all items in the template ticked.

Draft PRs are welcome for early feedback on approach. Drafts will not be formally reviewed.
Convert to Ready when all gates pass.

---

## 10. Acceptance Criteria

A PR is ready to merge when ALL of the following are true:

| Criterion | How verified |
|---|---|
| `spec.md`, `plan.md`, `tasks.md` committed on branch (Tier 2) | Reviewer reads the files |
| Failing test committed before implementation (Tier 2) | SHA provided; reviewer checks git log |
| All tests pass (manual procedure until `ctest` is wired) | Evidence in PR description |
| `clang-format --dry-run --Werror` passes on all changed files | CI lint gate / manual |
| `clang-tidy` reports no new warnings on changed files | CI lint gate / manual |
| No new build-time dependency without justification | Reviewer checks `Makefile` diff |
| SPDX header on all new source files | Reviewer inspects new files |
| DCO sign-off on all commits | GitHub DCO check |
| At least one owner approval | GitHub branch protection |
| Tick-path changes accompanied by argument that the path remains non-blocking | Reviewer reads PR body |

---

## 11. Review Process

All PRs to `main` require approval from at least one repository owner:

- **Ion Reguera** ([@ibiltari](https://github.com/ibiltari))
- **Adrià Masip** ([@backenv](https://github.com/backenv))

This is enforced by `.github/CODEOWNERS` and GitHub branch protection.

**What owners check:**

- Spec, plan, and tasks are coherent with the implementation (Tier 2).
- TDD sequence is evidenced in the git log.
- All gates pass.
- No new runtime dependency slipped in without justification.
- SPDX header present on all new source files.
- The real-time playback loop (`threadedMethod`) remains free of heap allocation, mutex
  contention, and blocking syscalls other than `nanosleep`.
- `setFrameRate` and `setTime` follow the established pause/seek/resume contract and send a
  Full Frame SysEx to allow receivers to re-lock immediately.
- The C FFI surface (`interface.h`) is not extended without a corresponding Python binding
  update, so the two layers remain in sync.

Expect review turnaround within 5 business days. For urgent fixes, open an issue first and
tag a maintainer — that speeds triage.

---

## 12. Changelog Line

You do not edit `CHANGELOG.md` — that is the maintainers' responsibility at release time.
Instead, include a **Changelog Line** in your PR description. Maintainers copy this line
verbatim (or lightly edited) when cutting a release.

Format:

```
[TYPE] Past-tense sentence describing what changed and why it matters to users.
```

Types: `Added`, `Changed`, `Fixed`, `Removed`, `Security`, `Performance`.

Examples:

```
[Added] MtcMaster::setFrameRate now reloads currentFRBits atomically so frame-rate
        changes take effect without a stop/start cycle.

[Fixed] threadedMethod no longer underflows mtcTime on stop when position is < 2 frames.

[Changed] Python MtcSender now forwards fps to the C++ library via MTCSender_setFrameRate.

[Removed] Direct port-0 opening in MtcMaster constructor; callers must now call openPort()
          explicitly to avoid surprise double-open via the FFI layer.
```

---

## 13. Dependency Governance

No new entry under the `LBLIBS` or `LDFLAGS` variables in the `Makefile`, and no new
`#include` of a third-party header that is not already available as a system package, may be
introduced without:

1. A written justification in the PR description explaining why the standard library and the
   existing dependency (`librtmidi`) cannot solve the problem.
2. A corresponding update to the Installation section of `README.md` listing the new
   `apt-get install` requirement.
3. Explicit acknowledgement from a repository owner in the review.

**Current runtime dependencies:**

- `librtmidi-dev` — cross-platform MIDI I/O (`RtMidiOut`, `RtMidiError`).

All other dependencies are satisfied by the C++17 standard library and POSIX APIs
(`pthread`, `nanosleep`, `chrono`, `ofstream`).

Git submodules are not currently used in this repository. If one is introduced, bumping a
pinned submodule commit requires the same justification as adding a new dependency, plus a
full build-and-test pass against the new pin.

---

## 14. License

`libmtcmaster` is licensed under the GNU General Public License v3.0 or later (GPL-3.0-or-later).
By contributing, you agree that your contributions will be licensed under GPL-3.0-or-later.

All new source files MUST carry the following SPDX header.

**C / C++ source files:**

```cpp
/*
 * ***
 * SPDX-FileCopyrightText: <year> Stagelab Coop SCCL
 * SPDX-License-Identifier: GPL-3.0-or-later
 * ***
 */
```

**Markdown files:**

```markdown
<!--
SPDX-FileCopyrightText: <year> Stagelab Coop SCCL
SPDX-License-Identifier: GPL-3.0-or-later
-->
```

**Makefile / shell / Python files:**

```bash
# ***
# SPDX-FileCopyrightText: <year> Stagelab Coop SCCL
# SPDX-License-Identifier: GPL-3.0-or-later
# ***
```

Replace `<year>` with the year the file is first committed.
