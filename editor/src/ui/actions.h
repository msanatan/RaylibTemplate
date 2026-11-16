#ifndef SCENIC_ACTIONS_H
#define SCENIC_ACTIONS_H

#include <optional>
#include <string>

#include "document/session.h"
#include "levels/document/level.h"

// Edits the menus, shortcuts, and panels share. Each is an operation batch
// sent through the session, so it validates and undoes like any agent edit.

std::string CreateObject(EditorSession& session, LevelKind kind,
                         const LevelVec3& position,
                         const std::optional<std::string>& parent);
void DuplicateSelection(EditorSession& session);
void DeleteSelection(EditorSession& session);
void ReparentObject(EditorSession& session, const std::string& id,
                    const std::optional<std::string>& parent);
void MoveSibling(EditorSession& session, const std::string& id, int delta);
void PlayLevel(EditorSession& session, bool fresh);

#endif  // !SCENIC_ACTIONS_H
