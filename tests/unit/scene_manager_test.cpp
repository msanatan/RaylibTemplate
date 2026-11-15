#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "core/scene_manager.h"

namespace
{
using Events = std::vector<std::string>;

class RecordingScene : public Scene
{
public:
  RecordingScene(SceneManager &scenes, Events *events, std::string label, bool overlay = false)
      : Scene(scenes), events(events), label(std::move(label)), overlay(overlay)
  {
    events->push_back("create " + this->label);
  }

  ~RecordingScene() override { events->push_back("destroy " + label); }

  const char *name() const override { return label.c_str(); }
  void update(float) override { events->push_back("update " + label); }
  void draw() const override { events->push_back("draw " + label); }
  bool isOverlay() const override { return overlay; }

private:
  Events *events;
  std::string label;
  bool overlay;
};

class SelfReplacingScene : public Scene
{
public:
  SelfReplacingScene(SceneManager &scenes, Events *events) : Scene(scenes), events(events) {}

  const char *name() const override { return "self_replacing"; }
  void update(float) override { scenes.replace<RecordingScene>(events, std::string("next")); }
  void draw() const override {}

private:
  Events *events;
};

// Each update applies the changes queued before it, so this settles a freshly queued change
Events Settle(SceneManager &scenes, Events &events)
{
  scenes.update(0.0f);
  Events settled;
  settled.swap(events);
  return settled;
}
} // namespace

TEST_CASE("a pushed scene is created by the next update and updates from the one after")
{
  Events events;
  SceneManager scenes;
  scenes.push<RecordingScene>(&events, std::string("menu"));
  CHECK(events.empty());
  CHECK_FALSE(scenes.isDone());

  CHECK(Settle(scenes, events) == Events{"create menu"});
  CHECK(Settle(scenes, events) == Events{"update menu"});
}

TEST_CASE("only the top scene updates")
{
  Events events;
  SceneManager scenes;
  scenes.push<RecordingScene>(&events, std::string("game"));
  scenes.push<RecordingScene>(&events, std::string("pause"));
  Settle(scenes, events);

  CHECK(Settle(scenes, events) == Events{"update pause"});
}

TEST_CASE("an overlay keeps the scene beneath it on screen")
{
  Events events;
  SceneManager scenes;
  scenes.push<RecordingScene>(&events, std::string("game"));
  scenes.push<RecordingScene>(&events, std::string("pause"), true);
  Settle(scenes, events);

  scenes.draw();
  CHECK(events == Events{"draw game", "draw pause"});
}

TEST_CASE("a non-overlay hides the scene beneath it")
{
  Events events;
  SceneManager scenes;
  scenes.push<RecordingScene>(&events, std::string("game"));
  scenes.push<RecordingScene>(&events, std::string("settings"));
  Settle(scenes, events);

  scenes.draw();
  CHECK(events == Events{"draw settings"});
}

TEST_CASE("pop returns control to the scene beneath")
{
  Events events;
  SceneManager scenes;
  scenes.push<RecordingScene>(&events, std::string("game"));
  scenes.push<RecordingScene>(&events, std::string("pause"));
  Settle(scenes, events);

  scenes.pop();
  CHECK(Settle(scenes, events) == Events{"update pause", "destroy pause"});
  CHECK(Settle(scenes, events) == Events{"update game"});
}

TEST_CASE("replace destroys the old scene before creating the new one")
{
  Events events;
  SceneManager scenes;
  scenes.push<RecordingScene>(&events, std::string("level1"));
  Settle(scenes, events);

  scenes.replace<RecordingScene>(&events, std::string("level2"));
  CHECK(Settle(scenes, events) == Events{"update level1", "destroy level1", "create level2"});
}

TEST_CASE("a scene can replace itself from its own update")
{
  Events events;
  SceneManager scenes;
  scenes.push<SelfReplacingScene>(&events);
  Settle(scenes, events);

  CHECK(Settle(scenes, events) == Events{"create next"});
  CHECK(scenes.describe() == "scene 0: next \n");
}

TEST_CASE("reset clears the whole stack from the top down")
{
  Events events;
  SceneManager scenes;
  scenes.push<RecordingScene>(&events, std::string("game"));
  scenes.push<RecordingScene>(&events, std::string("pause"));
  Settle(scenes, events);

  scenes.reset<RecordingScene>(&events, std::string("menu"));
  CHECK(Settle(scenes, events) == Events{"update pause", "destroy pause", "destroy game", "create menu"});
}

TEST_CASE("quit empties the stack and ends the game")
{
  Events events;
  SceneManager scenes;
  scenes.push<RecordingScene>(&events, std::string("menu"));
  Settle(scenes, events);

  scenes.quit();
  CHECK_FALSE(scenes.isDone());
  Settle(scenes, events);
  CHECK(scenes.isDone());
}
