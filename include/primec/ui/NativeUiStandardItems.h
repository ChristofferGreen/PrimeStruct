#pragma once

// Titles and shortcuts of the standard menu items (PS_UI_STANDARD_*), shared by
// the backends so the headless backend reports exactly what the platform
// backends put in their menus. "cmd" is Command on macOS and Ctrl elsewhere.

#include "primec/ui/NativeUi.h"

namespace primec::ui {

struct StandardItemInfo {
  const char *title;
  const char *shortcut;
};

// {nullptr, nullptr} for an id that is not a standard item.
inline StandardItemInfo standardItemInfo(int32_t standardId) {
  switch (standardId) {
  case PS_UI_STANDARD_UNDO: return {"Undo", "cmd+z"};
  case PS_UI_STANDARD_REDO: return {"Redo", "cmd+shift+z"};
  case PS_UI_STANDARD_CUT: return {"Cut", "cmd+x"};
  case PS_UI_STANDARD_COPY: return {"Copy", "cmd+c"};
  case PS_UI_STANDARD_PASTE: return {"Paste", "cmd+v"};
  case PS_UI_STANDARD_SELECT_ALL: return {"Select All", "cmd+a"};
  case PS_UI_STANDARD_FIND: return {"Find...", "cmd+f"};
  case PS_UI_STANDARD_QUIT: return {"Quit", "cmd+q"};
  case PS_UI_STANDARD_ABOUT: return {"About", ""};
  case PS_UI_STANDARD_HIDE: return {"Hide", "cmd+h"};
  case PS_UI_STANDARD_HIDE_OTHERS: return {"Hide Others", "cmd+alt+h"};
  case PS_UI_STANDARD_MINIMIZE: return {"Minimize", "cmd+m"};
  case PS_UI_STANDARD_ZOOM: return {"Zoom", ""};
  case PS_UI_STANDARD_BRING_ALL_TO_FRONT: return {"Bring All to Front", ""};
  case PS_UI_STANDARD_FULL_SCREEN: return {"Enter Full Screen", "cmd+ctrl+f"};
  case PS_UI_STANDARD_HELP: return {"Help", "cmd+?"};
  default: return {nullptr, nullptr};
  }
}

} // namespace primec::ui
