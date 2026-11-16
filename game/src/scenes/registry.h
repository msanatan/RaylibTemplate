#ifndef GBM_REGISTRY_H
#define GBM_REGISTRY_H

#include <set>
#include <string>

#include "core/scenes/scene_manager.h"

void RegisterScenes(SceneManager& scenes);

// Scenes whose saved state is gameplay progress in an authored level. The
// --fresh-level option starts these from the level's authored state.
std::set<std::string> LevelSceneNames();

#endif  // !GBM_REGISTRY_H
