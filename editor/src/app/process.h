#ifndef SCENIC_PROCESS_H
#define SCENIC_PROCESS_H

#include <string>
#include <vector>

bool LaunchProcess(const std::string& executable,
                   const std::vector<std::string>& arguments,
                   const std::string& workingDirectory, std::string& error);

#endif  // !SCENIC_PROCESS_H
