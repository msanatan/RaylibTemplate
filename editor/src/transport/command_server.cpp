#include "transport/command_server.h"

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>

#if defined(_WIN32)
// clang-format off
#include <winsock2.h>
#include <afunix.h>
#include <process.h>
// clang-format on
#else
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif

namespace {
namespace fs = std::filesystem;

const size_t maximumMessageBytes = size_t{16} * 1024 * 1024;
const auto responseTimeout = std::chrono::seconds(60);

#if defined(_WIN32)
void StartSockets() {
  static const bool started = [] {
    WSADATA data;
    return WSAStartup(MAKEWORD(2, 2), &data) == 0;
  }();
  (void)started;
}
void CloseSocket(int socket) { closesocket((SOCKET)socket); }
long long ProcessId() { return _getpid(); }
bool WaitReadable(int socket, int milliseconds) {
  WSAPOLLFD descriptor = {(SOCKET)socket, POLLRDNORM, 0};
  return WSAPoll(&descriptor, 1, milliseconds) > 0;
}
#else
void StartSockets() {}
void CloseSocket(int socket) { close(socket); }
long long ProcessId() { return getpid(); }
bool WaitReadable(int socket, int milliseconds) {
  pollfd descriptor = {socket, POLLIN, 0};
  return poll(&descriptor, 1, milliseconds) > 0;
}
#endif

std::string RandomHex(size_t length) {
  std::random_device random;
  static const char* const digits = "0123456789abcdef";
  std::string text;
  for (size_t i = 0; i < length; i++) {
    text += digits[random() % 16];
  }
  return text;
}

bool FillAddress(const std::string& path, sockaddr_un& address) {
  address = {};
  address.sun_family = AF_UNIX;
  if (path.size() >= sizeof(address.sun_path)) {
    return false;
  }
  std::memcpy(address.sun_path, path.c_str(), path.size() + 1);
  return true;
}

// A peer that disconnects before reading its answer must surface as a failed
// send. By default it raises SIGPIPE instead, which would end the editor and
// lose unsaved work.
#if defined(MSG_NOSIGNAL)
const int sendFlags = MSG_NOSIGNAL;
#else
const int sendFlags = 0;
#endif

void DisableBrokenPipeSignal([[maybe_unused]] int socket) {
#if defined(SO_NOSIGPIPE)
  const int enabled = 1;
  setsockopt(socket, SOL_SOCKET, SO_NOSIGPIPE, &enabled, sizeof(enabled));
#endif
}

bool SendAll(int socket, const std::string& text) {
  size_t sent = 0;
  while (sent < text.size()) {
    const auto count =
        send(socket, text.data() + sent, (int)(text.size() - sent), sendFlags);
    if (count <= 0) {
      return false;
    }
    sent += (size_t)count;
  }
  return true;
}

bool ReceiveLine(int socket, std::string& line) {
  char buffer[4096];
  while (line.size() < maximumMessageBytes) {
    const auto count = recv(socket, buffer, sizeof(buffer), 0);
    if (count <= 0) {
      return false;
    }
    line.append(buffer, (size_t)count);
    if (line.back() == '\n') {
      line.pop_back();
      return true;
    }
  }
  return false;
}

std::optional<SessionInfo> ReadSession(const fs::path& file) {
  std::ifstream stream(file);
  const LevelJson json = LevelJson::parse(stream, nullptr, false);
  if (!json.is_object() || !json.contains("id") || !json.contains("socket")) {
    return std::nullopt;
  }
  SessionInfo info;
  info.id = json.value("id", "");
  info.project = json.value("project", "");
  info.socket = json.value("socket", "");
  info.pid = json.value("pid", 0LL);
  return info;
}

int Connect(const SessionInfo& session) {
  StartSockets();
  sockaddr_un address;
  if (!FillAddress(session.socket, address)) {
    return -1;
  }
  const int connection = (int)socket(AF_UNIX, SOCK_STREAM, 0);
  if (connection < 0) {
    return -1;
  }
  DisableBrokenPipeSignal(connection);
  if (connect(connection, (const sockaddr*)&address, sizeof(address)) != 0) {
    CloseSocket(connection);
    return -1;
  }
  return connection;
}
}  // namespace

std::string SessionsDirectory() {
  const char* overridden = std::getenv("SCENIC_HOME");
  if (overridden != nullptr && *overridden != '\0') {
    return (fs::path(overridden) / "sessions").string();
  }
  const char* home = std::getenv("HOME");
  if (home == nullptr) {
    home = std::getenv("USERPROFILE");
  }
  return (fs::path(home != nullptr ? home : ".") / ".scenic" / "sessions")
      .string();
}

std::vector<SessionInfo> ListSessions() {
  std::vector<SessionInfo> sessions;
  std::error_code error;
  for (const fs::directory_entry& entry :
       fs::directory_iterator(SessionsDirectory(), error)) {
    if (entry.path().extension() != ".json") {
      continue;
    }
    const std::optional<SessionInfo> info = ReadSession(entry.path());
    if (!info.has_value()) {
      fs::remove(entry.path(), error);
      continue;
    }
    const int connection = Connect(*info);
    if (connection < 0) {
      fs::remove(entry.path(), error);
      fs::remove(info->socket, error);
      continue;
    }
    CloseSocket(connection);
    sessions.push_back(*info);
  }
  return sessions;
}

std::optional<SessionInfo> FindSession(const std::string& id) {
  for (const SessionInfo& session : ListSessions()) {
    if (session.id == id) {
      return session;
    }
  }
  return std::nullopt;
}

std::optional<std::string> SendToSession(const SessionInfo& session,
                                         const std::string& request,
                                         std::string& error) {
  const int connection = Connect(session);
  if (connection < 0) {
    error = "Could not connect to editor session \"" + session.id + "\"";
    return std::nullopt;
  }
  std::string response;
  const bool received =
      SendAll(connection, request + "\n") && ReceiveLine(connection, response);
  CloseSocket(connection);
  if (!received) {
    error = "Editor session \"" + session.id +
            "\" closed the connection without answering";
    return std::nullopt;
  }
  return response;
}

CommandServer::~CommandServer() { stop(); }

bool CommandServer::start(const std::string& project, std::string& error) {
  StartSockets();
  std::error_code filesystemError;
  const fs::path directory = SessionsDirectory();
  fs::create_directories(directory, filesystemError);
  fs::permissions(directory.parent_path(), fs::perms::owner_all,
                  filesystemError);
  fs::permissions(directory, fs::perms::owner_all, filesystemError);

  info.id = RandomHex(8);
  info.project = project;
  info.pid = ProcessId();
  info.socket = (directory / (info.id + ".sock")).string();
  sessionFile = (directory / (info.id + ".json")).string();

  sockaddr_un address;
  if (!FillAddress(info.socket, address)) {
    error = "The session socket path is too long: " + info.socket +
            ". Set SCENIC_HOME to a shorter directory";
    return false;
  }
  listener = (int)socket(AF_UNIX, SOCK_STREAM, 0);
  if (listener < 0 ||
      bind(listener, (const sockaddr*)&address, sizeof(address)) != 0 ||
      listen(listener, 8) != 0) {
    error = "Could not listen on " + info.socket;
    if (listener >= 0) {
      CloseSocket(listener);
      listener = -1;
    }
    return false;
  }
  fs::permissions(info.socket, fs::perms::owner_read | fs::perms::owner_write,
                  filesystemError);

  {
    std::ofstream file(sessionFile);
    file << LevelJson{{"id", info.id},
                      {"project", info.project},
                      {"socket", info.socket},
                      {"pid", info.pid}}
                .dump(2)
         << "\n";
  }
  fs::permissions(sessionFile, fs::perms::owner_read | fs::perms::owner_write,
                  filesystemError);

  running = true;
  thread = std::thread([this] { serve(); });
  return true;
}

void CommandServer::stop() {
  if (!running.exchange(false)) {
    return;
  }
  thread.join();
  CloseSocket(listener);
  listener = -1;

  for (LiveRequest& request : queue) {
    request.response->set_value("");
  }
  queue.clear();

  std::error_code error;
  fs::remove(sessionFile, error);
  fs::remove(info.socket, error);
}

std::optional<LiveRequest> CommandServer::next() {
  const std::lock_guard<std::mutex> lock(mutex);
  if (queue.empty()) {
    return std::nullopt;
  }
  LiveRequest request = queue.front();
  queue.erase(queue.begin());
  return request;
}

void CommandServer::serve() {
  while (running) {
    if (!WaitReadable(listener, 100)) {
      continue;
    }
    const int connection = (int)accept(listener, nullptr, nullptr);
    if (connection >= 0) {
      DisableBrokenPipeSignal(connection);
      try {
        handleConnection(connection);
      } catch (const LevelJson::exception& error) {
        SendAll(connection, LevelJson{{"ok", false},
                                      {"code", "invalid_request"},
                                      {"message", error.what()},
                                      {"exit_code", 2}}
                                    .dump() +
                                "\n");
      }
      CloseSocket(connection);
    }
  }
}

void CommandServer::handleConnection(int connection) {
  std::string line;
  if (!WaitReadable(connection, 5000) || !ReceiveLine(connection, line)) {
    return;
  }
  const LevelJson request = LevelJson::parse(line, nullptr, false);
  if (!request.is_object()) {
    SendAll(connection,
            LevelJson{{"ok", false},
                      {"code", "invalid_request"},
                      {"message", "A request must be a JSON object"},
                      {"exit_code", 2}}
                    .dump() +
                "\n");
    return;
  }

  LiveRequest live = {request, std::make_shared<std::promise<std::string>>()};
  std::future<std::string> response = live.response->get_future();
  {
    const std::lock_guard<std::mutex> lock(mutex);
    queue.push_back(live);
  }
  // Polled so that stopping the server is never stuck behind a request
  for (auto waited = std::chrono::milliseconds(0);
       running && waited < responseTimeout;
       waited += std::chrono::milliseconds(50)) {
    if (response.wait_for(std::chrono::milliseconds(50)) ==
        std::future_status::ready) {
      const std::string text = response.get();
      if (!text.empty()) {
        SendAll(connection, text + "\n");
      }
      return;
    }
  }
}
