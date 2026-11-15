# Raylib Template


This is a template for a raylib project. It uses CMake and Ninja to build the project and includes a few useful things:


- Basic Raylib 3d project
- GitHub Actions for building for Windows, macOS, and HTML5
- Your hopes and dreams

## Project layout

```
src/
  main.cpp     window, main loop, pushes the first scene
  core/        reusable template code
    input/     action bindings and state; touch pointer capture and drawing
    assets/    asset loading, development file watching, packed archive backend
    scenes/    Scene interface and SceneManager stack, sessions, and reloads
    testing/   playtest script parser and frame-by-frame runner
    ui.h/.cpp  menu widget and text drawing helper
    log.h/.cpp             structured logging
    launch_options.h/.cpp  command-line options
  scenes/      one file pair per screen: splash, main_menu, settings, gameplay, pause
  game/        this game's own code: player, entities, levels
assets/
  branding/  textures/  models/  fonts/  shaders/  data/
  audio/music/  audio/sfx/
cmake/
  Assets.cmake  desktop asset packing and web preloading
  Format.cmake  pinned code formatter
```

Each screen is a `Scene` with `update` and `draw`. Scenes change screens through the `SceneManager` they are given:

- `scenes.push<PauseScene>()` puts a scene on top and keeps the one below alive
- `scenes.pop()` closes the top scene
- `scenes.replace<GameplayScene>()` swaps the top scene
- `scenes.reset<MainMenuScene>()` clears the whole stack first
- `scenes.quit()` exits the game

Load assets through `core/assets/assets.h` in a scene's constructor and unload them in its destructor.

In Debug desktop builds, saving any file under `assets/` while the game is running rebuilds the live scenes so they load the new version.

`core/assets/asset_watcher.h/.cpp` handles filesystem polling and debounce. The asset-loading facade keeps the reload callback and load-failure policy.

A scene that implements `saveState()` and `restoreState()` keeps its place through that rebuild, and through a restart when the game is run with `--session`.

## Input

Scenes ask about named actions, never about keys: `ActionPressed("pause")`, `ActionDown(...)`, `ActionAxis("move")`. `src/game/bindings.cpp` is the one place that says which keys, gamepad buttons, gestures and on-screen controls trigger each action.

The action API lives in `core/input/input.h`. `touch_controls.h/.cpp` owns pointer sampling, stick capture, and drawing; the input system decides which actions' controls are active.

On-screen controls appear once the player touches the screen, and only for actions the current scene is asking about. Run with `--touch` to use them with a mouse on desktop.

## Testing

```sh
ctest --preset test-macos-debug               # everything
ctest --preset test-macos-debug -L unit       # unit tests only, no window needed
ctest --preset test-macos-debug -L playtest   # scripted runs of the real game
```

- Unit tests live in `tests/unit/` and use [doctest](https://github.com/doctest/doctest).
- Playtests are the scripts in `tests/playtests/`. Each one drives the game with scripted input, checks its state and saves screenshots to `build/<preset>/shots/`. The commands are listed in `src/core/testing/script.h`.

`core/testing/script.h/.cpp` defines and parses commands. `playtest.h/.cpp` executes them, injects input, and captures screenshots.

The game also takes these options directly:

```sh
RaylibTemplate --scene gameplay            # start on a scene from src/scenes/registry.cpp
RaylibTemplate --scene gameplay,pause      # start on a stack of scenes, bottom first
RaylibTemplate --script my_playtest.txt    # run a playtest script, exit 1 if an `expect` fails
RaylibTemplate --session session.txt       # resume from this file if it exists, save to it on exit
RaylibTemplate --log game.log              # also write the log to this file
RaylibTemplate --touch                     # show on-screen controls and treat the mouse as a finger
```

Every log line is stamped `[seconds frame level scene]`, and each scene change is logged. Game code logs with raylib's `TraceLog`.

## Formatting and linting

Code is formatted with clang-format in Google style, and CI fails on unformatted code. Both tools are pinned and fetched through [uv](https://docs.astral.sh/uv/), so results are the same on every machine.

```sh
cmake -P cmake/Format.cmake                              # format everything
cmake -DCHECK=ON -P cmake/Format.cmake                   # what CI runs
cmake --build --preset build-macos-debug --target lint   # clang-tidy bug checks
```

## Requirements

This template requires:

- CMake
- Ninja

### macOS Setup (with Homebrew)

```sh
brew install cmake ninja
```

### General Build Instructions

```sh
cmake -B build -G Ninja
cmake --build build
```

The built executable will be in the `build/` directory.
