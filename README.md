# Raylib Template


This is a template for a raylib project. It uses CMake and Ninja to build the project and includes a few useful things:


- Basic Raylib 3d project
- GitHub Actions for building for Windows, macOS, and HTML5
- Your hopes and dreams

## Project layout

```
src/
  main.cpp     window, main loop, pushes the first scene
  core/        the template itself, identical across games: assets, scene stack, menu widget
  scenes/      one file pair per screen: splash, main_menu, settings, gameplay, pause
  game/        this game's own code: player, entities, levels
assets/
  branding/  textures/  models/  fonts/  shaders/  data/
  audio/music/  audio/sfx/
```

Each screen is a `Scene` with `update` and `draw`. Scenes change screens through the `SceneManager` they are given:

- `scenes.push<PauseScene>()` puts a scene on top and keeps the one below alive
- `scenes.pop()` closes the top scene
- `scenes.replace<GameplayScene>()` swaps the top scene
- `scenes.reset<MainMenuScene>()` clears the whole stack first
- `scenes.quit()` exits the game

Load assets through `core/assets.h` in a scene's constructor and unload them in its destructor.

In Debug desktop builds, saving any file under `assets/` while the game is running rebuilds the live scenes so they load the new version.

## Testing

```sh
ctest --preset test-macos-debug               # everything
ctest --preset test-macos-debug -L unit       # unit tests only, no window needed
ctest --preset test-macos-debug -L playtest   # scripted runs of the real game
```

- Unit tests live in `tests/unit/` and use [doctest](https://github.com/doctest/doctest).
- Playtests are the scripts in `tests/playtests/`. Each one drives the game with scripted input, checks its state and saves screenshots to `build/<preset>/shots/`. The commands are listed in `src/core/playtest.h`.

The game also takes these options directly:

```sh
RaylibTemplate --scene gameplay            # start on a scene from src/scenes/registry.cpp
RaylibTemplate --script my_playtest.txt    # run a playtest script, exit 1 if an `expect` fails
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
