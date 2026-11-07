// /std/web (docs/spec/stdlib-reference.md): controls that take handler closures, their generated routes and the HTML
// they write, on the VM and optexe.
#include "primec/embed/Script.h"
#include "primec/embed/ScriptEngine.h"
#include "primec/testing/TestScratch.h"

#include "third_party/doctest.h"

#include <sys/wait.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

using primec::embed::ScriptEngine;

TEST_SUITE_BEGIN("primestruct.backend.web");

namespace {
const std::string Primec = PRIMESTRUCT_PRIMEC_PATH;
const std::string Fixtures = std::string(PRIMESTRUCT_SOURCE_DIR) + "/tests/fixtures/http/";

std::string scratch(const std::string &name) {
  const std::filesystem::path dir = primec::testing::testScratchDir("web");
  std::filesystem::create_directories(dir);
  return (dir / name).string();
}

std::string readFile(const std::string &path) {
  std::ifstream file(path, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

std::string shellQuote(const std::string &text) {
  return "'" + text + "'";
}

int run(const std::string &command) {
  const int status = std::system(command.c_str());
  return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

// Builds the fixture `name`.prime with --emit=optexe; returns the executable path.
std::string buildOptexe(const std::string &name) {
  const std::string exe = scratch(name);
  const std::string log = exe + ".log";
  const int status =
      run(shellQuote(Primec) + " --emit=optexe " + shellQuote(Fixtures + name + ".prime") + " -o " +
          shellQuote(exe) + " > " + shellQuote(log) + " 2>&1");
  REQUIRE_MESSAGE(status == 0, readFile(log));
  return exe;
}
} // namespace

TEST_CASE("web controls call their click closures through generated routes on the VM") {
  ScriptEngine engine;
  const auto script = engine.compileFile(Fixtures + "web_controls.prime");
  REQUIRE_MESSAGE(script.valid(), script.diagnostics());
  const auto result = script.run();
  REQUIRE_MESSAGE(result.ok, result.diagnostics);
  CHECK_MESSAGE(result.exitCode == 0,
                "web_controls case " << result.exitCode
                                     << " failed (tests/fixtures/http/web_controls.prime)");
}

TEST_CASE("web controls call their click closures through generated routes through optexe") {
  const std::string exe = buildOptexe("web_controls");
  const int failed = run(shellQuote(exe));
  CHECK_MESSAGE(failed == 0,
                "web_controls case " << failed
                                     << " failed (tests/fixtures/http/web_controls.prime)");
}

TEST_CASE("a duplicate control id stops the program at setup on the VM") {
  ScriptEngine engine;
  const auto script = engine.compileFile(Fixtures + "web_duplicate_control.prime");
  REQUIRE_MESSAGE(script.valid(), script.diagnostics());
  const auto result = script.run();
  // The panic stops the run with a trap before `main` reaches `return(7i32)`.
  const bool trapped = !result.ok || (result.exitCode != 0 && result.exitCode != 7);
  CHECK_MESSAGE(trapped, "exit code " << result.exitCode);
}

TEST_CASE("a duplicate control id stops the program at setup through optexe") {
  const std::string exe = buildOptexe("web_duplicate_control");
  const std::string errors = exe + ".err";
  const int status = run(shellQuote(exe) + " 2> " + shellQuote(errors));
  CHECK(status != 0);
  CHECK(status != 7);
  const std::string text = readFile(errors);
  CHECK_MESSAGE(text.find("duplicate control id: hello\n") != std::string::npos, text);
}

TEST_CASE("web regions answer the fragment script's requests with fragments on the VM") {
  ScriptEngine engine;
  const auto script = engine.compileFile(Fixtures + "web_fragments.prime");
  REQUIRE_MESSAGE(script.valid(), script.diagnostics());
  const auto result = script.run();
  REQUIRE_MESSAGE(result.ok, result.diagnostics);
  CHECK_MESSAGE(result.exitCode == 0,
                "web_fragments case " << result.exitCode
                                      << " failed (tests/fixtures/http/web_fragments.prime)");
}

TEST_CASE("web regions answer the fragment script's requests with fragments through optexe") {
  const std::string exe = buildOptexe("web_fragments");
  const int failed = run(shellQuote(exe));
  CHECK_MESSAGE(failed == 0,
                "web_fragments case " << failed
                                      << " failed (tests/fixtures/http/web_fragments.prime)");
}

TEST_CASE("a duplicate region id stops the program at setup on the VM") {
  ScriptEngine engine;
  const auto script = engine.compileFile(Fixtures + "web_duplicate_region.prime");
  REQUIRE_MESSAGE(script.valid(), script.diagnostics());
  std::string errors;
  primec::embed::RunOptions options;
  options.output = [&errors](int fd, std::string_view chunk) {
    if (fd == 2) {
      errors.append(chunk);
    }
  };
  const auto result = script.run({}, options);
  const bool trapped = !result.ok || (result.exitCode != 0 && result.exitCode != 7);
  CHECK_MESSAGE(trapped, "exit code " << result.exitCode);
  CHECK_MESSAGE(errors.find("duplicate region id: clicks\n") != std::string::npos, errors);
}

// The script is the fixed asset stdlib/std/web/app.js; stdlib/std/web/fragment_script.prime carries the same bytes
// in a raw literal, which ends at the first "'".
TEST_CASE("the fragment script is a fixed asset within its 5120-byte budget") {
  const std::string webDir = std::string(PRIMESTRUCT_SOURCE_DIR) + "/stdlib/std/web/";
  const std::string asset = readFile(webDir + "app.js");
  REQUIRE(!asset.empty());
  CHECK_MESSAGE(asset.size() <= 5120, "app.js has " << asset.size() << " bytes");
  CHECK(asset.find('\'') == std::string::npos);
  CHECK(asset.find("X-PrimeStruct-Fragment") != std::string::npos);
  CHECK(asset.find("data-ps-target") != std::string::npos);
  const std::string embedded = readFile(webDir + "fragment_script.prime");
  CHECK_MESSAGE(embedded.find("return('" + asset + "'utf8)") != std::string::npos,
                "fragment_script.prime does not hold the bytes of app.js");
}

TEST_CASE("control event handlers decode the fragment script's requests on the VM") {
  ScriptEngine engine;
  const auto script = engine.compileFile(Fixtures + "web_events.prime");
  REQUIRE_MESSAGE(script.valid(), script.diagnostics());
  const auto result = script.run();
  REQUIRE_MESSAGE(result.ok, result.diagnostics);
  CHECK_MESSAGE(result.exitCode == 0,
                "web_events case " << result.exitCode
                                   << " failed (tests/fixtures/http/web_events.prime)");
}

TEST_CASE("control event handlers decode the fragment script's requests through optexe") {
  const std::string exe = buildOptexe("web_events");
  const int failed = run(shellQuote(exe));
  CHECK_MESSAGE(failed == 0,
                "web_events case " << failed << " failed (tests/fixtures/http/web_events.prime)");
}

TEST_CASE("a second handler for one event of a control stops the program at setup on the VM") {
  ScriptEngine engine;
  const auto script = engine.compileFile(Fixtures + "web_duplicate_handler.prime");
  REQUIRE_MESSAGE(script.valid(), script.diagnostics());
  std::string errors;
  primec::embed::RunOptions options;
  options.output = [&errors](int fd, std::string_view chunk) {
    if (fd == 2) {
      errors.append(chunk);
    }
  };
  const auto result = script.run({}, options);
  const bool trapped = !result.ok || (result.exitCode != 0 && result.exitCode != 7);
  CHECK_MESSAGE(trapped, "exit code " << result.exitCode);
  CHECK_MESSAGE(errors.find("duplicate handler: hello mouseover\n") != std::string::npos, errors);
}

// The request format web_events.prime decodes is the one the script writes: the event routes, the form fields of
// PointerEvent and KeyEvent, the named keys with the codes of /std/ui/events Key, the debounce of hovers and key
// events for the focused control only.
TEST_CASE("the fragment script sends each control event in the format the app decodes") {
  const std::string asset =
      readFile(std::string(PRIMESTRUCT_SOURCE_DIR) + "/stdlib/std/web/app.js");
  REQUIRE(!asset.empty());
  for (const char *piece : {"\"/_on/\" + found.getAttribute(\"data-ps-id\") + \"/\" + kind",
                            "data-ps-on",
                            "{button: button, x: Math.round(event.clientX - box.left)",
                            "y: Math.round(event.clientY - box.top)}",
                            "fields.modifiers = (event.shiftKey ? 1 : 0) + (event.ctrlKey ? 2 : 0) "
                            "+ (event.altKey ? 4 : 0) +",
                            "(event.metaKey ? 8 : 0)",
                            "{key: key, repeat: event.repeat ? 1 : 0}",
                            "[0, 2, 1][event.button]",
                            "pointer(found, \"mouseover\", event, -1); }, 100)",
                            "control(document.activeElement, kind)",
                            "[\"mousedown\", \"mouseup\"]",
                            "[\"keydown\", \"keyup\"]",
                            "Enter: -1, Tab: -2, Escape: -3, Backspace: -4, Delete: -5",
                            "ArrowLeft: -6, ArrowRight: -7",
                            "ArrowUp: -8, ArrowDown: -9, Home: -10, End: -11, PageUp: -12, "
                            "PageDown: -13"}) {
    CHECK_MESSAGE(asset.find(piece) != std::string::npos, "app.js lacks: " << piece);
  }
}

TEST_SUITE_END();
