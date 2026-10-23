#include "primec/embed/ScriptEngine.h"
#include "primec/testing/TestScratch.h"
#include "primec/ui/NativeUi.h"
#include "primec/ui/NativeUiBindings.h"
#include "primec/ui/NativeUiHeadless.h"

#include "tests/unit/ui/CompiledEditorMirror.h"
#include "primec/ui/SyntaxHighlight.h"

#include "third_party/doctest.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

// Highlighting part of the editor acceptance gate: the editor opens
// one sample per language and the headless backend shows the style runs.

namespace headless = primec::ui::headless;
namespace colors = primec::ui::highlight_colors;
using primec::embed::Script;
using primec::embed::ScriptEngine;
using primec::ui::Language;

TEST_SUITE_BEGIN("primestruct.ui.editor_acceptance");

namespace {
constexpr int32_t CommandOpen = 2;
constexpr uint64_t EditorWindow = 1;
constexpr uint64_t EditorView = 2;

std::string fixturePath(const std::string &name) {
  return std::string(PRIMESTRUCT_SOURCE_DIR) + "/tests/fixtures/ui/highlight/" + name;
}

std::string readFile(const std::string &path) {
  std::ifstream file(path, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

// Opens `path` in the editor, optionally types `typed`, closes without a prompt
// (typing is followed by a Don't Save answer), and returns the style runs.
std::vector<headless::StyleRun> openInEditor(const std::string &path, const std::string &typed = "") {
  headless::reset();
  headless::pushCommand(CommandOpen);
  headless::pushOpenPanelAnswer(path);
  if (!typed.empty()) {
    headless::pushTypeText(0, typed);
    headless::pushAlertAnswer(1);
  }
  headless::pushCloseWindow(EditorWindow);
  ScriptEngine engine;
  primec::ui::bindNativeUi(engine);
  const Script script = engine.compileFile(std::string(PRIMESTRUCT_SOURCE_DIR) + "/examples/apps/text_editor/main.prime");
  REQUIRE_MESSAGE(script.valid(), script.diagnostics());
  compiled_editor::Mirror mirror;
  const auto result = script.run();
  REQUIRE_MESSAGE(result.ok, result.diagnostics);
  mirror.expectSame(result.exitCode);
  return headless::styleRuns(EditorView);
}

struct Expect {
  const char *slice;
  int32_t rgb;
  int32_t flags;
};

// True when some run covers exactly `slice` of `text` with the given style.
bool hasRun(const std::vector<headless::StyleRun> &runs, const std::string &text, const Expect &expect) {
  for (const auto &run : runs) {
    if (text.substr(static_cast<size_t>(run.start), static_cast<size_t>(run.end - run.start)) == expect.slice &&
        run.rgb == expect.rgb && run.flags == expect.flags) {
      return true;
    }
  }
  return false;
}

void checkSample(const std::string &name, Language language, const std::vector<Expect> &expected) {
  const std::string path = fixturePath(name);
  const std::string text = readFile(path);
  REQUIRE(!text.empty());
  const auto runs = openInEditor(path);
  std::string dump;
  for (const auto &run : runs) {
    dump += "[" + text.substr(static_cast<size_t>(run.start), static_cast<size_t>(run.end - run.start)) + "|" +
            std::to_string(run.rgb) + "," + std::to_string(run.flags) + "] ";
  }
  for (const Expect &expect : expected) {
    CHECK_MESSAGE(hasRun(runs, text, expect), name << ": " << expect.slice << " " << expect.rgb << "," << expect.flags
                                                   << " in " << dump);
  }
  // The editor shows exactly what the highlighter computes, in order, without
  // overlaps, on code point boundaries (the backend rejects anything else).
  const auto direct = primec::ui::highlight(language, text);
  REQUIRE(runs.size() == direct.size());
  int32_t previousEnd = 0;
  for (size_t i = 0; i < runs.size(); ++i) {
    CHECK(runs[i].start == direct[i].start);
    CHECK(runs[i].end == direct[i].end);
    CHECK(runs[i].start >= previousEnd);
    previousEnd = runs[i].end;
  }
}
} // namespace

TEST_CASE("highlight: language follows the file extension") {
  using primec::ui::languageForPath;
  CHECK(languageForPath("/a/b/main.prime") == Language::PrimeStruct);
  CHECK(languageForPath("x.CPP") == Language::Cpp);
  CHECK(languageForPath("x.hpp") == Language::Cpp);
  CHECK(languageForPath("x.mm") == Language::Cpp);
  CHECK(languageForPath("tool.py") == Language::Python);
  CHECK(languageForPath("data.json") == Language::Json);
  CHECK(languageForPath("README.md") == Language::Markdown);
  CHECK(languageForPath("app.tsx") == Language::JavaScript);
  CHECK(languageForPath("app.mjs") == Language::JavaScript);
  CHECK(languageForPath("lib.rs") == Language::Rust);
  CHECK(languageForPath("main.go") == Language::Go);
  CHECK(languageForPath("A.java") == Language::Java);
  CHECK(languageForPath("a.kts") == Language::Kotlin);
  CHECK(languageForPath("a.swift") == Language::Swift);
  CHECK(languageForPath("run.zsh") == Language::Shell);
  CHECK(languageForPath("q.SQL") == Language::Sql);
  CHECK(languageForPath("/src/CMakeLists.txt") == Language::CMake);
  CHECK(languageForPath("x.cmake") == Language::CMake);
  CHECK(languageForPath("notes.txt") == Language::None);
  CHECK(languageForPath("Makefile") == Language::None);
  CHECK(languageForPath("dir.d/noext") == Language::None);
  CHECK(languageForPath("") == Language::None);
}

TEST_CASE("highlight: PrimeStruct sample") {
  checkSample("sample.prime", Language::PrimeStruct,
              {{"import", colors::Keyword, 0},
               {"// Adds one.", colors::Comment, PS_UI_STYLE_ITALIC},
               {"/* block */", colors::Comment, PS_UI_STYLE_ITALIC},
               {"return", colors::Keyword, 0},
               {"i32", colors::Type, 0},
               {"string", colors::Type, 0},
               {"1i32", colors::Number, 0},
               {"\"h\xC3\xA9llo, \xE6\x97\xA5\xE6\x9C\xAC\"", colors::String, 0}});
}

TEST_CASE("highlight: C++ sample") {
  checkSample("sample.cpp", Language::Cpp,
              {{"#include <vector>", colors::Keyword, 0},
               {"// counts items", colors::Comment, PS_UI_STYLE_ITALIC},
               {"/* total */", colors::Comment, PS_UI_STYLE_ITALIC},
               {"int", colors::Type, 0},
               {"char", colors::Type, 0},
               {"const", colors::Keyword, 0},
               {"for", colors::Keyword, 0},
               {"return", colors::Keyword, 0},
               {"\"items: 42\"", colors::String, 0},
               {"42", colors::Number, 0}});
}

TEST_CASE("highlight: Python sample") {
  checkSample("sample.py", Language::Python,
              {{"# say hello", colors::Comment, PS_UI_STYLE_ITALIC},
               {"def", colors::Keyword, 0},
               {"str", colors::Type, 0},
               {"\"\"\"Docstring with 'quotes'.\"\"\"", colors::String, 0},
               {"3.5", colors::Number, 0},
               {"if", colors::Keyword, 0},
               {"None", colors::Keyword, 0},
               {"'nobody'", colors::String, 0},
               {"\"hello {name}\"", colors::String, 0}});
}

TEST_CASE("highlight: JSON sample") {
  checkSample("sample.json", Language::Json,
              {{"\"name\"", colors::Type, 0},
               {"\"tags\"", colors::Type, 0},
               {"\"text editor\"", colors::String, 0},
               {"2", colors::Number, 0},
               {"-1.5e3", colors::Number, 0},
               {"\"a\"", colors::String, 0},
               {"\"\xE6\x97\xA5\xE6\x9C\xAC\"", colors::String, 0},
               {"true", colors::Keyword, 0},
               {"null", colors::Keyword, 0}});
}

TEST_CASE("highlight: Markdown sample") {
  checkSample("sample.md", Language::Markdown,
              {{"# Title", colors::Heading, PS_UI_STYLE_BOLD},
               {"*emphasis*", colors::Marker, PS_UI_STYLE_ITALIC},
               {"**strong**", colors::Strong, PS_UI_STYLE_BOLD},
               {"`code`", colors::Code, 0},
               {"[link](http://x.y)", colors::Link, 0},
               {"-", colors::Marker, PS_UI_STYLE_BOLD},
               {"1.", colors::Marker, PS_UI_STYLE_BOLD},
               {"> quoted", colors::Comment, PS_UI_STYLE_ITALIC},
               {"```", colors::Code, 0},
               {"fenced code", colors::Code, 0}});
}

TEST_CASE("highlight: function names are coloured in code languages") {
  checkSample("sample.prime", Language::PrimeStruct, {{"add_one", colors::Function, 0}});
  checkSample("sample.cpp", Language::Cpp, {{"count_items", colors::Function, 0}});
  checkSample("sample.py", Language::Python, {{"greet", colors::Function, 0}});
}

TEST_CASE("highlight: JavaScript and TypeScript sample") {
  checkSample("sample.js", Language::JavaScript,
              {{"// fetch data", colors::Comment, PS_UI_STYLE_ITALIC},
               {"import", colors::Keyword, 0},
               {"\"./load.js\"", colors::String, 0},
               {"from", colors::Keyword, 0},
               {"@observe", colors::Annotation, 0},
               {"class", colors::Keyword, 0},
               {"Store", colors::Type, 0},
               {"async", colors::Keyword, 0},
               {"fetchAll", colors::Function, 0},
               {"const", colors::Keyword, 0},
               {"`https://x.y/${items}`", colors::String, 0},
               {"null", colors::Keyword, 0},
               {"load", colors::Function, 0},
               {"'ok'", colors::String, 0},
               {"42", colors::Number, 0}});
}

TEST_CASE("highlight: Rust sample") {
  checkSample("sample.rs", Language::Rust,
              {{"// a Rust sample", colors::Comment, PS_UI_STYLE_ITALIC},
               {"#[derive(Debug)]", colors::Annotation, 0},
               {"struct", colors::Keyword, 0},
               {"Point", colors::Type, 0},
               {"'a", colors::Annotation, 0},
               {"str", colors::Type, 0},
               {"fn", colors::Keyword, 0},
               {"main", colors::Function, 0},
               {"u32", colors::Type, 0},
               {"5", colors::Number, 0},
               {"println!", colors::Function, 0},
               {"\"x = {}\"", colors::String, 0},
               {"'z'", colors::String, 0}});
}

TEST_CASE("highlight: Go sample") {
  checkSample("sample.go", Language::Go,
              {{"// Package main", colors::Comment, PS_UI_STYLE_ITALIC},
               {"package", colors::Keyword, 0},
               {"import", colors::Keyword, 0},
               {"\"fmt\"", colors::String, 0},
               {"func", colors::Keyword, 0},
               {"main", colors::Function, 0},
               {"var", colors::Keyword, 0},
               {"int", colors::Type, 0},
               {"3", colors::Number, 0},
               {"Println", colors::Function, 0},
               {"`a raw string`", colors::String, 0}});
}

TEST_CASE("highlight: Java sample") {
  checkSample("sample.java", Language::Java,
              {{"// Java sample", colors::Comment, PS_UI_STYLE_ITALIC},
               {"@Override", colors::Annotation, 0},
               {"public", colors::Keyword, 0},
               {"class", colors::Keyword, 0},
               {"Greeter", colors::Type, 0},
               {"final", colors::Keyword, 0},
               {"int", colors::Type, 0},
               {"7", colors::Number, 0},
               {"String", colors::Type, 0},
               {"greet", colors::Function, 0},
               {"\"hi \"", colors::String, 0}});
}

TEST_CASE("highlight: Kotlin sample") {
  checkSample("sample.kt", Language::Kotlin,
              {{"// Kotlin sample", colors::Comment, PS_UI_STYLE_ITALIC},
               {"@Suppress", colors::Annotation, 0},
               {"\"unused\"", colors::String, 0},
               {"fun", colors::Keyword, 0},
               {"greet", colors::Function, 0},
               {"String", colors::Type, 0},
               {"Int", colors::Type, 0},
               {"val", colors::Keyword, 0},
               {"3", colors::Number, 0}});
}

TEST_CASE("highlight: Swift sample") {
  checkSample("sample.swift", Language::Swift,
              {{"// Swift sample", colors::Comment, PS_UI_STYLE_ITALIC},
               {"@available", colors::Annotation, 0},
               {"func", colors::Keyword, 0},
               {"greet", colors::Function, 0},
               {"String", colors::Type, 0},
               {"Int", colors::Type, 0},
               {"let", colors::Keyword, 0},
               {"3", colors::Number, 0}});
}

TEST_CASE("highlight: shell sample") {
  checkSample("sample.sh", Language::Shell,
              {{"#!/bin/sh", colors::Comment, PS_UI_STYLE_ITALIC},
               {"# deploy script", colors::Comment, PS_UI_STYLE_ITALIC},
               {"\"world\"", colors::String, 0},
               {"if", colors::Keyword, 0},
               {"\"$name\"", colors::String, 0},
               {"then", colors::Keyword, 0},
               {"\"hello ${name}\"", colors::String, 0},
               {"'literal'", colors::String, 0},
               {"$HOME", colors::Variable, 0},
               {"fi", colors::Keyword, 0}});
}

TEST_CASE("highlight: SQL sample") {
  checkSample("sample.sql", Language::Sql,
              {{"-- list users", colors::Comment, PS_UI_STYLE_ITALIC},
               {"SELECT", colors::Keyword, 0},
               {"FROM", colors::Keyword, 0},
               {"WHERE", colors::Keyword, 0},
               {"21", colors::Number, 0},
               {"AND", colors::Keyword, 0},
               {"'Paris'", colors::String, 0},
               {"CREATE", colors::Keyword, 0},
               {"TABLE", colors::Keyword, 0},
               {"INT", colors::Type, 0},
               {"PRIMARY", colors::Keyword, 0},
               {"KEY", colors::Keyword, 0},
               {"VARCHAR", colors::Type, 0}});
}

TEST_CASE("highlight: CMake sample") {
  checkSample("sample.cmake", Language::CMake,
              {{"# build the app", colors::Comment, PS_UI_STYLE_ITALIC},
               {"cmake_minimum_required", colors::Function, 0},
               {"3.20", colors::Number, 0},
               {"set", colors::Function, 0},
               {"\"demo\"", colors::String, 0},
               {"if", colors::Keyword, 0},
               {"message", colors::Function, 0},
               {"\"${NAME} on Windows\"", colors::String, 0},
               {"endif", colors::Keyword, 0}});
}

TEST_CASE("highlight: markup and data extensions") {
  using primec::ui::languageForPath;
  CHECK(languageForPath("index.html") == Language::Html);
  CHECK(languageForPath("data.XML") == Language::Html);
  CHECK(languageForPath("logo.svg") == Language::Html);
  CHECK(languageForPath("site.css") == Language::Css);
  CHECK(languageForPath("ci.yml") == Language::Yaml);
  CHECK(languageForPath("ci.yaml") == Language::Yaml);
  CHECK(languageForPath("Cargo.toml") == Language::Toml);
  CHECK(languageForPath("my.ini") == Language::Ini);
  CHECK(languageForPath("app.conf") == Language::Ini);
}

TEST_CASE("highlight: HTML sample") {
  checkSample("sample.html", Language::Html,
              {{"<!DOCTYPE html>", colors::Keyword, 0},
               {"<!-- a page -->", colors::Comment, PS_UI_STYLE_ITALIC},
               {"<div", colors::Keyword, 0},
               {"class", colors::Type, 0},
               {"\"box\"", colors::String, 0},
               {"id", colors::Type, 0},
               {"'main'", colors::String, 0},
               {">", colors::Keyword, 0},
               {"&amp;", colors::Number, 0},
               {"<br", colors::Keyword, 0},
               {"/>", colors::Keyword, 0},
               {"</div", colors::Keyword, 0}});
}

TEST_CASE("highlight: CSS sample") {
  checkSample("sample.css", Language::Css,
              {{"/* theme */", colors::Comment, PS_UI_STYLE_ITALIC},
               {"@media", colors::Keyword, 0},
               {"600px", colors::Number, 0},
               {".box", colors::Type, 0},
               {"a:hover", colors::Type, 0},
               {"color", colors::Keyword, 0},
               {"#ff8800", colors::Number, 0},
               {"margin", colors::Keyword, 0},
               {"-4px", colors::Number, 0},
               {"1.5em", colors::Number, 0},
               {"font-family", colors::Keyword, 0},
               {"\"Helvetica\"", colors::String, 0},
               {"background", colors::Keyword, 0},
               {"rgb", colors::Function, 0},
               {"!important", colors::Keyword, 0}});
}

TEST_CASE("highlight: YAML sample") {
  checkSample("sample.yaml", Language::Yaml,
              {{"# settings", colors::Comment, PS_UI_STYLE_ITALIC},
               {"---", colors::Keyword, 0},
               {"name", colors::Type, 0},
               {"\"demo app\"", colors::String, 0},
               {"version", colors::Type, 0},
               {"3.5", colors::Number, 0},
               {"true", colors::Keyword, 0},
               {"&base", colors::Annotation, 0},
               {"retries", colors::Type, 0},
               {"3", colors::Number, 0},
               {"# inline comment", colors::Comment, PS_UI_STYLE_ITALIC},
               {"-", colors::Marker, PS_UI_STYLE_BOLD},
               {"null", colors::Keyword, 0},
               {"key", colors::Type, 0}});
}

TEST_CASE("highlight: TOML and INI samples") {
  checkSample("sample.toml", Language::Toml,
              {{"# config", colors::Comment, PS_UI_STYLE_ITALIC},
               {"[package]", colors::Heading, PS_UI_STYLE_BOLD},
               {"name", colors::Type, 0},
               {"\"demo\"", colors::String, 0},
               {"12", colors::Number, 0},
               {"true", colors::Keyword, 0},
               {"tags", colors::Type, 0},
               {"\"a\"", colors::String, 0},
               {"\"b\"", colors::String, 0}});
  checkSample("sample.ini", Language::Ini,
              {{"; ini sample", colors::Comment, PS_UI_STYLE_ITALIC},
               {"[server]", colors::Heading, PS_UI_STYLE_BOLD},
               {"port", colors::Type, 0},
               {"8080", colors::Number, 0},
               {"host", colors::Type, 0},
               {"\"localhost\"", colors::String, 0}});
}

TEST_CASE("highlight: strings and comments hide their contents from the lexer") {
  const auto runs = primec::ui::highlight(Language::Cpp, "// int return 42\nconst char *s = \"if 7 //\";\n");
  REQUIRE(runs.size() == 4);
  CHECK(runs[0] == primec::ui::HighlightRun{0, 16, colors::Comment, PS_UI_STYLE_ITALIC});
  CHECK(runs[1].rgb == colors::Keyword); // const
  CHECK(runs[2].rgb == colors::Type);    // char
  CHECK(runs[3].rgb == colors::String);  // the whole literal, one run
  CHECK(runs[3].end - runs[3].start == 9);
}

TEST_CASE("highlight: unknown extensions and untitled documents get no styles") {
  CHECK(openInEditor(fixturePath("notes.txt")).empty());
}

TEST_CASE("highlight: each tab is styled for its own file") {
  const std::string cpp = fixturePath("sample.cpp");
  const std::string json = fixturePath("sample.json");
  headless::reset();
  headless::pushCommand(CommandOpen);
  headless::pushOpenPanelAnswer(cpp);
  headless::pushCommand(CommandOpen);
  headless::pushOpenPanelAnswer(json);
  headless::pushCloseWindow(10);
  headless::pushCloseWindow(EditorWindow);
  ScriptEngine engine;
  primec::ui::bindNativeUi(engine);
  const Script script = engine.compileFile(std::string(PRIMESTRUCT_SOURCE_DIR) + "/examples/apps/text_editor/main.prime");
  REQUIRE_MESSAGE(script.valid(), script.diagnostics());
  compiled_editor::Mirror mirror;
  const auto result = script.run();
  REQUIRE_MESSAGE(result.ok, result.diagnostics);
  mirror.expectSame(result.exitCode);
  // Window 1 holds text view 2 (C++), the second tab's window 10 holds text view 11 (JSON).
  const auto cppRuns = primec::ui::highlight(Language::Cpp, readFile(cpp));
  const auto jsonRuns = primec::ui::highlight(Language::Json, readFile(json));
  const auto first = headless::styleRuns(EditorView);
  const auto second = headless::styleRuns(11);
  REQUIRE(first.size() == cppRuns.size());
  REQUIRE(second.size() == jsonRuns.size());
  CHECK(first.front().start == cppRuns.front().start);
  CHECK(second.front().start == jsonRuns.front().start);
  CHECK(first.front().rgb != second.front().rgb);
}

TEST_CASE("highlight: files past 64 KiB are styled too, and only huge ones are not") {
  std::string big;
  while (big.size() < 100 * 1024) {
    big += "## Heading\n\nSome *text* with `code` and **bold**.\n\n";
  }
  const std::filesystem::path dir = primec::testing::testScratchDir("native_ui_editor_highlight");
  std::filesystem::create_directories(dir);
  const std::string path = (dir / "big.md").string();
  std::ofstream(path, std::ios::binary) << big;
  const auto expected = primec::ui::highlight(Language::Markdown, big);
  REQUIRE(big.size() > 64 * 1024);
  const auto runs = openInEditor(path);
  CHECK(runs.size() == expected.size());
  CHECK(runs.size() > 1000);
  // Above the limit the text stays unstyled.
  headless::reset();
  REQUIRE(ps_ui_init("Limit"));
  const uint64_t view = ps_ui_text_view_create();
  REQUIRE(ps_ui_text_view_set_text(view, std::string(primec::ui::MaxHighlightBytes + 1, 'a').c_str()));
  CHECK(ps_ui_text_view_highlight(view, "x.md"));
  CHECK(headless::styleRuns(view).empty());
}

TEST_CASE("highlight: a file opened into a fresh New tab is styled") {
  const std::string md = fixturePath("sample.md");
  headless::reset();
  headless::pushCommand(CommandOpen);
  headless::pushOpenPanelAnswer(md);
  headless::pushCommand(1); // New: a second, untitled tab (window 10, text view 11)
  headless::pushCommand(CommandOpen);
  headless::pushOpenPanelAnswer(md);
  headless::pushCloseWindow(10);
  headless::pushCloseWindow(EditorWindow);
  ScriptEngine engine;
  primec::ui::bindNativeUi(engine);
  const Script script = engine.compileFile(std::string(PRIMESTRUCT_SOURCE_DIR) + "/examples/apps/text_editor/main.prime");
  REQUIRE_MESSAGE(script.valid(), script.diagnostics());
  compiled_editor::Mirror mirror;
  const auto result = script.run();
  REQUIRE_MESSAGE(result.ok, result.diagnostics);
  mirror.expectSame(result.exitCode);
  const auto expected = primec::ui::highlight(Language::Markdown, readFile(md));
  REQUIRE(!expected.empty());
  CHECK(headless::styleRuns(EditorView).size() == expected.size());
  CHECK(headless::styleRuns(11).size() == expected.size());
  CHECK(headless::windowPath(10) == md);
  // The open went into the New tab (reused, not a third tab).
  CHECK(headless::tabGroup(EditorWindow).size() == 0);
  CHECK(headless::windowState(11).exists == false);
}

TEST_CASE("highlight: the editor restyles after typing") {
  const std::string path = fixturePath("sample.json");
  const auto before = openInEditor(path);
  const auto after = openInEditor(path, " true");
  REQUIRE(!before.empty());
  // The typed text is appended to the document; the whole text is restyled.
  CHECK(after.size() >= before.size());
}
