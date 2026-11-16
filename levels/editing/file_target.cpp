#include "levels/editing/file_target.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>

#include "levels/document/validation.h"

namespace {
namespace fs = std::filesystem;

// Creating a directory either succeeds or reports that it exists, on every
// platform, which makes it a portable lock between cooperating writers
class WriteLock {
 public:
  explicit WriteLock(const std::string& path) : directory(path + ".lock") {
    const auto staleAfter = std::chrono::seconds(10);
    for (int attempt = 0; attempt < 40; attempt++) {
      std::error_code error;
      if (fs::create_directory(directory, error)) {
        held = true;
        return;
      }
      // A writer that crashed leaves its lock behind
      const auto modified = fs::last_write_time(directory, error);
      if (!error && fs::file_time_type::clock::now() - modified > staleAfter) {
        fs::remove(directory, error);
        continue;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
  }
  ~WriteLock() {
    if (held) {
      std::error_code error;
      fs::remove(directory, error);
    }
  }
  WriteLock(const WriteLock&) = delete;
  WriteLock& operator=(const WriteLock&) = delete;

  bool isHeld() const { return held; }

 private:
  fs::path directory;
  bool held = false;
};

FileTarget& AsFile(CommandTarget& target) {
  return dynamic_cast<FileTarget&>(target);
}

LevelJson Format(CommandTarget& target, const LevelJson& args) {
  FileTarget& file = AsFile(target);
  const Level& level = RequireLevel(target);
  const LevelDiagnostics errors = ValidateLevel(level);
  if (!errors.empty()) {
    throw CommandFailure{CommandExit::Invalid, "invalid_document",
                         "Only valid levels can be formatted: " +
                             DescribeDiagnostic(errors.front()),
                         errors};
  }

  const std::string canonical = SerializeLevel(level);
  const bool isCanonical = canonical == file.bytes();
  if (args.value("check", false)) {
    if (!isCanonical) {
      throw CommandFailure{CommandExit::Noncanonical, "noncanonical",
                           "The file is valid but not in canonical form. Run "
                           "`format --expect <revision>` to rewrite it"};
    }
    return {{"canonical", true}};
  }

  RequireRevision(target, args);
  if (isCanonical) {
    return {{"canonical", true}, {"completed", nullptr}};
  }
  WriteLevelFile(file.path(), canonical, file.document().revision);
  file.reload();
  return {{"canonical", true}, {"changed", true}, {"completed", "file_commit"}};
}

LevelJson NewLevel(CommandTarget& target, const LevelJson& args) {
  FileTarget& file = AsFile(target);
  const std::string path = args.value("path", file.path());
  if (fs::weakly_canonical(path) != fs::weakly_canonical(file.path())) {
    throw CommandFailure{CommandExit::Invalid, "invalid_argument",
                         "--path must name the same level as --file"};
  }
  if (file.exists()) {
    throw CommandFailure{
        CommandExit::Io, "file_exists",
        "\"" + file.path() + "\" already exists; `level new` never overwrites"};
  }
  WriteLevelFile(file.path(), SerializeLevel(Level()), "");
  file.reload();
  return {{"changed", true}, {"completed", "file_commit"}};
}
}  // namespace

std::optional<std::string> ReadTextFile(const std::string& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return std::nullopt;
  }
  std::ostringstream text;
  text << file.rdbuf();
  return text.str();
}

std::string FileRevision(const std::string& bytes) {
  return "file:" + HashText(bytes);
}

void WriteLevelFile(const std::string& path, const std::string& text,
                    const std::string& expectedRevision) {
  std::error_code error;
  const fs::path target(path);
  if (target.has_parent_path()) {
    fs::create_directories(target.parent_path(), error);
  }

  const WriteLock lock(path);
  if (!lock.isHeld()) {
    throw CommandFailure{
        CommandExit::Io, "file_locked",
        "Another writer is holding \"" + path + "\". Try again"};
  }

  // Checked under the lock: the file may have changed since it was read
  const std::optional<std::string> current = ReadTextFile(path);
  const std::string currentRevision =
      current.has_value() ? FileRevision(*current) : "";
  if (currentRevision != expectedRevision) {
    throw CommandFailure{
        CommandExit::Conflict, "disk_conflict",
        current.has_value()
            ? "\"" + path +
                  "\" changed on disk since it was read. Inspect it again"
            : "\"" + path + "\" no longer exists on disk"};
  }

  const fs::path temporary = path + ".tmp";
  {
    std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
    out << text;
    out.flush();
    if (!out) {
      fs::remove(temporary, error);
      throw CommandFailure{CommandExit::Io, "write_failed",
                           "Could not write \"" + path + "\""};
    }
  }
  fs::rename(temporary, target, error);
  if (error) {
    fs::remove(temporary, error);
    throw CommandFailure{CommandExit::Io, "write_failed",
                         "Could not replace \"" + path + "\""};
  }
}

std::string FindProjectRoot(const std::string& path) {
  std::error_code error;
  fs::path directory = fs::absolute(path, error);
  if (!fs::is_directory(directory, error)) {
    directory = directory.parent_path();
  }
  while (!directory.empty()) {
    if (fs::exists(directory / "project.toml", error)) {
      return directory.string();
    }
    if (directory == directory.parent_path()) {
      break;
    }
    directory = directory.parent_path();
  }
  return "";
}

FileTarget::FileTarget(std::string path, std::string assetsDirectory)
    : file(std::move(path)), assets(std::move(assetsDirectory)) {
  if (assets.empty()) {
    const std::string project = FindProjectRoot(file);
    assets = project.empty() ? fs::path(file).parent_path().string()
                             : (fs::path(project) / "assets").string();
  }
  reload();
}

void FileTarget::reload() {
  text = ReadTextFile(file);
  parsed = text.has_value() ? ParseLevel(*text) : LevelParseResult();
}

DocumentState FileTarget::document() const {
  DocumentState state;
  state.path = file;
  if (text.has_value()) {
    state.open = true;
    state.revision = FileRevision(*text);
    state.level = parsed.level.has_value() ? &*parsed.level : nullptr;
  }
  return state;
}

LevelDiagnostics FileTarget::loadErrors() const {
  if (text.has_value()) {
    return parsed.errors;
  }
  LevelDiagnostic missing;
  missing.code = "file_not_found";
  missing.message = "\"" + file + "\" does not exist";
  return {missing};
}

std::string FileTarget::commit(const Level& level, const std::string&) {
  WriteLevelFile(file, SerializeLevel(level), document().revision);
  reload();
  return "file_commit";
}

void RegisterFileCommands(CommandRegistry& registry) {
  registry.add({
      .name = "format",
      .description =
          "Check or rewrite a valid level file in canonical serialization.",
      .supportsFile = true,
      .hasSideEffects = true,
      .params =
          {{"check", "boolean", false,
            "Report whether the file is canonical; never writes. Exits 5 when "
            "it is not"},
           {"expect", "string", false,
            "Rewrite the file if it is still at this revision"}},
      .resultDescription = "canonical: true; completed: file_commit, or null "
                           "when nothing was written.",
      .run = Format,
  });
  registry.add({
      .name = "level new",
      .description =
          "Create a valid empty level. Refuses a path that already exists.",
      .supportsFile = true,
      .hasSideEffects = true,
      .params = {{"path", "string", false,
                  "The new level; defaults to the --file target"}},
      .resultDescription = "completed: file_commit.",
      .run = NewLevel,
  });
}
