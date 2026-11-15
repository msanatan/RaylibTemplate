#ifndef GBM_SCENE_MANAGER_H
#define GBM_SCENE_MANAGER_H

#include <functional>
#include <memory>
#include <vector>

#include "core/scene.h"

// Only the top scene updates. Changes are applied after that update, and the incoming scene is built
// after the outgoing one is destroyed, so a scene can replace itself and two levels never share memory.
class SceneManager
{
public:
  template <typename T, typename... Args>
  void push(Args... args)
  {
    pending.push_back({Change::Push, factory<T>(args...)});
  }

  template <typename T, typename... Args>
  void replace(Args... args)
  {
    pending.push_back({Change::Pop, factory<T>(args...)});
  }

  template <typename T, typename... Args>
  void reset(Args... args)
  {
    pending.push_back({Change::Clear, factory<T>(args...)});
  }

  void pop();
  void quit();

  bool isDone() const;
  void update(float deltaTime);
  void draw() const;

private:
  enum class Change
  {
    Push,
    Pop,
    Clear
  };

  using Factory = std::function<std::unique_ptr<Scene>()>;

  struct PendingChange
  {
    Change change;
    Factory create;
  };

  template <typename T, typename... Args>
  Factory factory(Args... args)
  {
    return [this, args...]() { return std::make_unique<T>(*this, args...); };
  }

  void applyPending();

  std::vector<std::unique_ptr<Scene>> stack;
  std::vector<PendingChange> pending;
};

#endif // !GBM_SCENE_MANAGER_H
