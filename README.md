# Raylib Template


This is a template for a raylib project. It uses CMake and Ninja to build the project and includes a few useful things:


- Basic Raylib 3d project
- GitHub Actions for building for Windows, macOS, and HTML5
- Your hopes and dreams

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
