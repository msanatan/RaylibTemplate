#ifndef GBM_SCENE_H
#define GBM_SCENE_H

class SceneManager;

class Scene
{
public:
  explicit Scene(SceneManager &scenes) : scenes(scenes) {}
  virtual ~Scene() = default;
  Scene(const Scene &) = delete;
  Scene &operator=(const Scene &) = delete;

  virtual void update(float deltaTime) = 0;
  virtual void draw() const = 0;

  // Overlays such as a pause menu keep the scene beneath them on screen
  virtual bool isOverlay() const { return false; }

protected:
  SceneManager &scenes;
};

#endif // !GBM_SCENE_H
