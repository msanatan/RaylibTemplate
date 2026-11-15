#include "core/scene_manager.h"

#include <sstream>

namespace
{
std::unique_ptr<Scene> CreateWithState(const std::function<std::unique_ptr<Scene>()> &create, const std::string &state)
{
  std::unique_ptr<Scene> scene = create();
  if (!state.empty())
  {
    scene->restoreState(state);
  }
  return scene;
}
} // namespace

void SceneManager::pop()
{
  pending.push_back({Change::Pop, nullptr, ""});
}

void SceneManager::quit()
{
  pending.push_back({Change::Quit, nullptr, ""});
}

bool SceneManager::pushNamed(const std::string &name, const std::string &state)
{
  const auto scene = registered.find(name);
  if (scene == registered.end())
  {
    return false;
  }
  pending.push_back({Change::Push, scene->second, state});
  return true;
}

std::string SceneManager::saveSession() const
{
  std::string session;
  for (const Entry &entry : stack)
  {
    session += std::string(entry.scene->name()) + " " + entry.scene->saveState() + "\n";
  }
  return session;
}

bool SceneManager::restoreSession(const std::string &session)
{
  std::vector<std::pair<std::string, std::string>> scenes;
  std::istringstream lines(session);
  std::string line;
  while (std::getline(lines, line))
  {
    const size_t space = line.find(' ');
    const std::string name = line.substr(0, space);
    if (registered.count(name) == 0)
    {
      return false;
    }
    scenes.push_back({name, space == std::string::npos ? "" : line.substr(space + 1)});
  }

  for (const auto &[name, state] : scenes)
  {
    pushNamed(name, state);
  }
  return !scenes.empty();
}

void SceneManager::reload()
{
  for (Entry &entry : stack)
  {
    const std::string state = entry.scene->saveState();
    entry.scene.reset();
    entry.scene = CreateWithState(entry.create, state);
  }
}

bool SceneManager::isDone() const
{
  return quitting || (stack.empty() && pending.empty());
}

void SceneManager::update(float deltaTime)
{
  if (!stack.empty())
  {
    stack.back().scene->update(deltaTime);
  }
  applyPending();
}

void SceneManager::draw() const
{
  size_t first = stack.size();
  while (first > 0 && stack[--first].scene->isOverlay())
  {
  }
  for (size_t i = first; i < stack.size(); i++)
  {
    stack[i].scene->draw();
  }
}

std::string SceneManager::describe() const
{
  std::string description;
  for (size_t i = 0; i < stack.size(); i++)
  {
    description += "scene " + std::to_string(i) + ": " + stack[i].scene->name() + " " + stack[i].scene->debugState() + "\n";
  }
  return description;
}

std::string SceneManager::stackNames() const
{
  std::string names;
  for (const Entry &entry : stack)
  {
    names += (names.empty() ? "" : " > ") + std::string(entry.scene->name());
  }
  return names;
}

void SceneManager::applyPending()
{
  std::vector<PendingChange> changes;
  changes.swap(pending);

  for (const PendingChange &change : changes)
  {
    // Quitting leaves the stack alone so the session can still be saved on the way out
    quitting = quitting || change.change == Change::Quit;
    if (change.change == Change::Pop && !stack.empty())
    {
      stack.pop_back();
    }
    while (change.change == Change::Clear && !stack.empty())
    {
      stack.pop_back();
    }
    if (change.create)
    {
      stack.push_back({CreateWithState(change.create, change.state), change.create});
    }
  }
}
