#ifndef GBM_LEVEL_FILE_TARGET_H
#define GBM_LEVEL_FILE_TARGET_H

#include <optional>
#include <string>

#include "levels/document/serialization.h"
#include "levels/editing/commands.h"

std::optional<std::string> ReadTextFile(const std::string& path);

// A hash of the exact bytes, so any outside edit changes it
std::string FileRevision(const std::string& bytes);

// Writes a temporary file beside the target and renames it into place.
// `expectedRevision` is the revision the caller last saw, or empty when the
// file must not exist yet. Throws CommandFailure when another writer holds
// the file, the disk content has moved on, or the write fails.
void WriteLevelFile(const std::string& path, const std::string& text,
                    const std::string& expectedRevision);

std::string FindProjectRoot(const std::string& path);

class FileTarget : public CommandTarget {
 public:
  explicit FileTarget(std::string path, std::string assetsDirectory = "");

  const char* kind() const override { return "file"; }
  DocumentState document() const override;
  std::string assetsDirectory() const override { return assets; }
  LevelDiagnostics loadErrors() const override;
  std::string commit(const Level& level, const std::string& label) override;

  const std::string& path() const { return file; }
  bool exists() const { return text.has_value(); }
  const std::string& bytes() const { return *text; }
  void reload();

 private:
  std::string file;
  std::string assets;
  std::optional<std::string> text;
  LevelParseResult parsed;
};

void RegisterFileCommands(CommandRegistry& registry);

#endif  // !GBM_LEVEL_FILE_TARGET_H
