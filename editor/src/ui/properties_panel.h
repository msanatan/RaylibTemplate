#ifndef SCENIC_PROPERTIES_PANEL_H
#define SCENIC_PROPERTIES_PANEL_H

#include <initializer_list>
#include <string>

#include "levels/document/level_index.h"
#include "ui/context.h"

inline constexpr const char* assetDragPayload = "SCENIC_ASSET";

class PropertiesPanel {
 public:
  void draw(const UiContext& ui);

 private:
  struct NumberField {
    const char* label;
    const char* operation;
    const char* field;
    float speed;
    const char* format;
  };

  void drawIdentity(EditorSession& session, const LevelObject& object);
  void drawTransform(EditorSession& session, const LevelObject& object,
                     const std::optional<LevelTransform>& world);
  void drawShape(EditorSession& session, const LevelObject& object);
  void drawCamera(EditorSession& session, const LevelObject& object);
  void drawAppearance(EditorSession& session, const LevelObject& object);
  void drawColor(EditorSession& session, const LevelObject& object);
  void drawTexture(EditorSession& session, const LevelObject& object);
  void drawGameData(EditorSession& session, const LevelObject& object);
  void applyGameData(EditorSession& session, const LevelObject& object);
  void dragNumbers(EditorSession& session, const std::string& id,
                   const NumberField& field,
                   std::initializer_list<double> values);

  // The custom-properties text being edited, kept until applied or the
  // selection or document changes underneath it
  std::string draftObject;
  std::string draftRevision;
  std::string draftText;
  std::string draftError;
  bool pickingColor = false;
};

#endif  // !SCENIC_PROPERTIES_PANEL_H
