# Raylib Template

A C++20 raylib template for 3D games, with a scene stack, action-based input, asset loading, and scripted playtests. GitHub Actions builds Windows, macOS, and HTML5 versions.

## Build

Requires a C++20 compiler, CMake, and Ninja. Formatting and linting also require [uv](https://docs.astral.sh/uv/).

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

The executable is under `build/game/` (inside `RaylibTemplate.app/Contents/MacOS/` on macOS). With presets, the executable is under `build/<preset>/game/`. Other platform and configuration presets are in [CMakePresets.json](CMakePresets.json).

3D levels are authored in Scenic, the level editor, or with its command-line companion `scenic-cli`. Both are built by the editor presets; [game/project.toml](game/project.toml) holds their settings.

```sh
cmake --preset macos-editor
cmake --build --preset build-macos-editor
./build/macos-editor/bin/Scenic                  # opens this project's level
./build/macos-editor/bin/scenic-cli commands     # list the level commands
```

Default, release, and web presets leave the editor disabled.

## Where to work

```text
game/src/main.cpp  startup and main loop
game/src/core/     reusable systems: assets, input, scenes, testing, UI, logging
game/src/scenes/   screens and scene registration
game/src/game/     game rules, entities, and input bindings
game/assets/       game assets, including 3D levels in assets/levels/
game/tests/        game unit tests and scripted playtests
game/web/          HTML5 shell
game/project.toml  Scenic settings for this game
levels/            3D level library shared by the game and the editor
editor/            Scenic level editor, scenic-cli, and their tests
cmake/             shared formatting tools
```

## 3D levels

Levels are versioned JSON, described by [level.schema.json](levels/document/level.schema.json) and the conventions in [level.h](levels/document/level.h). Gameplay loads [game/assets/levels/main.json](game/assets/levels/main.json) through [World](game/src/game/world.h): the object whose properties contain `"role": "player"` is the player, and the level's designated camera supplies the starting view.

Edit levels in Scenic, with `scenic-cli`, or as text. Scenic's Play and Reset buttons save and launch the game using the arguments in `game/project.toml`. A running Debug game reloads a saved level and keeps gameplay progress; F5 (Debug builds only) or `--fresh-level` returns to the authored starting state.

`scenic-cli` runs the same validated operations as the editor, against a level file (`--file`) or a running editor's unsaved document (`--editor`). Put target options (`--file`, `--editor`, `--assets`) before the command, positional arguments next, and command options last. Boolean options accept an optional `true` or `false` value. [AGENTS.md](AGENTS.md) has the workflow; `scenic-cli commands describe <name>` documents each command.

Each screen implements [Scene](game/src/core/scenes/scene.h). Register new screens in `game/src/scenes/registry.cpp`. The [SceneManager](game/src/core/scenes/scene_manager.h) supports `push`/`pop` for stacked screens, `replace` for the top screen, `reset` for the entire stack, and `quit`.

Load assets through `core/assets/assets.h` in scene constructors and unload them in destructors. Desktop Debug builds reload scenes when assets change; `saveState()` and `restoreState()` preserve progress across reloads and `--session` restarts.

Query named actions such as `ActionPressed("pause")` and `ActionAxis("move")`. Define bindings in `game/src/game/bindings.cpp`. On-screen controls appear when touched and while their actions are queried.

## Run and inspect

Pass these options to the built executable:

```sh
RaylibTemplate --scene gameplay            # start directly on a registered scene
RaylibTemplate --scene gameplay,pause      # start with a scene stack, bottom first
RaylibTemplate --script my_playtest.txt    # run a script; exit 1 on failure
RaylibTemplate --session session.txt       # restore/save scene state across runs
RaylibTemplate --log game.log              # copy structured logs to a file
RaylibTemplate --touch                     # test on-screen controls with the mouse
RaylibTemplate --fresh-level               # ignore saved gameplay progress in level scenes
```

Use raylib's `TraceLog` for game logging and scenes' `debugState()` for playtest assertions.

## Verify changes

```sh
ctest --test-dir build --output-on-failure -L unit       # no window needed
ctest --test-dir build --output-on-failure -L playtest   # opens game windows
ctest --test-dir build --output-on-failure -L integration   # levels, scenic-cli, editor document; editor builds
ctest --test-dir build --output-on-failure -L editor     # opens Scenic windows; editor builds
cmake -P cmake/Format.cmake                             # format code
cmake -DCHECK=ON -P cmake/Format.cmake                   # check formatting
cmake --build build --target lint                      # clang-tidy checks
```

Unit tests live in `game/tests/unit/`. Playtest examples are in [game/tests/playtests/](game/tests/playtests/); screenshots go to the build directory's `shots/` folder. See [script.h](game/src/core/testing/script.h) for commands and script rules. Level tests live in `levels/tests/`. Scenic's tests live in `editor/tests/`; its scripted runs are described in [script.h](editor/src/testing/script.h).
