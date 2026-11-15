#ifndef GBM_SCENE_MANAGER_H
#define GBM_SCENE_MANAGER_H

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "core/scene.h"

// Only the top scene updates. Changes are applied after that update, and the
// incoming scene is built after the outgoing one is destroyed, so a scene can
// replace itself and two levels never share memory.
class SceneManager {
 public:
  template <typename T, typename... Args>
  void push(Args... args) {
    pending.push_back({Change::Push, factory<T>(args...), ""});
  }

  template <typename T, typename... Args>
  void replace(Args... args) {
    pending.push_back({Change::Pop, factory<T>(args...), ""});
  }

  template <typename T, typename... Args>
  void reset(Args... args) {
    pending.push_back({Change::Clear, factory<T>(args...), ""});
  }

  void pop();
  void quit();

  template <typename T>
  void registerScene(const std::string& name) {
    registered[name] = factory<T>();
  }

  bool pushNamed(const std::string& name, const std::string& state = "");

  // One line per scene, bottom first: its name, then whatever its saveState()
  // returned
  std::string saveSession() const;
  bool restoreSession(const std::string& session);

  // Call between frames, never from inside a scene
  void reload();

  bool isDone() const;
  void update(float deltaTime);
  void draw() const;
  std::string describe() const;
  std::string stackNames() const;

 private:
  enum class Change { Push, Pop, Clear, Quit };

  using Factory = std::function<std::unique_ptr<Scene>()>;

  struct PendingChange {
    Change change;
    Factory create;
    std::string state;
  };

  struct Entry {
    std::unique_ptr<Scene> scene;
    Factory create;
  };

  template <typename T, typename... Args>
  Factory factory(Args... args) {
    return [this, args...]() { return std::make_unique<T>(*this, args...); };
  }

  void applyPending();

  std::map<std::string, Factory> registered;
  std::vector<Entry> stack;
  std::vector<PendingChange> pending;
  bool quitting = false;
};

#endif  // !GBM_SCENE_MANAGER_H
