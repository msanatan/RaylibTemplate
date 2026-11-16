#ifndef GBM_LOG_H
#define GBM_LOG_H

#include <string>

// Game code keeps logging through raylib's TraceLog. Once InitLog has run,
// every line is stamped as [seconds frame level scene] and copied to logFile
// when one is given.
void InitLog(const std::string& logFile);
void CloseLog();

void LogNextFrame();
void LogSceneStack(const std::string& sceneStack);

#endif  // !GBM_LOG_H
