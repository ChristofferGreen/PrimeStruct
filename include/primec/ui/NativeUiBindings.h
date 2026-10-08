#pragma once

#include "primec/embed/Script.h"
#include "primec/embed/ScriptEngine.h"

#include <string>
#include <vector>

namespace primec::ui {

// Binds every function of the native UI C ABI (include/primec/ui/NativeUi.h)
// under its own name, so a script declares it with `[host]`:
//
//   [host return<bool>]
//   ps_ui_init([string] appName) {
//   }
//
// Handles are `u64`, flags are `bool`, text is `string`, everything else `i32`.
// Whichever backend is linked answers the calls (the headless one in tests).
void bindNativeUi(embed::ScriptEngine &engine);
// Same bindings for one compiled script.
void bindNativeUi(embed::Script &script);

// The bound names with their signatures rendered like
// "ps_ui_window_show(u64) -> bool", in a fixed order.
std::vector<std::string> nativeUiBindingSignatures();

} // namespace primec::ui
