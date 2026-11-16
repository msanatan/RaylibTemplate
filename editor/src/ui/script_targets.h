#ifndef SCENIC_SCRIPT_TARGETS_H
#define SCENIC_SCRIPT_TARGETS_H

#include <map>
#include <string>

#include "raylib.h"

// Where named widgets were drawn this frame, so scripted runs can click them
// without depending on the layout: "row:<object id>", "field:position"
std::map<std::string, Rectangle>& ScriptTargets();
void RecordScriptTarget(const std::string& name);

#endif  // !SCENIC_SCRIPT_TARGETS_H
