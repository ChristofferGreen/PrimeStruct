#include "test_compile_run_helpers.h"

#include "test_compile_run_native_backend_core_helpers.h"

#if PRIMESTRUCT_NATIVE_CORE_ENABLED
TEST_SUITE_BEGIN("primestruct.compile.run.native_backend.core");

TEST_CASE("compiles and runs native void call with string param") {
  const std::string source = R"(
[return<void> effects(io_out)]
echo([string] msg) {
  print_line(msg)
}

[return<int> effects(io_out)]
main() {
  echo("alpha"utf8)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("compile_native_string_call.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_string_call_exe").string();
  const std::string outPath = (testScratchPath("") / "primec_native_string_call_out.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  const std::string runCmd = exePath + " > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "alpha\n");
}

TEST_CASE("compiles and runs native string indexing") {
  const std::string source = R"(
[return<int>]
main() {
  [string] text{"abc"utf8}
  [i32] a{at(text, 0i32)}
  [i32] b{at_unsafe(text, 1i32)}
  [i32] len{count(text)}
  return(plus(plus(a, b), len))
}
)";
  const std::string srcPath = writeTemp("compile_native_string_index_builtin.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_string_index_builtin_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == (97 + 98 + 3));
}

TEST_CASE("compiles and runs native string parameter indexing") {
  const std::string source = R"(
[return<i32>]
pick([string] text, [i32] index) {
  return(at(text, index))
}

[return<int>]
main() {
  return(pick("abc"utf8, 1i32))
}
)";
  const std::string srcPath = writeTemp("compile_native_string_param_index.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_string_param_index_exe").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath) == 98);
}

TEST_CASE("compiles and runs native software renderer command serialization deterministically") {
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
  [CommandList mut] commands{CommandList{}}
  commands.draw_rounded_rect(
    2i32,
    4i32,
    30i32,
    40i32,
    6i32,
    Rgba8{[r] 12i32, [g] 34i32, [b] 56i32, [a] 255i32}
  )
  commands.draw_text(
    7i32,
    9i32,
    14i32,
    Rgba8{[r] 255i32, [g] 240i32, [b] 0i32, [a] 255i32},
    "Hi!"utf8
  )
  dump_words(commands.serialize())
  return(commands.command_count())
}
)";
  const std::string srcPath = writeTemp("compile_native_ui_command_serialization.prime", source);
  const std::string exePath = (testScratchPath("") / "primec_native_ui_command_serialization").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_ui_command_serialization.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 2);
  CHECK(readFile(outPath) == "1,2,2,9,2,4,30,40,6,12,34,56,255,1,11,7,9,14,255,240,0,255,3,72,105,33\n");
}

TEST_CASE("compiles and runs native software renderer clip stack serialization deterministically") {
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
  [CommandList mut] commands{CommandList{}}
  commands.push_clip(1i32, 2i32, 20i32, 10i32)
  commands.draw_text(
    7i32,
    9i32,
    14i32,
    Rgba8{[r] 255i32, [g] 240i32, [b] 0i32, [a] 255i32},
    "Hi!"utf8
  )
  commands.push_clip(3i32, 4i32, 5i32, 6i32)
  commands.draw_rounded_rect(
    8i32,
    9i32,
    10i32,
    11i32,
    2i32,
    Rgba8{[r] 1i32, [g] 2i32, [b] 3i32, [a] 4i32}
  )
  commands.pop_clip()
  commands.pop_clip()
  commands.pop_clip()
  dump_words(commands.serialize())
  return(plus(commands.command_count(), commands.clip_depth()))
}
)";
  const std::string srcPath = writeTemp("compile_native_ui_clip_command_serialization.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_ui_clip_command_serialization").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_ui_clip_command_serialization.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 6);
  CHECK(readFile(outPath) ==
        "1,6,3,4,1,2,20,10,1,11,7,9,14,255,240,0,255,3,72,105,33,3,4,3,4,5,6,2,9,8,9,10,11,2,1,2,3,4,4,0,4,0\n");
}

TEST_CASE("compiles and runs native two-pass layout tree serialization deterministically") {
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
  [LayoutTree mut] tree{LayoutTree{}}
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
)";
  const std::string srcPath = writeTemp("compile_native_ui_layout_tree_serialization.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_ui_layout_tree_serialization").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_ui_layout_tree_serialization.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 6);
  CHECK(readFile(outPath) ==
        "1,6,2,-1,24,36,10,20,50,40,1,0,20,5,12,22,46,5,2,0,12,14,12,30,46,14,1,2,8,4,13,31,44,4,1,2,10,6,13,37,44,6,1,0,6,7,12,47,46,7\n");
}

TEST_CASE("compiles and runs native two-pass layout empty root deterministically") {
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
  [LayoutTree mut] tree{LayoutTree{}}
  [i32] root{tree.append_root_column(4i32, 9i32, 11i32, 13i32)}
  tree.measure()
  tree.arrange(3i32, 5i32, 11i32, 13i32)
  dump_words(tree.serialize())
  return(tree.node_count())
}
)";
  const std::string srcPath = writeTemp("compile_native_ui_layout_tree_empty_root.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_ui_layout_tree_empty_root").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_ui_layout_tree_empty_root.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 1);
  CHECK(readFile(outPath) == "1,1,2,-1,11,13,3,5,11,13\n");
}

TEST_CASE("compiles and runs native basic widget controls through layout deterministically") {
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
  [i32] root{layout.append_root_column(1i32, 2i32, 0i32, 0i32)}
  [i32] title{layout.append_label(root, 10i32, "Hi"utf8)}
  [i32] action{layout.append_button(root, 10i32, 3i32, "Go"utf8)}
  [i32] field{layout.append_input(root, 10i32, 2i32, 18i32, "abc"utf8)}
  layout.measure()
  layout.arrange(5i32, 6i32, 30i32, 46i32)

  [CommandList mut] commands{CommandList{}}
  commands.draw_label(layout, title, 10i32, Rgba8{[r] 1i32, [g] 2i32, [b] 3i32, [a] 255i32}, "Hi"utf8)
  commands.draw_button(
    layout,
    action,
    10i32,
    3i32,
    4i32,
    Rgba8{[r] 10i32, [g] 20i32, [b] 30i32, [a] 255i32},
    Rgba8{[r] 250i32, [g] 251i32, [b] 252i32, [a] 255i32},
    "Go"utf8
  )
  commands.draw_input(
    layout,
    field,
    10i32,
    2i32,
    3i32,
    Rgba8{[r] 40i32, [g] 50i32, [b] 60i32, [a] 255i32},
    Rgba8{[r] 200i32, [g] 210i32, [b] 220i32, [a] 255i32},
    "abc"utf8
  )
  dump_words(commands.serialize())
  return(plus(layout.node_count(), commands.command_count()))
}
)";
  const std::string srcPath = writeTemp("compile_native_ui_basic_widget_controls.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_ui_basic_widget_controls").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_ui_basic_widget_controls.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 9);
  CHECK(readFile(outPath) ==
        "1,5,1,10,6,7,10,1,2,3,255,2,72,105,2,9,6,19,28,16,4,10,20,30,255,1,10,9,22,10,250,251,252,255,2,71,111,2,9,6,37,28,14,3,40,50,60,255,1,11,8,39,10,200,210,220,255,3,97,98,99\n");
}

TEST_CASE("compiles and runs native panel container widget deterministically") {
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
  [i32] root{layout.append_root_column(1i32, 2i32, 0i32, 0i32)}
  [i32] title{layout.append_label(root, 10i32, "Top"utf8)}
  [i32] panel{layout.append_panel(root, 2i32, 1i32, 20i32, 12i32)}
  [i32] action{layout.append_button(panel, 10i32, 2i32, "Go"utf8)}
  [i32] field{layout.append_input(panel, 10i32, 1i32, 16i32, "abc"utf8)}
  [i32] footer{layout.append_label(root, 10i32, "!"utf8)}
  layout.measure()
  layout.arrange(4i32, 5i32, 28i32, 60i32)

  [CommandList mut] commands{CommandList{}}
  commands.draw_label(layout, title, 10i32, Rgba8{[r] 1i32, [g] 2i32, [b] 3i32, [a] 255i32}, "Top"utf8)
  commands.begin_panel(layout, panel, 4i32, Rgba8{[r] 8i32, [g] 9i32, [b] 10i32, [a] 255i32})
  commands.draw_button(
    layout,
    action,
    10i32,
    2i32,
    3i32,
    Rgba8{[r] 20i32, [g] 30i32, [b] 40i32, [a] 255i32},
    Rgba8{[r] 200i32, [g] 201i32, [b] 202i32, [a] 255i32},
    "Go"utf8
  )
  commands.draw_input(
    layout,
    field,
    10i32,
    1i32,
    2i32,
    Rgba8{[r] 50i32, [g] 60i32, [b] 70i32, [a] 255i32},
    Rgba8{[r] 210i32, [g] 211i32, [b] 212i32, [a] 255i32},
    "abc"utf8
  )
  commands.end_panel()
  commands.draw_label(layout, footer, 10i32, Rgba8{[r] 1i32, [g] 2i32, [b] 3i32, [a] 255i32}, "!"utf8)
  dump_words(commands.serialize())
  return(plus(layout.node_count(), plus(commands.command_count(), commands.clip_depth())))
}
)";
  const std::string srcPath = writeTemp("compile_native_ui_panel_widget.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_ui_panel_widget").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_ui_panel_widget.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 15);
  CHECK(readFile(outPath) ==
        "1,9,1,11,5,6,10,1,2,3,255,3,84,111,112,2,9,5,18,26,31,4,8,9,10,255,3,4,7,20,22,27,2,9,7,20,22,14,3,20,30,40,255,1,10,9,22,10,200,201,202,255,2,71,111,2,9,7,35,22,12,2,50,60,70,255,1,11,8,36,10,210,211,212,255,3,97,98,99,4,0,1,9,5,51,10,1,2,3,255,1,33\n");
}

TEST_CASE("compiles and runs native empty panel container stays balanced deterministically") {
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
  [i32] root{layout.append_root_column(0i32, 0i32, 0i32, 0i32)}
  [i32] panel{layout.append_panel(root, 3i32, 1i32, 12i32, 10i32)}
  layout.measure()
  layout.arrange(2i32, 3i32, 20i32, 18i32)

  [CommandList mut] commands{CommandList{}}
  commands.begin_panel(layout, panel, 5i32, Rgba8{[r] 9i32, [g] 8i32, [b] 7i32, [a] 255i32})
  commands.end_panel()
  dump_words(commands.serialize())
  return(plus(layout.node_count(), plus(commands.command_count(), commands.clip_depth())))
}
)";
  const std::string srcPath = writeTemp("compile_native_ui_empty_panel_widget.prime", source);
  const std::string exePath =
      (testScratchPath("") / "primec_native_ui_empty_panel_widget").string();
  const std::string outPath =
      (testScratchPath("") / "primec_native_ui_empty_panel_widget.txt").string();

  const std::string compileCmd = "./primec --emit=native " + srcPath + " -o " + exePath + " --entry /main";
  CHECK(runCommand(compileCmd) == 0);
  CHECK(runCommand(exePath + " > " + outPath) == 5);
  CHECK(readFile(outPath) == "1,3,2,9,2,3,20,10,5,9,8,7,255,3,4,5,6,14,4,4,0\n");
}

TEST_SUITE_END();
#endif
