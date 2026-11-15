#ifndef GBM_REGISTRY_H
#define GBM_REGISTRY_H

#include <string>

#include "core/scene_manager.h"

// Lets `--scene NAME` start the game on any screen. Returns false for an unknown name.
bool PushSceneByName(SceneManager &scenes, const std::string &name);

#endif // !GBM_REGISTRY_H
