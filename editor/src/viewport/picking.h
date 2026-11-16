#ifndef SCENIC_PICKING_H
#define SCENIC_PICKING_H

#include <string>

#include "levels/document/level_index.h"
#include "raylib.h"

inline constexpr float iconRadius = 0.3f;

std::string PickObject(const LevelIndex& index, Ray ray);

#endif  // !SCENIC_PICKING_H
