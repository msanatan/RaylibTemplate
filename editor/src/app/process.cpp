#include "app/process.h"

#include <filesystem>

#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>

#include <csignal>
#endif

bool LaunchProcess(const std::string& executable,
                   const std::vector<std::string>& arguments,
                   const std::string& workingDirectory, std::string& error) {
  if (!std::filesystem::exists(executable)) {
    error = "The game executable \"" + executable +
            "\" does not exist. Build the game, or set [game] executable in "
            "project.toml";
    return false;
  }

#if defined(_WIN32)
  std::string commandLine = "\"" + executable + "\"";
  for (const std::string& argument : arguments) {
    commandLine += " \"" + argument + "\"";
  }
  STARTUPINFOA startup = {};
  startup.cb = sizeof(startup);
  PROCESS_INFORMATION process = {};
  if (!CreateProcessA(nullptr, commandLine.data(), nullptr, nullptr, FALSE, 0,
                      nullptr, workingDirectory.c_str(), &startup, &process)) {
    error = "Could not start \"" + executable + "\"";
    return false;
  }
  CloseHandle(process.hThread);
  CloseHandle(process.hProcess);
  return true;
#else
  // The editor never waits for the game, so finished games must not linger
  // as zombies
  std::signal(SIGCHLD, SIG_IGN);
  const pid_t child = fork();
  if (child < 0) {
    error = "Could not start \"" + executable + "\"";
    return false;
  }
  if (child == 0) {
    if (chdir(workingDirectory.c_str()) != 0) {
      _exit(127);
    }
    std::vector<char*> argv;
    argv.push_back(const_cast<char*>(executable.c_str()));
    for (const std::string& argument : arguments) {
      argv.push_back(const_cast<char*>(argument.c_str()));
    }
    argv.push_back(nullptr);
    execv(executable.c_str(), argv.data());
    _exit(127);
  }
  return true;
#endif
}
