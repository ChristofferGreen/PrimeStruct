#pragma once

bool buildCachedEmittedCppExecutableAtO0(const std::string &fixtureName,
                                         const std::string &source,
                                         std::string &exePathOut);

static void expectCppVectorCountCompatibilityTypeMismatchReject(const std::string &compileCmd) {
  const std::string errPath =
      testScratchPath("primec_cpp_vector_count_compatibility_type_mismatch_reject_err.txt").string();
  const std::string captureCmd = compileCmd + " > /dev/null 2> " + errPath;
  CHECK(runCommand(captureCmd) != 0);
  CHECK_FALSE(readFile(errPath).empty());
}

static std::string sharedCppEmitterFixtureArgs(int selector) {
  std::string args;
  for (int index = 1; index < selector; ++index) {
    args += " ";
    args += quoteShellArg("case" + std::to_string(index));
  }
  return args;
}

static std::string requireSharedCppEmitterFixtureExecutable(const std::string &fixtureName,
                                                            const std::string &source) {
  std::string exePath;
  REQUIRE(buildCachedEmittedCppExecutableAtO0(fixtureName, source, exePath));
  return exePath;
}

static int runSharedCppEmitterFixture(const std::string &fixtureName,
                                      const std::string &source,
                                      int selector) {
  const std::string exePath = requireSharedCppEmitterFixtureExecutable(fixtureName, source);
  return runCommand(quoteShellArg(exePath) + sharedCppEmitterFixtureArgs(selector));
}

static int runSharedCppEmitterFixtureToFile(const std::string &fixtureName,
                                            const std::string &source,
                                            int selector,
                                            const std::string &outPath) {
  const std::string exePath = requireSharedCppEmitterFixtureExecutable(fixtureName, source);
  return runCommand(quoteShellArg(exePath) + sharedCppEmitterFixtureArgs(selector) + " > " + quoteShellArg(outPath));
}

static const std::string &sharedCppEmitterResultFixtureSource() {
  static const std::string source = R"(
[return<Result<i32, FileError>>]
lift([i32] value) {
  return(Result.ok(value))
}

[return<int>]
run_result_combinators() {
  [Result<i32, FileError>] first{ Result.ok(2i32) }
  [Result<i32, FileError>] second{ Result.ok(3i32) }
  return(
    Result.and_then(
      Result.map2(
        Result.map(first, []([i32] value) { return(multiply(value, 4i32)) }),
        second,
        []([i32] left, [i32] right) { return(plus(left, right)) }
      ),
      []([i32] total) { return(lift(plus(total, 5i32))) }
    )
  )
}

[return<int>]
run_result_ok_lambda() {
  [Result<i32, FileError>] first{ Result.ok(2i32) }
  [Result<i32, FileError>] second{ Result.ok(3i32) }
  return(
    Result.and_then(
      Result.map2(first, second, []([i32] left, [i32] right) { return(plus(left, right)) }),
      []([i32] total) { return(Result.ok(multiply(total, 4i32))) }
    )
  )
}

[return<int>]
main([array<string>] args) {
  [i32] selector{count(args)}
  if(equal(selector, 2i32)) {
    return(run_result_combinators())
  }
  if(equal(selector, 3i32)) {
    return(run_result_ok_lambda())
  }
  return(91i32)
}
)";
  return source;
}

static const std::string &sharedCppEmitterImageFixtureSource() {
  static const std::string source = R"(
import /std/image/*

[effects(heap_alloc, io_out, file_write), return<int>]
main([array<string>] args) {
  [i32 mut] width{0i32}
  [i32 mut] height{0i32}
  [vector<i32> mut] pixels{vector<i32>()}
  print_line(Result.why(/std/image/ppm/read(width, height, pixels, "input.ppm"utf8)))
  print_line(Result.why(/std/image/ppm/write("output.ppm"utf8, width, height, pixels)))
  print_line(Result.why(/std/image/png/read(width, height, pixels, "input.png"utf8)))
  print_line(Result.why(/std/image/png/write("output.png"utf8, width, height, pixels)))
  return(plus(width, height))
}
)";
  return source;
}

static const std::string &sharedCppEmitterUiCommandFixtureSource() {
  static const std::string source = R"(
import /std/collections/*
import /std/math/*
import /std/ui/*

[effects(io_out), return<void>]
dump_words([vector<i32>] words) {
  [i32] len{vectorCount<i32>(words)}
  for([i32 mut] index{0i32}, less_than(index, len), assign(index, plus(index, 1i32))) {
    if(greater_than(index, 0i32)) {
      print(","utf8)
    }
    print(vectorAt<i32>(words, index))
  }
  print_line(""utf8)
}

[effects(heap_alloc, io_out), return<int>]
run_ui_command_serialization() {
  [CommandList mut] commands{CommandList()}
  commands.draw_rounded_rect(
    2i32,
    4i32,
    30i32,
    40i32,
    6i32,
    Rgba8([r] 12i32, [g] 34i32, [b] 56i32, [a] 255i32)
  )
  commands.draw_text(
    7i32,
    9i32,
    14i32,
    Rgba8([r] 255i32, [g] 240i32, [b] 0i32, [a] 255i32),
    "Hi!"utf8
  )
  dump_words(commands.serialize())
  return(commands.command_count())
}

[effects(heap_alloc, io_out), return<int>]
run_ui_clip_command_serialization() {
  [CommandList mut] commands{CommandList()}
  commands.push_clip(1i32, 2i32, 20i32, 10i32)
  commands.draw_text(
    7i32,
    9i32,
    14i32,
    Rgba8([r] 255i32, [g] 240i32, [b] 0i32, [a] 255i32),
    "Hi!"utf8
  )
  commands.push_clip(3i32, 4i32, 5i32, 6i32)
  commands.draw_rounded_rect(
    8i32,
    9i32,
    10i32,
    11i32,
    2i32,
    Rgba8([r] 1i32, [g] 2i32, [b] 3i32, [a] 4i32)
  )
  commands.pop_clip()
  commands.pop_clip()
  commands.pop_clip()
  dump_words(commands.serialize())
  return(plus(commands.command_count(), commands.clip_depth()))
}

[effects(heap_alloc, io_out), return<int>]
main([array<string>] args) {
  [i32] selector{count(args)}
  if(equal(selector, 2i32)) {
    return(run_ui_command_serialization())
  }
  if(equal(selector, 3i32)) {
    return(run_ui_clip_command_serialization())
  }
  return(91i32)
}
)";
  return source;
}

static const std::string &sharedCppEmitterUiLayoutFixtureSource() {
  static const std::string source = R"(
import /std/collections/*
import /std/math/*
import /std/ui/*

[effects(io_out), return<void>]
dump_words([vector<i32>] words) {
  [i32] len{vectorCount<i32>(words)}
  for([i32 mut] index{0i32}, less_than(index, len), assign(index, plus(index, 1i32))) {
    if(greater_than(index, 0i32)) {
      print(","utf8)
    }
    print(vectorAt<i32>(words, index))
  }
  print_line(""utf8)
}

[effects(heap_alloc, io_out), return<int>]
run_ui_layout_tree_serialization() {
  [LayoutTree mut] tree{LayoutTree()}
  [i32] root{tree.append_root_column(2i32, 3i32, 10i32, 4i32)}
  [i32] header{tree.append_leaf(root, 20i32, 5i32)}
  [i32] body{tree.append_column(root, 1i32, 2i32, 12i32, 8i32)}
  [i32] badge{tree.append_leaf(body, 8i32, 4i32)}
  [i32] details{tree.append_leaf(body, 10i32, 6i32)}
  [i32] footer{tree.append_leaf(root, 6i32, 7i32)}
  tree.measure()
  tree.arrange(10i32, 20i32, 50i32, 40i32)
  dump_words(tree.serialize())
  return(tree.node_count())
}

[effects(heap_alloc, io_out), return<int>]
run_ui_layout_tree_empty_root() {
  [LayoutTree mut] tree{LayoutTree()}
  [i32] root{tree.append_root_column(4i32, 9i32, 11i32, 13i32)}
  tree.measure()
  tree.arrange(3i32, 5i32, 11i32, 13i32)
  dump_words(tree.serialize())
  return(tree.node_count())
}

[effects(heap_alloc, io_out), return<int>]
main([array<string>] args) {
  [i32] selector{count(args)}
  if(equal(selector, 2i32)) {
    return(run_ui_layout_tree_serialization())
  }
  if(equal(selector, 3i32)) {
    return(run_ui_layout_tree_empty_root())
  }
  return(91i32)
}
)";
  return source;
}

static const std::string &sharedCppEmitterUiWidgetFixtureSource() {
  static const std::string source = R"(
import /std/collections/*
import /std/math/*
import /std/ui/*

[effects(io_out), return<void>]
dump_words([vector<i32>] words) {
  [i32] len{vectorCount<i32>(words)}
  for([i32 mut] index{0i32}, less_than(index, len), assign(index, plus(index, 1i32))) {
    if(greater_than(index, 0i32)) {
      print(","utf8)
    }
    print(vectorAt<i32>(words, index))
  }
  print_line(""utf8)
}

[effects(heap_alloc, io_out), return<int>]
run_ui_basic_widget_controls() {
  [LayoutTree mut] layout{LayoutTree()}
  [i32] root{layout.append_root_column(1i32, 2i32, 0i32, 0i32)}
  [i32] title{layout.append_label(root, 10i32, "Hi"utf8)}
  [i32] action{layout.append_button(root, 10i32, 3i32, "Go"utf8)}
  [i32] field{layout.append_input(root, 10i32, 2i32, 18i32, "abc"utf8)}
  layout.measure()
  layout.arrange(5i32, 6i32, 30i32, 46i32)

  [CommandList mut] commands{CommandList()}
  commands.draw_label(layout, title, 10i32, Rgba8([r] 1i32, [g] 2i32, [b] 3i32, [a] 255i32), "Hi"utf8)
  commands.draw_button(
    layout,
    action,
    10i32,
    3i32,
    4i32,
    Rgba8([r] 10i32, [g] 20i32, [b] 30i32, [a] 255i32),
    Rgba8([r] 250i32, [g] 251i32, [b] 252i32, [a] 255i32),
    "Go"utf8
  )
  commands.draw_input(
    layout,
    field,
    10i32,
    2i32,
    3i32,
    Rgba8([r] 40i32, [g] 50i32, [b] 60i32, [a] 255i32),
    Rgba8([r] 200i32, [g] 210i32, [b] 220i32, [a] 255i32),
    "abc"utf8
  )
  dump_words(commands.serialize())
  return(plus(layout.node_count(), commands.command_count()))
}

[effects(heap_alloc, io_out), return<int>]
run_ui_panel_widget() {
  [LayoutTree mut] layout{LayoutTree()}
  [i32] root{layout.append_root_column(1i32, 2i32, 0i32, 0i32)}
  [i32] title{layout.append_label(root, 10i32, "Top"utf8)}
  [i32] panel{layout.append_panel(root, 2i32, 1i32, 20i32, 12i32)}
  [i32] action{layout.append_button(panel, 10i32, 2i32, "Go"utf8)}
  [i32] field{layout.append_input(panel, 10i32, 1i32, 16i32, "abc"utf8)}
  [i32] footer{layout.append_label(root, 10i32, "!"utf8)}
  layout.measure()
  layout.arrange(4i32, 5i32, 28i32, 60i32)

  [CommandList mut] commands{CommandList()}
  commands.draw_label(layout, title, 10i32, Rgba8([r] 1i32, [g] 2i32, [b] 3i32, [a] 255i32), "Top"utf8)
  commands.begin_panel(layout, panel, 4i32, Rgba8([r] 8i32, [g] 9i32, [b] 10i32, [a] 255i32))
  commands.draw_button(
    layout,
    action,
    10i32,
    2i32,
    3i32,
    Rgba8([r] 20i32, [g] 30i32, [b] 40i32, [a] 255i32),
    Rgba8([r] 200i32, [g] 201i32, [b] 202i32, [a] 255i32),
    "Go"utf8
  )
  commands.draw_input(
    layout,
    field,
    10i32,
    1i32,
    2i32,
    Rgba8([r] 50i32, [g] 60i32, [b] 70i32, [a] 255i32),
    Rgba8([r] 210i32, [g] 211i32, [b] 212i32, [a] 255i32),
    "abc"utf8
  )
  commands.end_panel()
  commands.draw_label(layout, footer, 10i32, Rgba8([r] 1i32, [g] 2i32, [b] 3i32, [a] 255i32), "!"utf8)
  dump_words(commands.serialize())
  return(plus(layout.node_count(), plus(commands.command_count(), commands.clip_depth())))
}

[effects(heap_alloc, io_out), return<int>]
run_ui_empty_panel_widget() {
  [LayoutTree mut] layout{LayoutTree()}
  [i32] root{layout.append_root_column(0i32, 0i32, 0i32, 0i32)}
  [i32] panel{layout.append_panel(root, 3i32, 1i32, 12i32, 10i32)}
  layout.measure()
  layout.arrange(2i32, 3i32, 20i32, 18i32)

  [CommandList mut] commands{CommandList()}
  commands.begin_panel(layout, panel, 5i32, Rgba8([r] 9i32, [g] 8i32, [b] 7i32, [a] 255i32))
  commands.end_panel()
  dump_words(commands.serialize())
  return(plus(layout.node_count(), plus(commands.command_count(), commands.clip_depth())))
}

[effects(heap_alloc, io_out), return<int>]
main([array<string>] args) {
  [i32] selector{count(args)}
  if(equal(selector, 2i32)) {
    return(run_ui_basic_widget_controls())
  }
  if(equal(selector, 3i32)) {
    return(run_ui_panel_widget())
  }
  if(equal(selector, 4i32)) {
    return(run_ui_empty_panel_widget())
  }
  return(91i32)
}
)";
  return source;
}

static const std::string &sharedCppEmitterUiLoginFixtureSource() {
  static const std::string source = R"(
import /std/collections/*
import /std/math/*
import /std/ui/*

[effects(io_out), return<void>]
dump_words([vector<i32>] words) {
  [i32] len{vectorCount<i32>(words)}
  for([i32 mut] index{0i32}, less_than(index, len), assign(index, plus(index, 1i32))) {
    if(greater_than(index, 0i32)) {
      print(","utf8)
    }
    print(vectorAt<i32>(words, index))
  }
  print_line(""utf8)
}

[effects(heap_alloc, io_out), return<int>]
run_ui_composite_login_form() {
  [LayoutTree mut] layout{LayoutTree()}
  [i32] root{layout.append_root_column(1i32, 0i32, 0i32, 0i32)}
  [LoginFormNodes] login{layout.append_login_form(
    root,
    2i32,
    1i32,
    10i32,
    1i32,
    2i32,
    16i32,
    "Login"utf8,
    "alice"utf8,
    "secret"utf8,
    "Go"utf8
  )}
  layout.measure()
  layout.arrange(6i32, 7i32, 40i32, 57i32)

  [CommandList mut] commands{CommandList()}
  commands.draw_login_form(
    layout,
    login,
    10i32,
    1i32,
    2i32,
    4i32,
    3i32,
    Rgba8([r] 9i32, [g] 8i32, [b] 7i32, [a] 255i32),
    Rgba8([r] 1i32, [g] 2i32, [b] 3i32, [a] 255i32),
    Rgba8([r] 20i32, [g] 30i32, [b] 40i32, [a] 255i32),
    Rgba8([r] 200i32, [g] 201i32, [b] 202i32, [a] 255i32),
    Rgba8([r] 50i32, [g] 60i32, [b] 70i32, [a] 255i32),
    Rgba8([r] 250i32, [g] 251i32, [b] 252i32, [a] 255i32),
    "Login"utf8,
    "alice"utf8,
    "secret"utf8,
    "Go"utf8
  )
  dump_words(commands.serialize())
  return(plus(layout.node_count(), plus(commands.command_count(), commands.clip_depth())))
}

[effects(heap_alloc, io_out), return<int>]
run_ui_html_login_form() {
  [LayoutTree mut] layout{LayoutTree()}
  [i32] root{layout.append_root_column(1i32, 0i32, 0i32, 0i32)}
  [LoginFormNodes] login{layout.append_login_form(
    root,
    2i32,
    1i32,
    10i32,
    1i32,
    2i32,
    16i32,
    "Login"utf8,
    "alice"utf8,
    "secret"utf8,
    "Go"utf8
  )}
  layout.measure()
  layout.arrange(6i32, 7i32, 40i32, 57i32)

  [HtmlCommandList mut] html{HtmlCommandList()}
  html.emit_login_form(
    layout,
    login,
    10i32,
    1i32,
    2i32,
    4i32,
    3i32,
    Rgba8([r] 9i32, [g] 8i32, [b] 7i32, [a] 255i32),
    Rgba8([r] 1i32, [g] 2i32, [b] 3i32, [a] 255i32),
    Rgba8([r] 20i32, [g] 30i32, [b] 40i32, [a] 255i32),
    Rgba8([r] 200i32, [g] 201i32, [b] 202i32, [a] 255i32),
    Rgba8([r] 50i32, [g] 60i32, [b] 70i32, [a] 255i32),
    Rgba8([r] 250i32, [g] 251i32, [b] 252i32, [a] 255i32),
    "Login"utf8,
    "alice"utf8,
    "secret"utf8,
    "Go"utf8,
    "user_input"utf8,
    "pass_input"utf8,
    "submit_click"utf8
  )
  dump_words(html.serialize())
  return(plus(layout.node_count(), html.commandCount))
}

[effects(heap_alloc, io_out), return<int>]
main([array<string>] args) {
  [i32] selector{count(args)}
  if(equal(selector, 2i32)) {
    return(run_ui_composite_login_form())
  }
  if(equal(selector, 3i32)) {
    return(run_ui_html_login_form())
  }
  return(91i32)
}
)";
  return source;
}

static const std::string &sharedCppEmitterUiEventFixtureSource() {
  static const std::string source = R"(
import /std/collections/*
import /std/math/*
import /std/ui/*

[effects(io_out), return<void>]
dump_words([vector<i32>] words) {
  [i32] len{vectorCount<i32>(words)}
  for([i32 mut] index{0i32}, less_than(index, len), assign(index, plus(index, 1i32))) {
    if(greater_than(index, 0i32)) {
      print(","utf8)
    }
    print(vectorAt<i32>(words, index))
  }
  print_line(""utf8)
}

[effects(heap_alloc, io_out), return<int>]
run_ui_event_stream() {
  [LayoutTree mut] layout{LayoutTree()}
  [i32] root{layout.append_root_column(1i32, 0i32, 0i32, 0i32)}
  [LoginFormNodes] login{layout.append_login_form(
    root,
    2i32,
    1i32,
    10i32,
    1i32,
    2i32,
    16i32,
    "Login"utf8,
    "alice"utf8,
    "secret"utf8,
    "Go"utf8
  )}

  [UiEventStream mut] events{UiEventStream()}
  events.push_pointer_move(login.usernameInput, 7i32, 20i32, 30i32)
  events.push_pointer_down(login.submitButton, 7i32, 1i32, 20i32, 30i32)
  events.push_pointer_up(login.submitButton, 7i32, 1i32, 21i32, 31i32)
  events.push_key_down(login.usernameInput, 13i32, 3i32, 1i32)
  events.push_key_up(login.usernameInput, 13i32, 1i32)
  dump_words(events.serialize())
  return(plus(login.submitButton, events.event_count()))
}

[effects(heap_alloc, io_out), return<int>]
run_ui_ime_event_stream() {
  [LayoutTree mut] layout{LayoutTree()}
  [i32] root{layout.append_root_column(1i32, 0i32, 0i32, 0i32)}
  [LoginFormNodes] login{layout.append_login_form(
    root,
    2i32,
    1i32,
    10i32,
    1i32,
    2i32,
    16i32,
    "Login"utf8,
    "alice"utf8,
    "secret"utf8,
    "Go"utf8
  )}

  [UiEventStream mut] events{UiEventStream()}
  events.push_ime_preedit(login.usernameInput, 1i32, 4i32, "al|"utf8)
  events.push_ime_commit(login.usernameInput, "alice"utf8)
  dump_words(events.serialize())
  return(plus(login.usernameInput, events.event_count()))
}

[effects(heap_alloc, io_out), return<int>]
run_ui_resize_focus_event_stream() {
  [LayoutTree mut] layout{LayoutTree()}
  [i32] root{layout.append_root_column(1i32, 0i32, 0i32, 0i32)}
  [LoginFormNodes] login{layout.append_login_form(
    root,
    2i32,
    1i32,
    10i32,
    1i32,
    2i32,
    16i32,
    "Login"utf8,
    "alice"utf8,
    "secret"utf8,
    "Go"utf8
  )}

  [UiEventStream mut] events{UiEventStream()}
  events.push_resize(login.panel, 40i32, 57i32)
  events.push_focus_gained(login.usernameInput)
  events.push_focus_lost(login.usernameInput)
  dump_words(events.serialize())
  return(plus(login.usernameInput, events.event_count()))
}

[effects(heap_alloc, io_out), return<int>]
main([array<string>] args) {
  [i32] selector{count(args)}
  if(equal(selector, 2i32)) {
    return(run_ui_event_stream())
  }
  if(equal(selector, 3i32)) {
    return(run_ui_ime_event_stream())
  }
  if(equal(selector, 4i32)) {
    return(run_ui_resize_focus_event_stream())
  }
  return(91i32)
}
)";
  return source;
}
