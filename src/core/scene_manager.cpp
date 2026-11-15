#include "core/scene_manager.h"

void SceneManager::pop()
{
  pending.push_back({Change::Pop, nullptr});
}

void SceneManager::quit()
{
  pending.push_back({Change::Clear, nullptr});
}

bool SceneManager::isDone() const
{
  return stack.empty() && pending.empty();
}

void SceneManager::update(float deltaTime)
{
  if (!stack.empty())
  {
    stack.back()->update(deltaTime);
  }
  applyPending();
}

void SceneManager::draw() const
{
  size_t first = stack.size();
  while (first > 0 && stack[--first]->isOverlay())
  {
  }
  for (size_t i = first; i < stack.size(); i++)
  {
    stack[i]->draw();
  }
}

std::string SceneManager::describe() const
{
  std::string description;
  for (size_t i = 0; i < stack.size(); i++)
  {
    description += "scene " + std::to_string(i) + ": " + stack[i]->name() + " " + stack[i]->debugState() + "\n";
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
      stack.push_back(change.create());
    }
  }
}
