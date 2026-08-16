# Official games as independent repositories

A decision about how TGE and its games relate. TGE is the engine/library; a game
that grows past a simple example becomes its **own repository** that consumes TGE
as an external dependency.

## Organization

```text
github.com/daimgz/TGE
github.com/daimgz/tge-tetris
github.com/daimgz/tge-sokoban
github.com/daimgz/tge-pacman
...
```

TGE may keep small examples that demonstrate the API, but a game with its own
development, tests, assets, roadmap and releases lives as a standalone project.

### Names

Prefer `tge-<game>` (e.g. `tge-tetris`, `tge-sokoban`, `tge-pacman`): the prefix
marks it as an official TGE-ecosystem project and works well for GitHub, packages
and binaries. The binary may carry the same name (`tge-tetris`).

## Build system

Independent games use **CMake**.

Motives: Linux / Windows / macOS, IDE integration, `cmake --install`, and
packaging. TGE itself stays **Make-based** (not converted to CMake just to please
the games' build system).

```text
tge-tetris/
├── CMakeLists.txt
├── src/
├── include/
├── tests/
├── assets/
├── docs/
└── README.md
```

Do **not** copy TGE physically inside the game (`third_party/tge/` with a source
copy). TGE remains an external dependency.

## Depending on TGE during development

The game pins TGE to an exact revision — never to `main` without a pin.

```cmake
set(TGE_GIT_REPOSITORY "https://github.com/daimgz/TGE.git" CACHE STRING "...")
set(TGE_GIT_TAG "abc1234" CACHE STRING "TGE commit or tag")
```

`GIT_TAG` may be a commit SHA (typical during iterative engine+game development)
or a release tag (for a game release). Example: while Tetris needs a capability
TGE gained at commit `abc1234`, the game points at `abc1234` — no TGE release
required for every change.

Three consumption modes:

1. **`TGE_SOURCE_DIR`** — local TGE checkout for joint engine+game development.
2. **`TGE_GIT_TAG`** — exact Git revision; mandatory for reproducible standalone
   builds (CI, packaging).
3. **`TGE_INSTALL_PREFIX` / `TGE_DIR`** (find_package) — consumer of an installed
   TGE; deferred to a later iteration. `FetchContent`/`ExternalProject` is a dev
   convenience, not an obligation of the final consumer.

## Joint development flow

```text
game development
   ↓
game finds it needs a capability
   ↓
design/implement that capability in TGE
   ↓
commit TGE
   ↓
game pins that commit
   ↓
game tests
   ↓
validation
   ↓
later, when appropriate: release of TGE
```

Different games may temporarily pin different TGE revisions. That is valid and
desirable during development.

## Release vs development

- **Development:** the game may pin a commit SHA.
- **Game release:** prefer depending on a tagged TGE release
  (`tge-tetris v0.1.0 → TGE v1.0.0-beta.1`, later `→ TGE v1.1.0`). A release has
  a reproducible, semantically identifiable dependency.

Never depend on `TGE main` without a fixed revision.

## Relationship

A game must **not** modify TGE locally to solve its problems. If a game needs an
engine feature:

```text
game → real need → TGE → commit → game updates its pin
```

never:

```text
game → modified copy of TGE
```

This avoids accidental forks and keeps TGE as the single implementation.

## Tetris as the first case

`tge-tetris` is the first game under this model: C++17, CMake, TGE as a pinned Git
dependency. It can grow well past the original example (7-bag, ghost, SRS,
scoring, levels, hold, preview, replay, stats, …). That development happens in the
game repo, not in TGE's examples.

## Practical rule

Do not create a new repository just to change files. Create an independent repo
when a game has its own development, tests, roadmap, and TGE-version pin. Small
examples and API probes stay inside TGE.

## Final shape

```text
        TGE
         │
   ┌─────┼─────┐
   │     │     │
tge-tetris tge-pacman tge-sokoban
   │     │     │
 CMake  CMake  CMake
   └── TGE dependency (commit / release) ──┘
```

TGE is the platform; the games are independent projects that consume it and,
through real use, may drive its evolution.
