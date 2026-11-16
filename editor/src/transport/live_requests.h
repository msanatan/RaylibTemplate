#ifndef SCENIC_LIVE_REQUESTS_H
#define SCENIC_LIVE_REQUESTS_H

#include <optional>
#include <string>

#include "document/session.h"
#include "transport/command_server.h"

class LiveRequests {
 public:
  bool start(EditorSession& session, std::string& error);
  void stop() { server.stop(); }

  // Stops at a request that needs the frame drawn first, such as a
  // screenshot; later requests wait so the capture shows the state its
  // response reports
  void process(EditorSession& session);
  bool isAwaitingFrame() const { return deferred.has_value(); }
  void finishFrame(bool succeeded, const std::string& error);

 private:
  CommandServer server;
  std::optional<LiveRequest> deferred;
  LevelJson deferredResult;
};

#endif  // !SCENIC_LIVE_REQUESTS_H
