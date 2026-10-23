#pragma once

// Titles of the "recent files" menu items, shared by the platform backends and the headless backend so
// `headless::menuBarOutline()` shows what the platform menu holds.

#include <string>
#include <vector>

namespace primec::ui {

inline std::string recentFileName(const std::string &path) {
  const size_t slash = path.find_last_of('/');
  return slash == std::string::npos ? path : path.substr(slash + 1);
}

// The menu title of the recent file at `index`: its file name, with the name of its folder added when
// another recent file has the same name.
inline std::string recentFileTitle(const std::vector<std::string> &paths, size_t index) {
  const std::string name = recentFileName(paths[index]);
  bool clash = false;
  for (size_t i = 0; i < paths.size(); ++i) {
    clash = clash || (i != index && recentFileName(paths[i]) == name);
  }
  if (!clash) {
    return name;
  }
  const std::string folder = paths[index].substr(0, paths[index].size() - name.size());
  const std::string trimmed = folder.size() > 1 ? folder.substr(0, folder.size() - 1) : folder;
  return name + " \xE2\x80\x94 " + recentFileName(trimmed);
}

} // namespace primec::ui
