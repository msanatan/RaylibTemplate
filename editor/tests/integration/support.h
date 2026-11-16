#ifndef GBM_INTEGRATION_SUPPORT_H
#define GBM_INTEGRATION_SUPPORT_H

#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include "levels/document/level.h"
#include "levels/editing/file_target.h"
#include "transport/cli.h"

// A project directory that is removed when the test ends. Kept under /tmp
// with a short name because Unix socket paths have a small length limit.
struct TemporaryProject {
  std::filesystem::path root;

  TemporaryProject() {
    root = std::filesystem::path("/tmp") /
           ("scenic-test-" + std::to_string(std::random_device{}()));
    std::filesystem::create_directories(root / "assets" / "levels");
    std::ofstream(root / "project.toml") << "[editor]\n";
  }
  ~TemporaryProject() { std::filesystem::remove_all(root); }
  TemporaryProject(const TemporaryProject&) = delete;
  TemporaryProject& operator=(const TemporaryProject&) = delete;

  std::string level(const std::string& name = "level.json") const {
    return (root / "assets" / "levels" / name).string();
  }
};

struct CliResult {
  int exitCode = 0;
  LevelJson json;
};

// Runs scenic-cli in process, exactly as the executable would
inline CliResult Cli(const std::vector<std::string>& arguments,
                     const std::string& standardInput = "") {
  std::istringstream in(standardInput);
  std::ostringstream out;
  CliResult result;
  result.exitCode = RunCli(arguments, in, out);
  result.json = LevelJson::parse(out.str());
  return result;
}

inline std::string Revision(const CliResult& result) {
  return result.json["document"]["revision"].get<std::string>();
}

inline std::string FileBytes(const std::string& path) {
  return ReadTextFile(path).value_or("");
}

#endif  // !GBM_INTEGRATION_SUPPORT_H
