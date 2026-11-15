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

class StatefulScene : public Scene
{
public:
  explicit StatefulScene(SceneManager &scenes) : Scene(scenes) {}

  const char *name() const override { return "stateful"; }
  void update(float) override { progress += "!"; }
  void draw() const override {}
  std::string debugState() const override { return progress; }
  std::string saveState() const override { return progress; }
  void restoreState(const std::string &state) override { progress = state; }

private:
  std::string progress = "start";
};

class PlainScene : public Scene
{
public:
  explicit PlainScene(SceneManager &scenes) : Scene(scenes) {}

  const char *name() const override { return "plain"; }
  void update(float) override {}
  void draw() const override {}
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
  CHECK(scenes.stackNames() == "game > pause");
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

TEST_CASE("reload rebuilds every scene in place, bottom first")
{
  Events events;
  SceneManager scenes;
  scenes.push<RecordingScene>(&events, std::string("game"));
  scenes.push<RecordingScene>(&events, std::string("pause"), true);
  Settle(scenes, events);

  scenes.reload();
  CHECK(events == Events{"destroy game", "create game", "destroy pause", "create pause"});
  CHECK(scenes.describe() == "scene 0: game \nscene 1: pause \n");
}

TEST_CASE("reload hands each scene back the state it saved")
{
  SceneManager scenes;
  scenes.push<StatefulScene>();
  scenes.update(0.0f);
  scenes.update(0.0f);
  CHECK(scenes.describe() == "scene 0: stateful start!\n");

  scenes.reload();
  CHECK(scenes.describe() == "scene 0: stateful start!\n");
}

TEST_CASE("quit ends the game after the current update and keeps the stack for saving")
{
  Events events;
  SceneManager scenes;
  scenes.push<RecordingScene>(&events, std::string("menu"));
  Settle(scenes, events);

  scenes.quit();
  CHECK_FALSE(scenes.isDone());
  Settle(scenes, events);
  CHECK(scenes.isDone());
  CHECK(scenes.saveSession() == "menu \n");
}

TEST_CASE("only registered names can be pushed")
{
  SceneManager scenes;
  scenes.registerScene<PlainScene>("plain");

  CHECK(scenes.pushNamed("plain"));
  CHECK_FALSE(scenes.pushNamed("missing"));
  scenes.update(0.0f);
  CHECK(scenes.describe() == "scene 0: plain \n");
}

TEST_CASE("a saved session restores the same stack with each scene's state")
{
  SceneManager played;
  played.push<StatefulScene>();
  played.push<PlainScene>();
  played.update(0.0f);
  const std::string session = played.saveSession();
  CHECK(session == "stateful start\nplain \n");

  SceneManager resumed;
  resumed.registerScene<StatefulScene>("stateful");
  resumed.registerScene<PlainScene>("plain");
  CHECK(resumed.restoreSession(session));
  resumed.update(0.0f);
  CHECK(resumed.saveSession() == session);
}

TEST_CASE("a session naming an unregistered scene is refused and pushes nothing")
{
  SceneManager scenes;
  scenes.registerScene<PlainScene>("plain");

  CHECK_FALSE(scenes.restoreSession("plain \nmissing \n"));
  CHECK(scenes.isDone());
}
