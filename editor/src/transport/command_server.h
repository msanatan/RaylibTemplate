#ifndef SCENIC_COMMAND_SERVER_H
#define SCENIC_COMMAND_SERVER_H

#include <atomic>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "levels/document/level.h"

struct SessionInfo {
  std::string id;
  std::string project;
  std::string socket;
  long long pid = 0;
};

std::string SessionsDirectory();
std::vector<SessionInfo> ListSessions();
std::optional<SessionInfo> FindSession(const std::string& id);

std::optional<std::string> SendToSession(const SessionInfo& session,
                                         const std::string& request,
                                         std::string& error);

struct LiveRequest {
  LevelJson request;
  std::shared_ptr<std::promise<std::string>> response;
};

class CommandServer {
 public:
  CommandServer() = default;
  ~CommandServer();
  CommandServer(const CommandServer&) = delete;
  CommandServer& operator=(const CommandServer&) = delete;

  bool start(const std::string& project, std::string& error);
  void stop();
  const std::string& sessionId() const { return info.id; }

  std::optional<LiveRequest> next();

 private:
  void serve();
  void handleConnection(int connection);

  SessionInfo info;
  std::string sessionFile;
  int listener = -1;
  std::thread thread;
  std::atomic<bool> running{false};
  std::mutex mutex;
  std::vector<LiveRequest> queue;
};

#endif  // !SCENIC_COMMAND_SERVER_H
