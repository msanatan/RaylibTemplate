#include "core/scene_manager.h"

void SceneManager::pop()
{
  pending.push_back({Change::Pop, nullptr});
}

void SceneManager::quit()
{
  pending.push_back({Change::Clear, nullptr});
}

void SceneManager::reload()
{
  for (Entry &entry : stack)
  {
    entry.scene.reset();
    entry.scene = entry.create();
  }
}

bool SceneManager::isDone() const
{
  return stack.empty() && pending.empty();
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

void SceneManager::applyPending()
{
  std::vector<PendingChange> changes;
  changes.swap(pending);

  for (const PendingChange &change : changes)
  {
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
      stack.push_back({change.create(), change.create});
    }
  }
}
