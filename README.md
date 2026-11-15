# Raylib Template

A C++20 raylib template for 2D and 3D games, with a scene stack, action-based input, asset loading, and scripted playtests. GitHub Actions builds Windows, macOS, and HTML5 versions.

## Build

Requires a C++20 compiler, CMake, and Ninja. Formatting and linting also require [uv](https://docs.astral.sh/uv/).

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

The executable is under `build/` (inside `RaylibTemplate.app/Contents/MacOS/` on macOS). Other platform and configuration presets are in [CMakePresets.json](CMakePresets.json).

Tuni (this is not Unity), is used for placing 3D objects at edit time.

```sh
cmake --preset macos-editor
cmake --build --preset build-macos-editor
./build/macos-editor/bin/Tuni
```

Default, release, and web presets leave the editor disabled.

## Where to work

```text
src/main.cpp   startup and main loop
src/core/      reusable systems: assets, input, scenes, testing, UI, logging
editor/src/    Tuni desktop editor
src/scenes/    screens and scene registration
src/game/      game rules, entities, and input bindings
assets/        game assets
```

Each screen implements [Scene](src/core/scenes/scene.h). Register new screens in `src/scenes/registry.cpp`. The [SceneManager](src/core/scenes/scene_manager.h) supports `push`/`pop` for stacked screens, `replace` for the top screen, `reset` for the entire stack, and `quit`.

Load assets through `core/assets/assets.h` in scene constructors and unload them in destructors. Desktop Debug builds reload scenes when assets change; `saveState()` and `restoreState()` preserve progress across reloads and `--session` restarts.

Query named actions such as `ActionPressed("pause")` and `ActionAxis("move")`. Define bindings in `src/game/bindings.cpp`. On-screen controls appear when touched and while their actions are queried.

## Run and inspect

Pass these options to the built executable:

```sh
RaylibTemplate --scene gameplay            # start directly on a registered scene
RaylibTemplate --scene gameplay,pause      # start with a scene stack, bottom first
RaylibTemplate --script my_playtest.txt    # run a script; exit 1 on failure
RaylibTemplate --session session.txt       # restore/save scene state across runs
RaylibTemplate --log game.log              # copy structured logs to a file
RaylibTemplate --touch                     # test on-screen controls with the mouse
```

Use raylib's `TraceLog` for game logging and scenes' `debugState()` for playtest assertions.

## Verify changes

```sh
ctest --test-dir build --output-on-failure -L unit       # no window needed
ctest --test-dir build --output-on-failure -L playtest   # opens game windows
cmake -P cmake/Format.cmake                             # format code
cmake -DCHECK=ON -P cmake/Format.cmake                   # check formatting
cmake --build build --target lint                      # clang-tidy checks
```

Unit tests live in `tests/unit/`. Playtest examples are in [tests/playtests/](tests/playtests/); screenshots go to the build directory's `shots/` folder. See [script.h](src/core/testing/script.h) for commands and script rules.
