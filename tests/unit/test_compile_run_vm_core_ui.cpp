#include "test_compile_run_helpers.h"

#include <cerrno>

TEST_SUITE_BEGIN("primestruct.compile.run.vm.core");


TEST_CASE("runs vm composite login form deterministically") {
  const std::string source = R"(
import /std/ui/*
import /std/math/*
import /std/collections/*

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
main() {
  [LayoutTree mut] layout{LayoutTree{}}
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

  [CommandList mut] commands{CommandList{}}
  commands.draw_login_form(
    layout,
    login,
    10i32,
    1i32,
    2i32,
    4i32,
    3i32,
    Rgba8{[r] 9i32, [g] 8i32, [b] 7i32, [a] 255i32},
    Rgba8{[r] 1i32, [g] 2i32, [b] 3i32, [a] 255i32},
    Rgba8{[r] 20i32, [g] 30i32, [b] 40i32, [a] 255i32},
    Rgba8{[r] 200i32, [g] 201i32, [b] 202i32, [a] 255i32},
    Rgba8{[r] 50i32, [g] 60i32, [b] 70i32, [a] 255i32},
    Rgba8{[r] 250i32, [g] 251i32, [b] 252i32, [a] 255i32},
    "Login"utf8,
    "alice"utf8,
    "secret"utf8,
    "Go"utf8
  )
  dump_words(commands.serialize())
  return(plus(layout.node_count(), plus(commands.command_count(), commands.clip_depth())))
}
)";
  const std::string srcPath = writeTemp("vm_ui_composite_login_form.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_vm_ui_composite_login_form.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 16);
  CHECK(readFile(outPath) ==
        "1,10,2,9,7,8,38,55,4,9,8,7,255,3,4,9,10,34,51,1,13,9,10,10,1,2,3,255,5,76,111,103,105,110,2,9,9,21,34,12,3,20,30,40,255,1,13,10,22,10,200,201,202,255,5,97,108,105,99,101,2,9,9,34,34,12,3,20,30,40,255,1,14,10,35,10,200,201,202,255,6,115,101,99,114,101,116,2,9,9,47,34,14,3,50,60,70,255,1,10,11,49,10,250,251,252,255,2,71,111,4,0\n");
}

TEST_CASE("runs vm html adapter login form deterministically") {
  const std::string source = R"(
import /std/ui/*
import /std/math/*
import /std/collections/*

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
main() {
  [LayoutTree mut] layout{LayoutTree{}}
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

  [HtmlCommandList mut] html{HtmlCommandList{}}
  html.emit_login_form(
    layout,
    login,
    10i32,
    1i32,
    2i32,
    4i32,
    3i32,
    Rgba8{[r] 9i32, [g] 8i32, [b] 7i32, [a] 255i32},
    Rgba8{[r] 1i32, [g] 2i32, [b] 3i32, [a] 255i32},
    Rgba8{[r] 20i32, [g] 30i32, [b] 40i32, [a] 255i32},
    Rgba8{[r] 200i32, [g] 201i32, [b] 202i32, [a] 255i32},
    Rgba8{[r] 50i32, [g] 60i32, [b] 70i32, [a] 255i32},
    Rgba8{[r] 250i32, [g] 251i32, [b] 252i32, [a] 255i32},
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
)";
  const std::string srcPath = writeTemp("vm_ui_html_login_form.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_vm_ui_html_login_form.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 14);
  CHECK(readFile(outPath) ==
        "1,8,1,12,1,0,7,8,38,55,2,4,9,8,7,255,2,17,2,1,9,10,34,10,10,1,2,3,255,5,76,111,103,105,110,4,23,3,1,9,21,34,12,10,1,3,20,30,40,255,200,201,202,255,5,97,108,105,99,101,5,13,3,2,10,117,115,101,114,95,105,110,112,117,116,4,24,4,1,9,34,34,12,10,1,3,20,30,40,255,200,201,202,255,6,115,101,99,114,101,116,5,13,4,2,10,112,97,115,115,95,105,110,112,117,116,3,20,5,1,9,47,34,14,10,2,3,50,60,70,255,250,251,252,255,2,71,111,5,15,5,1,12,115,117,98,109,105,116,95,99,108,105,99,107\n");
}

TEST_CASE("runs vm ui event stream deterministically") {
  const std::string source = R"(
import /std/ui/*
import /std/math/*
import /std/collections/*

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
main() {
  [LayoutTree mut] layout{LayoutTree{}}
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

  [UiEventStream mut] events{UiEventStream{}}
  events.push_pointer_move(login.usernameInput, 7i32, 20i32, 30i32)
  events.push_pointer_down(login.submitButton, 7i32, 1i32, 20i32, 30i32)
  events.push_pointer_up(login.submitButton, 7i32, 1i32, 21i32, 31i32)
  events.push_key_down(login.usernameInput, 13i32, 3i32, 1i32)
  events.push_key_up(login.usernameInput, 13i32, 1i32)
  dump_words(events.serialize())
  return(plus(login.submitButton, events.event_count()))
}
)";
  const std::string srcPath = writeTemp("vm_ui_event_stream.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_vm_ui_event_stream.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 10);
  CHECK(readFile(outPath) ==
        "1,5,1,5,3,7,-1,20,30,2,5,5,7,1,20,30,3,5,5,7,1,21,31,4,4,3,13,3,1,5,4,3,13,1,0\n");
}

TEST_CASE("runs vm ui ime event stream deterministically") {
  const std::string source = R"(
import /std/ui/*
import /std/math/*
import /std/collections/*

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
main() {
  [LayoutTree mut] layout{LayoutTree{}}
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

  [UiEventStream mut] events{UiEventStream{}}
  events.push_ime_preedit(login.usernameInput, 1i32, 4i32, "al|"utf8)
  events.push_ime_commit(login.usernameInput, "alice"utf8)
  dump_words(events.serialize())
  return(plus(login.usernameInput, events.event_count()))
}
)";
  const std::string srcPath = writeTemp("vm_ui_ime_event_stream.prime", source);
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_vm_ui_ime_event_stream.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 5);
  CHECK(readFile(outPath) ==
        "1,2,6,7,3,1,4,3,97,108,124,7,9,3,-1,-1,5,97,108,105,99,101\n");
}

TEST_CASE("runs vm ui resize and focus event stream deterministically") {
  const std::string source = R"(
import /std/ui/*
import /std/math/*
import /std/collections/*

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
main() {
  [LayoutTree mut] layout{LayoutTree{}}
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

  [UiEventStream mut] events{UiEventStream{}}
  events.push_resize(login.panel, 40i32, 57i32)
  events.push_focus_gained(login.usernameInput)
  events.push_focus_lost(login.usernameInput)
  dump_words(events.serialize())
  return(plus(login.usernameInput, events.event_count()))
}
)";
  const std::string srcPath = writeTemp("vm_ui_resize_focus_event_stream.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_ui_resize_focus_event_stream.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 6);
  CHECK(readFile(outPath) == "1,3,8,3,1,40,57,9,3,3,0,0,10,3,3,0,0\n");
}

TEST_CASE("runs vm with literal method call") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  inc([i32] value) {
    return(plus(value, 1i32))
  }
}

[return<int>]
main() {
  return(1i32.inc())
}
)";
  const std::string srcPath = writeTemp("vm_method_literal.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 2);
}


TEST_SUITE_END();
