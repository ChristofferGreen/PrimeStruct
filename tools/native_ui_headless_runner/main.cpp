// Runs a PrimeStruct program against the headless native UI backend.
//
//   native_ui_headless_runner program.prime [--command N] [--type TEXT]
//       [--close-window HANDLE] [--quit] [--open-answer PATH]
//       [--save-answer PATH] [--alert BUTTON] [--log]
//
// Scripted user actions are replayed in the order given. The program's exit
// code becomes the runner's; --log prints every recorded ABI call afterwards.

#include "primec/embed/ScriptEngine.h"
#include "primec/ui/NativeUiBindings.h"
#include "primec/ui/NativeUiHeadless.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace headless = primec::ui::headless;

int main(int argc, char **argv) {
  if (argc < 2) {
    std::fputs("usage: native_ui_headless_runner program.prime [actions] [--log]\n", stderr);
    return 64;
  }
  headless::reset();
  bool printLog = false;
  for (int i = 2; i < argc; ++i) {
    const std::string arg = argv[i];
    const bool hasValue = i + 1 < argc;
    if (arg == "--command" && hasValue) {
      headless::pushCommand(std::atoi(argv[++i]));
    } else if (arg == "--type" && hasValue) {
      headless::pushTypeText(0, argv[++i]);
    } else if (arg == "--close-window" && hasValue) {
      headless::pushCloseWindow(static_cast<uint64_t>(std::strtoull(argv[++i], nullptr, 10)));
    } else if (arg == "--quit") {
      headless::pushQuit();
    } else if (arg == "--open-answer" && hasValue) {
      headless::pushOpenPanelAnswer(argv[++i]);
    } else if (arg == "--save-answer" && hasValue) {
      headless::pushSavePanelAnswer(argv[++i]);
    } else if (arg == "--alert" && hasValue) {
      headless::pushAlertAnswer(std::atoi(argv[++i]));
    } else if (arg == "--log") {
      printLog = true;
    } else {
      std::fprintf(stderr, "unknown or incomplete option: %s\n", arg.c_str());
      return 64;
    }
  }

  primec::embed::ScriptEngine engine;
  primec::ui::bindNativeUi(engine);
  const auto script = engine.compileFile(argv[1]);
  if (!script.valid()) {
    std::fputs(script.diagnostics().c_str(), stderr);
    return 2;
  }
  const auto result = script.run();
  if (printLog) {
    for (const auto &line : headless::callLog()) {
      std::puts(line.c_str());
    }
  }
  if (!result.ok) {
    std::fputs(result.diagnostics.c_str(), stderr);
    return 3;
  }
  return result.exitCode;
}
