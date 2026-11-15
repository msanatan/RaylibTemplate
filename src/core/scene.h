#ifndef GBM_SCENE_H
#define GBM_SCENE_H

#include <string>

class SceneManager;

class Scene
{
public:
  explicit Scene(SceneManager &scenes) : scenes(scenes) {}
  virtual ~Scene() = default;
  Scene(const Scene &) = delete;
  Scene &operator=(const Scene &) = delete;

  virtual const char *name() const = 0;
  virtual void update(float deltaTime) = 0;
  virtual void draw() const = 0;

  virtual bool isOverlay() const { return false; }

  // Read by the playtest `dump` and `expect` commands
  virtual std::string debugState() const { return ""; }

  // Must fit on one line. Handed back after an asset reload or a `--session` restart.
  virtual std::string saveState() const { return ""; }
  virtual void restoreState(const std::string &) {}

protected:
  SceneManager &scenes;
};

#endif // !GBM_SCENE_H
