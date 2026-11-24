#pragma once

// Shortcut text of the native UI ABI (`ps_ui_menu_add_item`), parsed once for every backend. A shortcut is
// modifiers and a key joined by `+`, such as "cmd+s" or "cmd+shift+s"; "" is no shortcut. The backends map the parsed
// value to what their platform has (AppKit: NSEventModifierFlags and a key equivalent); the headless backend only
// checks it. docs/NativeUiPlan.md section 4 is the rule.

#include <cctype>
#include <cstddef>
#include <string>

namespace primec::ui {

struct ParsedShortcut {
  // The key, lower case: one printable ASCII character other than `+` ("s", ".", "3"), or the name of a key:
  // return (also spelled enter), tab, escape (esc), space, delete, backspace, left, right, up, down, home, end,
  // pageup, pagedown, f1 to f12. Empty for "" (no shortcut).
  std::string key;
  // `cmd`: Command on macOS and Ctrl elsewhere, so each backend maps it.
  bool primary = false;
  bool shift = false;
  // `alt`, also spelled `opt`.
  bool alt = false;
  // `ctrl`: the Control key itself, on every platform.
  bool control = false;
  // False for text that is not a shortcut: an unknown modifier or key, a modifier with no key after it, an empty
  // part (`cmd++s`), or a key that is not last. "" is valid and has no key.
  bool valid = false;
};

namespace detail {

inline std::string lowerAscii(std::string text) {
  for (char &c : text) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return text;
}

// The canonical name of a named key, or "" when `name` (lower case) is not one.
inline std::string namedShortcutKey(const std::string &name) {
  static const char *const names[] = {"return",
                                      "tab",
                                      "escape",
                                      "space",
                                      "delete",
                                      "backspace",
                                      "left",
                                      "right",
                                      "up",
                                      "down",
                                      "home",
                                      "end",
                                      "pageup",
                                      "pagedown"};
  for (const char *candidate : names) {
    if (name == candidate) {
      return name;
    }
  }
  if (name == "enter") {
    return "return";
  }
  if (name == "esc") {
    return "escape";
  }
  if (name.size() >= 2 && name.size() <= 3 && name[0] == 'f') {
    int number = 0;
    for (std::size_t i = 1; i < name.size(); ++i) {
      if (name[i] < '0' || name[i] > '9') {
        return {};
      }
      number = number * 10 + (name[i] - '0');
    }
    if (number >= 1 && number <= 12 && !(name.size() == 3 && name[1] == '0')) {
      return name;
    }
  }
  return {};
}

} // namespace detail

// Parses `text` (null is ""). Modifiers and the key are matched without regard to case.
inline ParsedShortcut parseShortcut(const char *text) {
  ParsedShortcut result;
  const std::string all = text != nullptr ? text : "";
  if (all.empty()) {
    result.valid = true;
    return result;
  }
  std::size_t start = 0;
  while (true) {
    std::size_t plus = all.find('+', start);
    const bool last = plus == std::string::npos;
    if (last) {
      plus = all.size();
    }
    const std::string part = detail::lowerAscii(all.substr(start, plus - start));
    if (part.empty()) {
      return ParsedShortcut{};
    }
    if (!last) {
      if (part == "cmd") {
        result.primary = true;
      } else if (part == "shift") {
        result.shift = true;
      } else if (part == "alt" || part == "opt") {
        result.alt = true;
      } else if (part == "ctrl") {
        result.control = true;
      } else {
        return ParsedShortcut{};
      }
      start = plus + 1;
      continue;
    }
    if (part.size() == 1 && part[0] > ' ' && part[0] < 127) {
      result.key = part;
    } else {
      result.key = detail::namedShortcutKey(part);
    }
    if (result.key.empty()) {
      return ParsedShortcut{};
    }
    result.valid = true;
    return result;
  }
}

} // namespace primec::ui
