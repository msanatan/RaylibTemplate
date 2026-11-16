#ifndef SCENIC_PROJECT_H
#define SCENIC_PROJECT_H

#include <string>
#include <vector>

struct Project {
  std::string root;
  std::string assetsDirectory;
  std::string preferencesDirectory;
  std::string defaultLevel;
  std::string gameExecutable;
  std::vector<std::string> playArguments;
  std::vector<std::string> resetArguments;

  std::string resolve(const std::string& path) const;
};

bool LoadProject(const std::string& root,
                 const std::string& fallbackGameExecutable, Project& project,
                 std::string& error);

#endif  // !SCENIC_PROJECT_H
