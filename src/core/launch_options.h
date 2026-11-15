#ifndef GBM_LAUNCH_OPTIONS_H
#define GBM_LAUNCH_OPTIONS_H

#include <string>
#include <vector>

struct LaunchOptions
{
  std::string scene = "splash";
  std::string script;
};

// Returns false and describes the problem in `error` for an unknown option or one missing its value
bool ParseLaunchOptions(const std::vector<std::string> &arguments, LaunchOptions &options, std::string &error);

#endif // !GBM_LAUNCH_OPTIONS_H
