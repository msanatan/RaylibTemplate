#ifndef GBM_LAUNCH_OPTIONS_H
#define GBM_LAUNCH_OPTIONS_H

#include <string>
#include <vector>

struct LaunchOptions {
  // Comma-separated when starting on a stack, bottom first: "gameplay,pause"
  std::string scene = "splash";
  std::string script;
  std::string session;
  std::string log;
};

bool ParseLaunchOptions(const std::vector<std::string>& arguments,
                        LaunchOptions& options, std::string& error);

#endif  // !GBM_LAUNCH_OPTIONS_H
