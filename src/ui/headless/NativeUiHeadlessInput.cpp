#include "NativeUiHeadlessState.h"

#include "primec/ui/NativeUi.h"
#include "primec/ui/NativeUiHeadless.h"

#include <initializer_list>
#include <string>
#include <utility>

// The pointer and key events of the headless backend (PS_UI_FEATURE_INPUT_EVENTS): the watch table, the event
// fields, the scripted actions and how a text view keeps the keys it uses.

namespace primec::ui::headless {
void recordScenario(const std::string &entry);
} // namespace primec::ui::headless

namespace primec::ui::headless::detail {
namespace {

constexpr int32_t AllWatchBits =
    PS_UI_WATCH_HOVER | PS_UI_WATCH_MOUSE_DOWN | PS_UI_WATCH_MOUSE_UP | PS_UI_WATCH_KEY_DOWN | PS_UI_WATCH_KEY_UP;

int32_t watchBits(uint64_t widget) {
  const auto found = state().watches.find(widget);
  return found != state().watches.end() ? found->second : 0;
}

void appendUtf8(std::string &out, int32_t codePoint) {
  const auto c = static_cast<uint32_t>(codePoint);
  if (c < 0x80) {
    out += static_cast<char>(c);
  } else if (c < 0x800) {
    out += static_cast<char>(0xC0 | (c >> 6));
    out += static_cast<char>(0x80 | (c & 0x3F));
  } else if (c < 0x10000) {
    out += static_cast<char>(0xE0 | (c >> 12));
    out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (c & 0x3F));
  } else if (c <= 0x10FFFF) {
    out += static_cast<char>(0xF0 | (c >> 18));
    out += static_cast<char>(0x80 | ((c >> 12) & 0x3F));
    out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (c & 0x3F));
  }
}

// What a text view does with a key down: the text it types ("" for none), and whether it keeps the key. It reports
// Escape and the keys with Command; it types characters, Enter and Tab without Control and Command, and keeps the
// other keys (editing and moving keys, Control combinations) without a change the backend models.
bool textViewKeeps(int32_t key, int32_t modifiers, std::string &typed) {
  typed.clear();
  if (key == PS_UI_KEY_ESCAPE || (modifiers & PS_UI_MOD_META) != 0) {
    return false;
  }
  if ((modifiers & PS_UI_MOD_CONTROL) == 0) {
    if (key >= 0) {
      appendUtf8(typed, key);
    } else if (key == PS_UI_KEY_ENTER) {
      typed = "\n";
    } else if (key == PS_UI_KEY_TAB) {
      typed = "\t";
    }
  }
  return true;
}

void pushInput(const std::string &entry, int32_t kind, uint64_t widget, InputFields fields) {
  recordScenario(entry);
  Action a;
  a.kind = Action::Kind::Input;
  a.command = kind;
  a.handle = widget;
  a.input = fields;
  state().actions.push_back(std::move(a));
}

std::string joinedFields(uint64_t widget, std::initializer_list<int32_t> values) {
  std::string out = std::to_string(widget);
  for (const int32_t value : values) {
    out += ":" + std::to_string(value);
  }
  return out;
}

void pushPointer(const char *name, int32_t kind, uint64_t widget, int32_t button, int32_t x, int32_t y,
                 int32_t modifiers, int32_t clickCount) {
  InputFields fields;
  fields.button = button;
  fields.x = x;
  fields.y = y;
  fields.modifiers = modifiers;
  fields.clickCount = clickCount;
  const bool hover = kind == PS_UI_EVENT_POINTER_ENTER || kind == PS_UI_EVENT_POINTER_LEAVE;
  const std::string entry = std::string(name) + ":" +
                            (hover ? joinedFields(widget, {x, y, modifiers})
                                   : joinedFields(widget, {button, x, y, modifiers, clickCount}));
  pushInput(entry, kind, widget, fields);
}

} // namespace

bool deliverInputAction(const Action &action) {
  State &s = state();
  const uint64_t widget = action.handle;
  TextView *view = textView(widget);
  if (view == nullptr && control(widget) == nullptr) {
    return false;
  }
  const int32_t bits = watchBits(widget);
  const uint64_t window = view != nullptr ? view->window : windowOfControl(widget);
  auto deliverIf = [&](int32_t bit) {
    if ((bits & bit) == 0) {
      return false;
    }
    setEvent(action.command, window, widget);
    s.eventInput = action.input;
    return true;
  };
  const int32_t key = action.input.key;
  switch (action.command) {
  case PS_UI_EVENT_POINTER_ENTER:
  case PS_UI_EVENT_POINTER_LEAVE:
    return deliverIf(PS_UI_WATCH_HOVER);
  case PS_UI_EVENT_MOUSE_DOWN:
    return deliverIf(PS_UI_WATCH_MOUSE_DOWN);
  case PS_UI_EVENT_MOUSE_UP:
    return deliverIf(PS_UI_WATCH_MOUSE_UP);
  case PS_UI_EVENT_KEY_DOWN: {
    if (view == nullptr) {
      return deliverIf(PS_UI_WATCH_KEY_DOWN);
    }
    std::string typed;
    if (!textViewKeeps(key, action.input.modifiers, typed)) {
      s.keysPassed.insert({widget, key});
      return deliverIf(PS_UI_WATCH_KEY_DOWN);
    }
    if (typed.empty()) {
      return false;
    }
    view->text += typed;
    view->selectionStart = view->selectionEnd = view->text.size();
    view->modified = true;
    setEvent(PS_UI_EVENT_TEXT_CHANGED, view->window, widget);
    return true;
  }
  case PS_UI_EVENT_KEY_UP:
    if (view != nullptr && s.keysPassed.erase({widget, key}) == 0) {
      return false;
    }
    return deliverIf(PS_UI_WATCH_KEY_UP);
  default:
    return false;
  }
}

} // namespace primec::ui::headless::detail

namespace primec::ui::headless {
using namespace detail;

void pushPointerEnter(uint64_t widget, int32_t x, int32_t y, int32_t modifiers) {
  pushPointer("enter", PS_UI_EVENT_POINTER_ENTER, widget, PS_UI_BUTTON_NONE, x, y, modifiers, 0);
}

void pushPointerLeave(uint64_t widget, int32_t x, int32_t y, int32_t modifiers) {
  pushPointer("leave", PS_UI_EVENT_POINTER_LEAVE, widget, PS_UI_BUTTON_NONE, x, y, modifiers, 0);
}

void pushMouseDown(uint64_t widget, int32_t button, int32_t x, int32_t y, int32_t modifiers, int32_t clickCount) {
  pushPointer("down", PS_UI_EVENT_MOUSE_DOWN, widget, button, x, y, modifiers, clickCount);
}

void pushMouseUp(uint64_t widget, int32_t button, int32_t x, int32_t y, int32_t modifiers, int32_t clickCount) {
  pushPointer("up", PS_UI_EVENT_MOUSE_UP, widget, button, x, y, modifiers, clickCount);
}

void pushKeyDown(uint64_t widget, int32_t key, int32_t modifiers, bool repeat) {
  InputFields fields;
  fields.key = key;
  fields.modifiers = modifiers;
  fields.repeat = repeat;
  pushInput("keydown:" + joinedFields(widget, {key, modifiers, repeat ? 1 : 0}), PS_UI_EVENT_KEY_DOWN, widget, fields);
}

void pushKeyUp(uint64_t widget, int32_t key, int32_t modifiers) {
  InputFields fields;
  fields.key = key;
  fields.modifiers = modifiers;
  pushInput("keyup:" + joinedFields(widget, {key, modifiers}), PS_UI_EVENT_KEY_UP, widget, fields);
}

int32_t watchedEvents(uint64_t widget) { return watchBits(widget); }

} // namespace primec::ui::headless

using namespace primec::ui::headless::detail;

namespace {
int32_t loggedField(const char *call, int32_t value) { return logged(call, value, std::to_string(value)); }
// The fields of the current event (all "none" before ps_ui_init or from another thread).
InputFields fields() { return usable() ? state().eventInput : InputFields{}; }
} // namespace

extern "C" {

bool ps_ui_supports(int32_t feature) {
  return loggedFlag("ps_ui_supports(" + std::to_string(feature) + ")",
                    feature == PS_UI_FEATURE_INPUT_EVENTS || feature == PS_UI_FEATURE_FIRST_FRAME);
}

// The headless "screen" draws a window as soon as it is shown: the first frame comes at the first wait after the
// first ps_ui_window_show, ahead of the scripted actions, or where the scenario put it (pushFirstFrame).
bool ps_ui_watch_first_frame(void) {
  State &s = state();
  const bool ok = usable() && !s.firstFrameDelivered;
  if (ok) {
    s.firstFrameWatched = true;
  }
  return loggedFlag("ps_ui_watch_first_frame()", ok);
}

bool ps_ui_widget_watch(uint64_t widget, int32_t mask) {
  const std::string call = "ps_ui_widget_watch(" + std::to_string(widget) + ", " + std::to_string(mask) + ")";
  const bool known = usable() && (textView(widget) != nullptr || control(widget) != nullptr);
  if (!known || (mask & ~AllWatchBits) != 0) {
    return loggedFlag(call, false);
  }
  if (mask == 0) {
    state().watches.erase(widget);
  } else {
    state().watches[widget] = mask;
  }
  return loggedFlag(call, true);
}

int32_t ps_ui_event_button(void) { return loggedField("ps_ui_event_button()", fields().button); }
int32_t ps_ui_event_x(void) { return loggedField("ps_ui_event_x()", fields().x); }
int32_t ps_ui_event_y(void) { return loggedField("ps_ui_event_y()", fields().y); }
int32_t ps_ui_event_modifiers(void) { return loggedField("ps_ui_event_modifiers()", fields().modifiers); }
int32_t ps_ui_event_click_count(void) { return loggedField("ps_ui_event_click_count()", fields().clickCount); }
int32_t ps_ui_event_key(void) { return loggedField("ps_ui_event_key()", fields().key); }
bool ps_ui_event_repeat(void) { return loggedFlag("ps_ui_event_repeat()", fields().repeat); }

} // extern "C"
