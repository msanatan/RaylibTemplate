#ifndef LEVELS_TEST_IDS_H
#define LEVELS_TEST_IDS_H

#include <cstdio>
#include <functional>
#include <regex>
#include <string>

// Level IDs must be UUIDs, which make tests hard to read. Tests name objects
// instead: TestId("door") is a fixed UUID for that name, and WithTestIds
// replaces every "@door" inside a JSON literal with it.
inline std::string TestId(const std::string& name) {
  char id[37];
  std::snprintf(
      id, sizeof(id), "00000000-0000-4000-8000-%012llx",
      (unsigned long long)(std::hash<std::string>()(name) & 0xFFFFFFFFFFFFULL));
  return id;
}

inline std::string WithTestIds(const std::string& text) {
  static const std::regex name("@([A-Za-z0-9_]+)");
  std::string result;
  auto end = text.cbegin();
  for (std::sregex_iterator match(text.begin(), text.end(), name), last;
       match != last; ++match) {
    result.append(end, (*match)[0].first);
    result += TestId((*match)[1].str());
    end = (*match)[0].second;
  }
  result.append(end, text.cend());
  return result;
}

#endif  // !LEVELS_TEST_IDS_H
