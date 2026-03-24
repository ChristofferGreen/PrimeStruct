#include <cerrno>

TEST_SUITE_BEGIN("primestruct.compile.run.vm.core");

TEST_CASE("runs vm with method call result") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  inc([i32] value) {
    return(plus(value, 1i32))
  }
}

[return<int>]
main() {
  [i32] value{5i32}
  return(plus(value.inc(), 2i32))
}
)";
  const std::string srcPath = writeTemp("vm_method_call.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 8);
}

TEST_CASE("vm supports support-matrix binding types") {
  const std::string source = R"(
import /std/collections/*

[return<int> effects(heap_alloc)]
main() {
  [i32] i{1i32}
  [i64] j{2i64}
  [u64] k{3u64}
  [bool] b{true}
  [f32] x{1.5f32}
  [f64] y{2.5f64}
  [array<i32>] arr{array<i32>(1i32, 2i32, 3i32)}
  [vector<i32>] vec{vector<i32>(4i32, 5i32)}
  [map<i32, i32>] table{map<i32, i32>(1i32, 7i32)}
  [string] text{"ok"utf8}
  return(plus(
    plus(
      plus(convert<int>(i), convert<int>(j)),
      plus(convert<int>(k), if(b, then() { 1i32 }, else() { 0i32 }))
    ),
    plus(
      plus(convert<int>(x), convert<int>(y)),
      plus(count(arr), plus(vectorCount<i32>(vec), plus(mapCount<i32, i32>(table), count(text))))
    )
  ))
}
)";
  const std::string srcPath = writeTemp("vm_support_matrix_types.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 18);
}

TEST_CASE("runs vm with raw string literal output") {
  const std::string source = R"(
[return<int> effects(io_out)]
main() {
  print_line("line\\nnext"raw_utf8)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_raw_string_literal.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_vm_raw_string_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "line\\\\nnext\n");
}

TEST_CASE("runs vm with raw single-quoted string literal output") {
  const std::string source = R"(
[return<int> effects(io_out)]
main() {
  print_line('line\\nnext'raw_utf8)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_raw_string_literal_single.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_vm_raw_string_single_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "line\\\\nnext\n");
}

TEST_CASE("runs vm with string literal indexing") {
  const std::string source = R"(
[return<int>]
main() {
  return(at("hello"utf8, 1i32))
}
)";
  const std::string srcPath = writeTemp("vm_string_literal_index.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 101);
}

TEST_CASE("runs vm software renderer command serialization deterministically") {
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
)";
  const std::string srcPath = writeTemp("vm_ui_command_serialization.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_vm_ui_command_serialization.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(outPath) == "1,2,2,9,2,4,30,40,6,12,34,56,255,1,11,7,9,14,255,240,0,255,3,72,105,33\n");
}

TEST_CASE("runs vm software renderer clip stack serialization deterministically") {
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
)";
  const std::string srcPath = writeTemp("vm_ui_clip_command_serialization.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_vm_ui_clip_command_serialization.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 6);
  CHECK(readFile(outPath) ==
        "1,6,3,4,1,2,20,10,1,11,7,9,14,255,240,0,255,3,72,105,33,3,4,3,4,5,6,2,9,8,9,10,11,2,1,2,3,4,4,0,4,0\n");
}

TEST_CASE("runs vm software renderer clip underflow stays deterministic") {
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
  [CommandList mut] commands{CommandList()}
  commands.pop_clip()
  commands.draw_text(
    1i32,
    2i32,
    12i32,
    Rgba8([r] 9i32, [g] 8i32, [b] 7i32, [a] 6i32),
    ""utf8
  )
  commands.pop_clip()
  dump_words(commands.serialize())
  return(plus(commands.command_count(), commands.clip_depth()))
}
)";
  const std::string srcPath = writeTemp("vm_ui_clip_underflow_serialization.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_vm_ui_clip_underflow_serialization.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 1);
  CHECK(readFile(outPath) == "1,1,1,8,1,2,12,9,8,7,6,0\n");
}

TEST_CASE("runs vm software renderer empty text serialization deterministically") {
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
  [CommandList mut] commands{CommandList()}
  commands.draw_text(
    1i32,
    2i32,
    12i32,
    Rgba8([r] 9i32, [g] 8i32, [b] 7i32, [a] 6i32),
    ""utf8
  )
  dump_words(commands.serialize())
  return(commands.command_count())
}
)";
  const std::string srcPath = writeTemp("vm_ui_empty_text_serialization.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_vm_ui_empty_text_serialization.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 1);
  CHECK(readFile(outPath) == "1,1,1,8,1,2,12,9,8,7,6,0\n");
}

TEST_CASE("runs vm two-pass layout tree serialization deterministically") {
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
)";
  const std::string srcPath = writeTemp("vm_ui_layout_tree_serialization.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_vm_ui_layout_tree_serialization.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 6);
  CHECK(readFile(outPath) ==
        "1,6,2,-1,24,36,10,20,50,40,1,0,20,5,12,22,46,5,2,0,12,14,12,30,46,14,1,2,8,4,13,31,44,4,1,2,10,6,13,37,44,6,1,0,6,7,12,47,46,7\n");
}

TEST_CASE("runs vm two-pass layout empty root deterministically") {
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
  [LayoutTree mut] tree{LayoutTree()}
  [i32] root{tree.append_root_column(4i32, 9i32, 11i32, 13i32)}
  tree.measure()
  tree.arrange(3i32, 5i32, 11i32, 13i32)
  dump_words(tree.serialize())
  return(tree.node_count())
}
)";
  const std::string srcPath = writeTemp("vm_ui_layout_tree_empty_root.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_vm_ui_layout_tree_empty_root.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 1);
  CHECK(readFile(outPath) == "1,1,2,-1,11,13,3,5,11,13\n");
}

TEST_CASE("runs vm basic widget controls through layout deterministically") {
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
)";
  const std::string srcPath = writeTemp("vm_ui_basic_widget_controls.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_vm_ui_basic_widget_controls.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 9);
  CHECK(readFile(outPath) ==
        "1,5,1,10,6,7,10,1,2,3,255,2,72,105,2,9,6,19,28,16,4,10,20,30,255,1,10,9,22,10,250,251,252,255,2,71,111,2,9,6,37,28,14,3,40,50,60,255,1,11,8,39,10,200,210,220,255,3,97,98,99\n");
}

TEST_CASE("runs vm panel container widget deterministically") {
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
)";
  const std::string srcPath = writeTemp("vm_ui_panel_widget.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_vm_ui_panel_widget.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 15);
  CHECK(readFile(outPath) ==
        "1,9,1,11,5,6,10,1,2,3,255,3,84,111,112,2,9,5,18,26,31,4,8,9,10,255,3,4,7,20,22,27,2,9,7,20,22,14,3,20,30,40,255,1,10,9,22,10,200,201,202,255,2,71,111,2,9,7,35,22,12,2,50,60,70,255,1,11,8,36,10,210,211,212,255,3,97,98,99,4,0,1,9,5,51,10,1,2,3,255,1,33\n");
}

TEST_CASE("runs vm empty panel container stays balanced deterministically") {
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
)";
  const std::string srcPath = writeTemp("vm_ui_empty_panel_widget.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_vm_ui_empty_panel_widget.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 5);
  CHECK(readFile(outPath) == "1,3,2,9,2,3,20,10,5,9,8,7,255,3,4,5,6,14,4,4,0\n");
}

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
)";
  const std::string srcPath = writeTemp("vm_ui_composite_login_form.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_vm_ui_composite_login_form.txt").string();
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
)";
  const std::string srcPath = writeTemp("vm_ui_html_login_form.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_vm_ui_html_login_form.txt").string();
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
)";
  const std::string srcPath = writeTemp("vm_ui_event_stream.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_vm_ui_event_stream.txt").string();
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
)";
  const std::string srcPath = writeTemp("vm_ui_ime_event_stream.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_vm_ui_ime_event_stream.txt").string();
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
)";
  const std::string srcPath = writeTemp("vm_ui_resize_focus_event_stream.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_vm_ui_resize_focus_event_stream.txt").string();
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

TEST_CASE("runs vm with heap alloc intrinsic") {
  const std::string source = R"(
[return<int> effects(heap_alloc)]
main() {
  [mut] ptr{/std/intrinsics/memory/alloc<i32>(1i32)}
  assign(dereference(ptr), 9i32)
  return(dereference(ptr))
}
)";
  const std::string srcPath = writeTemp("vm_heap_alloc_intrinsic.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 9);
}

TEST_CASE("runs vm with heap free intrinsic") {
  const std::string source = R"(
[return<int> effects(heap_alloc)]
main() {
  [mut] ptr{/std/intrinsics/memory/alloc<i32>(1i32)}
  assign(dereference(ptr), 9i32)
  [i32] value{dereference(ptr)}
  /std/intrinsics/memory/free(ptr)
  return(value)
}
)";
  const std::string srcPath = writeTemp("vm_heap_free_intrinsic.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 9);
}

TEST_CASE("runs vm with heap realloc intrinsic") {
  const std::string source = R"(
[return<int> effects(heap_alloc)]
main() {
  [mut] ptr{/std/intrinsics/memory/alloc<i32>(1i32)}
  assign(dereference(ptr), 9i32)
  [Pointer<i32> mut] grown{/std/intrinsics/memory/realloc(ptr, 2i32)}
  assign(dereference(plus(grown, 16i32)), 4i32)
  [i32] sum{plus(dereference(grown), dereference(plus(grown, 16i32)))}
  /std/intrinsics/memory/free(grown)
  return(sum)
}
)";
  const std::string srcPath = writeTemp("vm_heap_realloc_intrinsic.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 13);
}

TEST_CASE("runs vm with checked memory at intrinsic") {
  const std::string source = R"(
[return<int> effects(heap_alloc)]
main() {
  [mut] ptr{/std/intrinsics/memory/alloc<i32>(2i32)}
  assign(dereference(ptr), 9i32)
  [mut] second{/std/intrinsics/memory/at(ptr, 1i32, 2i32)}
  assign(dereference(second), 4i32)
  [i32] sum{plus(dereference(ptr), dereference(second))}
  /std/intrinsics/memory/free(ptr)
  return(sum)
}
)";
  const std::string srcPath = writeTemp("vm_heap_at_intrinsic.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 13);
}

TEST_CASE("runs vm with unchecked memory at intrinsic") {
  const std::string source = R"(
[return<int> effects(heap_alloc)]
main() {
  [mut] ptr{/std/intrinsics/memory/alloc<i32>(2i32)}
  assign(dereference(ptr), 9i32)
  [mut] second{/std/intrinsics/memory/at_unsafe(ptr, 1i32)}
  assign(dereference(second), 4i32)
  [i32] sum{plus(dereference(ptr), dereference(second))}
  /std/intrinsics/memory/free(ptr)
  return(sum)
}
)";
  const std::string srcPath = writeTemp("vm_heap_at_unsafe_intrinsic.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 13);
}

TEST_CASE("vm rejects checked memory at out of bounds") {
  const std::string source = R"(
[return<int> effects(heap_alloc)]
main() {
  [mut] ptr{/std/intrinsics/memory/alloc<i32>(1i32)}
  return(dereference(/std/intrinsics/memory/at(ptr, 1i32, 1i32)))
}
)";
  const std::string srcPath = writeTemp("vm_heap_at_out_of_bounds.prime", source);
  const std::string errPath = (testScratchPath("") / "primec_vm_heap_at_out_of_bounds.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) != 0);
  CHECK(readFile(errPath) == "pointer index out of bounds\n");
}

TEST_CASE("vm rejects dereference after heap free intrinsic") {
  const std::string source = R"(
[return<int> effects(heap_alloc)]
main() {
  [mut] ptr{/std/intrinsics/memory/alloc<i32>(1i32)}
  /std/intrinsics/memory/free(ptr)
  return(dereference(ptr))
}
)";
  const std::string srcPath = writeTemp("vm_heap_free_invalid_deref.prime", source);
  const std::string errPath = (testScratchPath("") / "primec_vm_heap_free_invalid_deref.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) != 0);
  CHECK(readFile(errPath).find("invalid indirect address in IR") != std::string::npos);
}

TEST_CASE("runs vm with match cases") {
  const std::string source = R"(
[return<int>]
main() {
  [i32] value{2i32}
  return(match(value, case(1i32) { 10i32 }, case(2i32) { 20i32 }, else() { 30i32 }))
}
)";
  const std::string srcPath = writeTemp("vm_match_cases.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 20);
}

TEST_CASE("runs vm with chained method calls") {
  const std::string source = R"(
namespace i32 {
  [return<int>]
  inc([i32] value) {
    return(plus(value, 1i32))
  }

  [return<int>]
  dec([i32] value) {
    return(minus(value, 1i32))
  }
}

[return<int>]
make() {
  return(4i32)
}

[return<int>]
main() {
  return(make().inc().dec())
}
)";
  const std::string srcPath = writeTemp("vm_method_chain.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 4);
}

TEST_CASE("runs vm with import alias") {
  const std::string source = R"(
import /util
namespace util {
  [public return<int>]
  inc([i32] value) {
    return(plus(value, 1i32))
  }
}
[return<int>]
main() {
  return(inc(4i32))
}
)";
  const std::string srcPath = writeTemp("vm_import_alias.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 5);
}

TEST_CASE("runs vm with multiple imports") {
  const std::string source = R"(
import /util, /std/math/*
namespace util {
  [public return<int>]
  add([i32] a, [i32] b) {
    return(plus(a, b))
  }
}
[return<int>]
main() {
  return(plus(add(2i32, 3i32), min(7i32, 3i32)))
}
)";
  const std::string srcPath = writeTemp("vm_import_multiple.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 8);
}

TEST_CASE("runs vm with whitespace-separated imports") {
  const std::string source = R"(
import /util /std/math/*
namespace util {
  [public return<int>]
  add([i32] a, [i32] b) {
    return(plus(a, b))
  }
}
[return<int>]
main() {
  return(plus(add(2i32, 3i32), min(7i32, 3i32)))
}
)";
  const std::string srcPath = writeTemp("vm_import_whitespace.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 8);
}

TEST_CASE("runs vm with capabilities and default effects") {
  const std::string source = R"(
[return<int> capabilities(io_out)]
main() {
  print_line("capabilities"utf8)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_capabilities_default_effects.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_vm_capabilities_out.txt").string();
  const std::string runCmd =
      "./primec --emit=vm " + srcPath + " --entry /main --default-effects=default > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "capabilities\n");
}

TEST_CASE("default effects none requires explicit effects in vm") {
  const std::string source = R"(
[return<int>]
main() {
  print_line("no effects"utf8)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_print_no_effects.prime", source);
  const std::string errPath = (testScratchPath("") / "primec_vm_print_no_effects_err.txt").string();
  const std::string runCmd =
      "./primec --emit=vm " + srcPath + " --entry /main --default-effects=none 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("print_line requires io_out effect") != std::string::npos);
}

TEST_CASE("default effects token does not enable io_err output in vm") {
  const std::string source = R"(
[return<int>]
main() {
  print_line_error("no stderr"utf8)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_default_effects_no_io_err.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_vm_default_effects_no_io_err_err.txt").string();
  const std::string runCmd =
      "./primec --emit=vm " + srcPath + " --entry /main --default-effects=default 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("print_line_error requires io_err effect") != std::string::npos);
}

TEST_CASE("runs vm with implicit utf8 strings") {
  const std::string source = R"(
[return<int> effects(io_out)]
main() {
  print_line("implicit")
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_implicit_utf8.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_vm_implicit_utf8_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "implicit\n");
}

TEST_CASE("runs vm with implicit utf8 single-quoted strings") {
  const std::string source = R"(
[return<int> effects(io_out)]
main() {
  print_line('implicit')
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_implicit_utf8_single.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_vm_implicit_utf8_single_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "implicit\n");
}

TEST_CASE("runs vm with escaped utf8 strings") {
  const std::string source = R"(
[return<int> effects(io_out)]
main() {
  print_line("line\nnext"utf8)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_utf8_escaped.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_vm_utf8_escaped_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "line\nnext\n");
}

TEST_CASE("runs vm with raw utf8 single-quoted strings") {
  const std::string source = R"(
[return<int> effects(io_out)]
main() {
  print_line('line\nnext'utf8)
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_utf8_escaped_single.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_vm_utf8_escaped_single_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "line\\nnext\n");
}

TEST_CASE("vm supports string return types") {
  const std::string source = R"(
[return<string>]
message() {
  return("hi"utf8)
}

[return<int> effects(io_out)]
main() {
  print_line(message())
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_string_return.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_vm_string_return_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "hi\n");
}

TEST_CASE("vm supports Result.why hooks") {
  const std::string source = R"(
[struct]
MyError() {
  [i32] code{0i32}
}

namespace MyError {
  [return<string>]
  why([MyError] err) {
    return("custom error"utf8)
  }
}

[return<Result<MyError>>]
make_error() {
  return(1i32)
}

[return<int> effects(io_out)]
main() {
  print_line(Result.why(make_error()))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_result_why_custom.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_vm_result_why_custom_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "custom error\n");
}

TEST_CASE("vm supports stdlib FileError result helpers") {
  const std::string source = R"(
import /std/file/*

[return<Result<FileError>>]
make_status() {
  return(FileError.status(fileReadEof()))
}

[return<Result<i32, FileError>>]
make_value() {
  return(FileError.result<i32>(fileReadEof()))
}

[return<int> effects(io_out)]
main() {
  [Result<FileError>] status{make_status()}
  [Result<i32, FileError>] valueStatus{make_value()}
  [FileError] err{fileReadEof()}
  [Result<FileError>] methodStatus{FileError.status(err)}
  [Result<i32, FileError>] methodValueStatus{FileError.result<i32>(err)}
  [bool] eof{fileErrorIsEof(fileReadEof())}
  [bool] otherEof{fileErrorIsEof(1i32)}
  [bool] directStatusError{Result.error(status)}
  [bool] methodStatusError{Result.error(methodStatus)}
  if(not(Result.error(status))) {
    return(1i32)
  }
  if(not(Result.error(valueStatus))) {
    return(2i32)
  }
  if(not(eof)) {
    return(3i32)
  }
  if(otherEof) {
    return(4i32)
  }
  if(not(directStatusError)) {
    return(5i32)
  }
  if(not(methodStatusError)) {
    return(6i32)
  }
  print_line(Result.why(status))
  print_line(Result.why(valueStatus))
  print_line(Result.why(methodStatus))
  print_line(Result.why(methodValueStatus))
  print_line(Result.why(FileError.status(fileReadEof())))
  print_line(Result.why(FileError.result<i32>(fileReadEof())))
  print_line(Result.why(methodStatus))
  print_line(Result.why(methodValueStatus))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_file_error_result_helpers.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_vm_stdlib_file_error_result_helpers_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "EOF\nEOF\nEOF\nEOF\nEOF\nEOF\nEOF\nEOF\n");
}

TEST_CASE("vm supports Result.map on IR-backed path") {
  const std::string source = R"(
import /std/file/*

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(err.why())
}

[return<int> effects(io_out, io_err) on_error<FileError, /log_file_error>]
main() {
  [Result<i32, FileError>] ok{Result.ok(2i32)}
  [Result<i32, FileError>] failed{FileError.result<i32>(fileReadEof())}
  [Result<i32, FileError>] mappedOk{
    Result.map(ok, []([i32] value) { return(multiply(value, 4i32)) })
  }
  [Result<i32, FileError>] mappedFailed{
    Result.map(failed, []([i32] value) { return(multiply(value, 4i32)) })
  }
  if(Result.error(mappedOk)) {
    return(1i32)
  }
  if(not(Result.error(mappedFailed))) {
    return(2i32)
  }
  print_line(Result.why(mappedFailed))
  return(try(mappedOk))
}
)";
  const std::string srcPath = writeTemp("vm_result_map_ir_backed.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_vm_result_map_ir_backed_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 8);
  CHECK(readFile(outPath) == "EOF\n");
}

TEST_CASE("vm supports Result.and_then on IR-backed path") {
  const std::string source = R"(
import /std/file/*

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(err.why())
}

[return<int> effects(io_out, io_err) on_error<FileError, /log_file_error>]
main() {
  [Result<i32, FileError>] ok{Result.ok(2i32)}
  [Result<i32, FileError>] failed{FileError.result<i32>(fileReadEof())}
  [Result<i32, FileError>] chainedOk{
    Result.and_then(ok, []([i32] value) { return(Result.ok(multiply(value, 4i32))) })
  }
  [Result<FileError>] chainedStatus{
    Result.and_then(ok, []([i32] value) { return(FileError.status(FileError.eof())) })
  }
  [Result<i32, FileError>] chainedFailed{
    Result.and_then(failed, []([i32] value) { return(Result.ok(multiply(value, 4i32))) })
  }
  if(Result.error(chainedOk)) {
    return(1i32)
  }
  if(not(Result.error(chainedStatus))) {
    return(2i32)
  }
  if(not(Result.error(chainedFailed))) {
    return(3i32)
  }
  print_line(Result.why(chainedStatus))
  print_line(Result.why(chainedFailed))
  return(try(chainedOk))
}
)";
  const std::string srcPath = writeTemp("vm_result_and_then_ir_backed.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_vm_result_and_then_ir_backed_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 8);
  CHECK(readFile(outPath) == "EOF\nEOF\n");
}

TEST_CASE("vm supports Result.map2 on IR-backed path") {
  const std::string source = R"(
import /std/collections/*

swallow_container_error([ContainerError] err) {}

[return<int> effects(io_out) on_error<ContainerError, /swallow_container_error>]
main() {
  [Result<i32, ContainerError>] first{Result.ok(2i32)}
  [Result<i32, ContainerError>] second{Result.ok(3i32)}
  [Result<i32, ContainerError>] leftFailed{/ContainerError/result<i32>(/ContainerError/missing_key())}
  [Result<i32, ContainerError>] rightFailed{/ContainerError/result<i32>(/ContainerError/empty())}
  [Result<i32, ContainerError>] summed{
    Result.map2(first, second, []([i32] left, [i32] right) { return(plus(left, right)) })
  }
  [Result<i32, ContainerError>] firstError{
    Result.map2(leftFailed, rightFailed, []([i32] left, [i32] right) { return(plus(left, right)) })
  }
  [Result<i32, ContainerError>] secondError{
    Result.map2(first, rightFailed, []([i32] left, [i32] right) { return(plus(left, right)) })
  }
  if(Result.error(summed)) {
    return(1i32)
  }
  if(not(Result.error(firstError))) {
    return(2i32)
  }
  if(not(Result.error(secondError))) {
    return(3i32)
  }
  print_line(Result.why(firstError))
  print_line(Result.why(secondError))
  return(try(summed))
}
)";
  const std::string srcPath = writeTemp("vm_result_map2_ir_backed.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_vm_result_map2_ir_backed_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 5);
  CHECK(readFile(outPath) == "container missing key\ncontainer empty\n");
}

TEST_CASE("vm supports f32 Result payloads on IR-backed paths") {
  const std::string source = R"(
import /std/file/*

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(err.why())
}

[return<int> effects(io_out, io_err) on_error<FileError, /log_file_error>]
main() {
  [Result<f32, FileError>] ok{Result.ok(1.5f32)}
  [Result<f32, FileError>] mapped{
    Result.map(ok, []([f32] value) { return(plus(value, 1.0f32)) })
  }
  [Result<f32, FileError>] chained{
    Result.and_then(mapped, []([f32] value) { return(Result.ok(multiply(value, 2.0f32))) })
  }
  [Result<f32, FileError>] summed{
    Result.map2(chained, Result.ok(0.5f32), []([f32] left, [f32] right) { return(plus(left, right)) })
  }
  [f32] value{summed?}
  print_line(convert<int>(multiply(value, 10.0f32)))
  return(convert<int>(multiply(value, 10.0f32)))
}
)";
  const std::string srcPath = writeTemp("vm_result_f32_ir_backed.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_vm_result_f32_ir_backed_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 55);
  CHECK(readFile(outPath) == "55\n");
}

TEST_CASE("vm supports direct Result.ok combinator sources on IR-backed paths") {
  const std::string source = R"(
import /std/file/*

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(err.why())
}

[return<int> effects(io_out, io_err) on_error<FileError, /log_file_error>]
main() {
  [Result<i32, FileError>] mapped{
    Result.map(Result.ok(2i32), []([i32] value) { return(multiply(value, 4i32)) })
  }
  [Result<i32, FileError>] chained{
    Result.and_then(Result.ok(2i32), []([i32] value) { return(Result.ok(plus(value, 3i32))) })
  }
  [Result<i32, FileError>] summed{
    Result.map2(Result.ok(2i32), Result.ok(3i32), []([i32] left, [i32] right) { return(plus(left, right)) })
  }
  print_line(try(mapped))
  print_line(try(chained))
  return(try(summed))
}
)";
  const std::string srcPath = writeTemp("vm_result_direct_ok_ir_backed.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_vm_result_direct_ok_ir_backed_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 5);
  CHECK(readFile(outPath) == "8\n5\n");
}

TEST_CASE("vm supports packed error struct Result payloads on IR-backed paths") {
  const std::string source = R"(
import /std/file/*
import /std/collections/*
import /std/image/*
import /std/gfx/*

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(err.why())
}

[return<int> effects(io_out, io_err) on_error<FileError, /log_file_error>]
main() {
  [Result<ContainerError, FileError>] mappedContainer{
    Result.map(Result.ok(/ContainerError/missing_key()),
      []([ContainerError] value) { return(/ContainerError/capacity_exceeded()) })
  }
  [Result<ImageError, FileError>] chainedImage{
    Result.and_then(Result.ok(/ImageError/read_unsupported()),
      []([ImageError] value) { return(Result.ok(/ImageError/invalid_operation())) })
  }
  [Result<GfxError, FileError>] summedGfx{
    Result.map2(Result.ok(GfxError.frame_acquire_failed()),
      Result.ok(GfxError.queue_submit_failed()),
      []([GfxError] left, [GfxError] right) { return(right) })
  }
  [ContainerError] container{try(mappedContainer)}
  [ImageError] image{try(chainedImage)}
  [GfxError] gfx{try(summedGfx)}
  print_line(container.why())
  print_line(image.why())
  print_line(gfx.why())
  return(plus(container.code, plus(image.code, gfx.code)))
}
)";
  const std::string srcPath = writeTemp("vm_result_packed_error_payloads_ir_backed.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_result_packed_error_payloads_ir_backed_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 15);
  CHECK(readFile(outPath) ==
        "container capacity exceeded\nimage_invalid_operation\nqueue_submit_failed\n");
}

TEST_CASE("vm supports direct single-slot struct Result.ok payloads on IR-backed paths") {
  const std::string source = R"(
import /std/file/*

[struct]
Label() {
  [i32] code{0i32}
}

[return<Result<Label, FileError>>]
make_label() {
  return(Result.ok(Label([code] 7i32)))
}

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(err.why())
}

[return<int> effects(io_out, io_err) on_error<FileError, /log_file_error>]
main() {
  [Label] value{try(make_label())}
  print_line(value.code)
  return(value.code)
}
)";
  const std::string srcPath = writeTemp("vm_result_single_slot_struct_payload_ir_backed.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_result_single_slot_struct_payload_ir_backed_out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 7);
  CHECK(readFile(outPath) == "7\n");
}

TEST_CASE("vm supports single-slot struct Result combinator payloads on IR-backed paths") {
  const std::string source = R"(
import /std/file/*

[struct]
Label() {
  [i32] code{0i32}
}

[return<Result<Label, FileError>>]
make_label([i32] code) {
  return(Result.ok(Label([code] code)))
}

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(err.why())
}

[return<int> effects(io_out, io_err) on_error<FileError, /log_file_error>]
main() {
  print_line(try(Result.map(make_label(2i32), []([Label] value) {
    return(Label([code] plus(value.code, 5i32)))
  })).code)
  print_line(try(Result.and_then(make_label(2i32), []([Label] value) {
    return(Result.ok(Label([code] plus(value.code, 3i32))))
  })).code)
  print_line(try(Result.map2(make_label(2i32), make_label(5i32), []([Label] left, [Label] right) {
    return(Label([code] plus(left.code, right.code)))
  })).code)
  return(19i32)
}
)";
  const std::string srcPath = writeTemp("vm_result_single_slot_struct_combinators_ir_backed.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_result_single_slot_struct_combinators_ir_backed_out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 19);
  CHECK(readFile(outPath) == "7\n5\n7\n");
}

TEST_CASE("vm supports File Result payloads on IR-backed paths") {
  const std::string filePath =
      (std::filesystem::temp_directory_path() / "primec_vm_result_file_payload_ir_backed.txt").string();
  {
    std::ofstream file(filePath, std::ios::binary);
    REQUIRE(file.good());
    file.write("ABC", 3);
    REQUIRE(file.good());
  }
  auto escape = [](const std::string &text) {
    std::string out;
    out.reserve(text.size());
    for (char c : text) {
      if (c == '\\' || c == '"') {
        out.push_back('\\');
      }
      out.push_back(c);
    }
    return out;
  };
  const std::string escapedPath = escape(filePath);
  const std::string source =
      "import /std/file/*\n"
      "[return<Result<File<Read>, FileError>> effects(file_read)]\n"
      "open_file([string] path) {\n"
      "  [File<Read>] file{ File<Read>(path)? }\n"
      "  return(Result.ok(file))\n"
      "}\n"
      "[effects(io_err)]\n"
      "log_file_error([FileError] err) {\n"
      "  print_line_error(err.why())\n"
      "}\n"
      "[return<int> effects(file_read, io_out, io_err) on_error<FileError, /log_file_error>]\n"
      "main() {\n"
      "  [Result<File<Read>, FileError>] mapped{\n"
      "    Result.map(open_file(\"" + escapedPath +
      "\"utf8), []([File<Read>] file) { return(file) })\n"
      "  }\n"
      "  [Result<File<Read>, FileError>] chained{\n"
      "    Result.and_then(open_file(\"" + escapedPath +
      "\"utf8), []([File<Read>] file) { return(Result.ok(file)) })\n"
      "  }\n"
      "  [Result<File<Read>, FileError>] summed{\n"
      "    Result.map2(open_file(\"" + escapedPath + "\"utf8), open_file(\"" + escapedPath +
      "\"utf8), []([File<Read>] left, [File<Read>] right) { return(left) })\n"
      "  }\n"
      "  [File<Read>] mappedFile{try(mapped)}\n"
      "  [File<Read>] chainedFile{try(chained)}\n"
      "  [File<Read>] summedFile{try(summed)}\n"
      "  [i32 mut] first{0i32}\n"
      "  [i32 mut] second{0i32}\n"
      "  [i32 mut] third{0i32}\n"
      "  mappedFile.read_byte(first)?\n"
      "  chainedFile.read_byte(second)?\n"
      "  summedFile.read_byte(third)?\n"
      "  print_line(first)\n"
      "  print_line(second)\n"
      "  print_line(third)\n"
      "  mappedFile.close()?\n"
      "  chainedFile.close()?\n"
      "  summedFile.close()?\n"
      "  return(plus(first, plus(second, third)))\n"
      "}\n";
  const std::string srcPath = writeTemp("vm_result_file_payload_ir_backed.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_result_file_payload_ir_backed_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 195);
  CHECK(readFile(outPath) == "65\n65\n65\n");
}

TEST_CASE("vm supports multi-slot struct Result payloads on IR-backed paths") {
  const std::string source = R"(
import /std/file/*

[struct]
Pair() {
  [i32] left{0i32}
  [i32] right{0i32}
}

[return<Result<Pair, FileError>>]
make_pair([i32] left, [i32] right) {
  return(Result.ok(Pair([left] left, [right] right)))
}

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(err.why())
}

[return<int> effects(io_out, io_err) on_error<FileError, /log_file_error>]
main() {
  [Pair] direct{try(make_pair(1i32, 2i32))}
  [Pair] mapped{try(Result.map(make_pair(2i32, 3i32), []([Pair] value) {
    return(Pair([left] plus(value.left, 5i32), [right] plus(value.right, 7i32)))
  }))}
  [Pair] chained{try(Result.and_then(make_pair(2i32, 3i32), []([Pair] value) {
    return(Result.ok(Pair([left] plus(value.left, value.right), [right] 9i32)))
  }))}
  [Pair] summed{try(Result.map2(make_pair(1i32, 4i32), make_pair(2i32, 5i32), []([Pair] left, [Pair] right) {
    return(Pair([left] plus(left.left, right.left), [right] plus(left.right, right.right)))
  }))}
  print_line(direct.left)
  print_line(direct.right)
  print_line(mapped.left)
  print_line(mapped.right)
  print_line(chained.left)
  print_line(chained.right)
  print_line(summed.left)
  print_line(summed.right)
  return(plus(direct.left,
    plus(direct.right,
      plus(mapped.left,
        plus(mapped.right,
          plus(chained.left, plus(chained.right, plus(summed.left, summed.right))))))))
}
)";
  const std::string srcPath = writeTemp("vm_result_multi_slot_struct_payload_ir_backed.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_result_multi_slot_struct_payload_ir_backed_out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 36);
  CHECK(readFile(outPath) == "1\n2\n7\n10\n5\n9\n3\n9\n");
}

TEST_CASE("vm supports array and vector Result payloads on IR-backed paths") {
  const std::string source = R"(
import /std/file/*

[return<Result<array<i32>, FileError>>]
make_numbers() {
  [array<i32>] values{array<i32>(1i32, 2i32, 3i32)}
  return(Result.ok(values))
}

[return<Result<vector<i32>, FileError>>]
make_vector() {
  return(Result.ok(vector<i32>(4i32, 5i32)))
}

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(err.why())
}

[return<int> effects(io_out, io_err) on_error<FileError, /log_file_error>]
main() {
  [array<i32>] direct{try(make_numbers())}
  [array<i32>] mapped{try(Result.map(make_numbers(), []([array<i32>] values) {
    return(values)
  }))}
  [vector<i32>] chained{try(Result.and_then(make_vector(), []([vector<i32>] values) {
    return(Result.ok(values))
  }))}
  [array<i32>] summed{try(Result.map2(make_numbers(), make_numbers(), []([array<i32>] left, [array<i32>] right) {
    return(right)
  }))}
  print_line(count(direct))
  print_line(direct[0i32])
  print_line(mapped[1i32])
  print_line(chained[1i32])
  print_line(count(summed))
  print_line(summed[2i32])
  return(plus(direct[0i32], plus(mapped[1i32], plus(chained[1i32], summed[2i32]))))
}
)";
  const std::string srcPath = writeTemp("vm_result_array_vector_payload_ir_backed.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_result_array_vector_payload_ir_backed_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 11);
  CHECK(readFile(outPath) == "3\n1\n2\n5\n3\n3\n");
}

TEST_CASE("vm supports block-bodied Result.and_then lambdas on IR-backed paths") {
  const std::string source = R"(
import /std/file/*

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(err.why())
}

[return<int> effects(io_err) on_error<FileError, /log_file_error>]
main() {
  [Result<i32, FileError>] ok{Result.ok(2i32)}
  [Result<i32, FileError>] chained{
    Result.and_then(ok, []([i32] value) {
      [i32] adjusted{plus(value, 1i32)}
      if(greater_than(adjusted, 2i32),
        then(){ return(Result.ok(multiply(adjusted, 3i32))) },
        else(){ return(Result.ok(0i32)) })
    })
  }
  return(try(chained))
}
)";
  const std::string srcPath = writeTemp("vm_result_and_then_block_body_ir_backed.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_result_and_then_block_body_ir_backed_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 9);
  CHECK(readFile(outPath).empty());
}

TEST_CASE("vm supports map Result payloads on IR-backed paths") {
  const std::string source = R"(
import /std/file/*
import /std/collections/*

[return<Result<map<i32, i32>, FileError>>]
make_values() {
  return(Result.ok(map<i32, i32>(1i32, 7i32, 3i32, 9i32)))
}

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(err.why())
}

[return<int> effects(io_out, io_err) on_error<FileError, /log_file_error>]
main() {
  [map<i32, i32>] direct{try(make_values())}
  [map<i32, i32>] mapped{try(Result.map(make_values(), []([map<i32, i32>] values) {
    return(values)
  }))}
  [map<i32, i32>] chained{try(Result.and_then(make_values(), []([map<i32, i32>] values) {
    return(Result.ok(values))
  }))}
  [map<i32, i32>] summed{try(Result.map2(make_values(), make_values(), []([map<i32, i32>] left, [map<i32, i32>] right) {
    return(right)
  }))}
  print_line(mapCount<i32, i32>(direct))
  print_line(try(direct.tryAt(1i32)))
  print_line(try(mapped.tryAt(3i32)))
  print_line(try(chained.tryAt(1i32)))
  print_line(mapCount<i32, i32>(summed))
  print_line(try(summed.tryAt(3i32)))
  return(plus(try(direct.tryAt(1i32)),
              plus(try(mapped.tryAt(3i32)),
                   plus(try(chained.tryAt(1i32)), try(summed.tryAt(3i32))))))
}
)";
  const std::string srcPath = writeTemp("vm_result_map_payload_ir_backed.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_result_map_payload_ir_backed_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 32);
  CHECK(readFile(outPath) == "2\n7\n9\n7\n2\n9\n");
}

TEST_CASE("vm supports Buffer Result payloads on IR-backed paths") {
  const std::string source = R"(
import /std/gfx/*

[return<Result<Buffer<i32>, GfxError>> effects(gpu_dispatch)]
make_buffer() {
  [array<i32>] values{array<i32>(1i32, 2i32, 3i32)}
  return(Result.ok(/std/gfx/Buffer/upload(values)))
}

[effects(io_err)]
log_gfx_error([GfxError] err) {
  print_line_error(err.why())
}

[return<int> effects(gpu_dispatch, io_out, io_err) on_error<GfxError, /log_gfx_error>]
main() {
  [Buffer<i32>] direct{try(make_buffer())}
  [Buffer<i32>] mapped{try(Result.map(make_buffer(), []([Buffer<i32>] values) {
    return(values)
  }))}
  [Buffer<i32>] chained{try(Result.and_then(make_buffer(), []([Buffer<i32>] values) {
    return(Result.ok(values))
  }))}
  [Buffer<i32>] summed{try(Result.map2(make_buffer(), make_buffer(), []([Buffer<i32>] left, [Buffer<i32>] right) {
    return(right)
  }))}
  [array<i32>] directOut{direct.readback()}
  [array<i32>] mappedOut{mapped.readback()}
  [array<i32>] chainedOut{chained.readback()}
  [array<i32>] summedOut{summed.readback()}
  print_line(directOut.count())
  print_line(directOut[0i32])
  print_line(mappedOut[1i32])
  print_line(chainedOut[2i32])
  print_line(summedOut.count())
  print_line(summedOut[2i32])
  return(plus(directOut[0i32],
              plus(mappedOut[1i32],
                   plus(chainedOut[2i32], summedOut[2i32]))))
}
)";
  const std::string srcPath = writeTemp("vm_result_buffer_payload_ir_backed.prime", source);
  const std::string outPath =
      (std::filesystem::temp_directory_path() / "primec_vm_result_buffer_payload_ir_backed_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 11);
  CHECK(readFile(outPath) == "3\n1\n2\n3\n3\n3\n");
}

TEST_CASE("vm rejects auto-bound direct Result combinator try consumers") {
  const std::string source = R"(
import /std/file/*

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(err.why())
}

[return<int> effects(io_out, io_err) on_error<FileError, /log_file_error>]
main() {
  [auto] mapped{ Result.map(Result.ok(2i32), []([i32] value) { return(multiply(value, 4i32)) }) }
  [auto] chained{ Result.and_then(Result.ok(2i32), []([i32] value) { return(Result.ok(plus(value, 3i32))) }) }
  [auto] summed{
    Result.map2(Result.ok(2i32), Result.ok(3i32), []([i32] left, [i32] right) { return(plus(left, right)) })
  }
  print_line(try(mapped))
  print_line(try(chained))
  return(try(summed))
}
)";
  const std::string srcPath = writeTemp("vm_result_auto_bound_combinators.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_vm_result_auto_bound_combinators_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath).find("try requires Result argument") != std::string::npos);
}

TEST_CASE("vm uses stdlib File helper wrappers") {
  const std::string filePath =
      (testScratchPath("") / "primec_vm_stdlib_file_helpers.txt").string();
  auto escape = [](const std::string &text) {
    std::string out;
    out.reserve(text.size());
    for (char c : text) {
      if (c == '\\' || c == '"') {
        out.push_back('\\');
      }
      out.push_back(c);
    }
    return out;
  };
  const std::string source = R"(
import /std/file/*

[return<Result<FileError>> effects(file_write) on_error<FileError, /log_file_error>]
main() {
  [File<Write>] file{ File<Write>("__PATH__"utf8)? }
  [array<i32>] bytes{ array<i32>(68i32, 69i32) }
  /File/write<Write, i32>(file, 65i32)?
  /File/write_line<Write, i32>(file, 66i32)?
  file.write_byte(67i32)?
  /File/write_bytes(file, bytes)?
  /File/flush(file)?
  file.close()?
  return(Result.ok())
}

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(Result.why(FileError.status(err)))
}
)";
  std::string program = source;
  const std::string placeholder = "__PATH__";
  const size_t pathPos = program.find(placeholder);
  REQUIRE(pathPos != std::string::npos);
  program.replace(pathPos, placeholder.size(), escape(filePath));
  const std::string srcPath = writeTemp("vm_stdlib_file_helpers.prime", program);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(filePath) == "6566\nCDE");
}

TEST_CASE("vm uses stdlib File open helper wrappers") {
  const std::string filePath =
      (std::filesystem::temp_directory_path() / "primec_vm_stdlib_file_open_helpers.txt").string();
  const std::string stdoutPath =
      (std::filesystem::temp_directory_path() / "primec_vm_stdlib_file_open_helpers_out.txt").string();
  auto escape = [](const std::string &text) {
    std::string out;
    out.reserve(text.size());
    for (char c : text) {
      if (c == '\\' || c == '"') {
        out.push_back('\\');
      }
      out.push_back(c);
    }
    return out;
  };
  const std::string source = R"(
import /std/file/*

[return<Result<FileError>> effects(file_read, file_write, io_out) on_error<FileError, /log_file_error>]
main() {
  [File<Write>] writer{/File/open_write("__PATH__"utf8)?}
  writer.write("alpha"utf8)?
  writer.close()?
  [File<Append>] appender{/File/open_append("__PATH__"utf8)?}
  appender.write_line("omega"utf8)?
  appender.close()?
  [File<Read>] reader{/File/open_read("__PATH__"utf8)?}
  [i32 mut] first{0i32}
  reader.read_byte(first)?
  print_line(first)
  reader.close()?
  return(Result.ok())
}

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(Result.why(FileError.status(err)))
}
)";
  std::string program = source;
  const std::string placeholder = "__PATH__";
  size_t pathPos = program.find(placeholder);
  REQUIRE(pathPos != std::string::npos);
  program.replace(pathPos, placeholder.size(), escape(filePath));
  pathPos = program.find(placeholder);
  REQUIRE(pathPos != std::string::npos);
  program.replace(pathPos, placeholder.size(), escape(filePath));
  pathPos = program.find(placeholder);
  REQUIRE(pathPos != std::string::npos);
  program.replace(pathPos, placeholder.size(), escape(filePath));
  const std::string srcPath = writeTemp("vm_stdlib_file_open_helpers.prime", program);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + stdoutPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(filePath) == "alphaomega\n");
  CHECK(readFile(stdoutPath) == "97\n");
}

TEST_CASE("vm stdlib File close helper disarms the original handle") {
  const std::string filePath =
      (testScratchPath("") / "primec_vm_stdlib_file_close_helper.txt").string();
  const auto escape = [](const std::string &text) {
    std::string out;
    out.reserve(text.size());
    for (char c : text) {
      if (c == '\\' || c == '"') {
        out.push_back('\\');
      }
      out.push_back(c);
    }
    return out;
  };
  const std::string source = R"(
import /std/file/*

[effects(file_write), return<Result<FileError>> on_error<FileError, /log_file_error>]
main() {
  [File<Write>] file{ File<Write>("__PATH__"utf8)? }
  /File/write<Write, string>(file, "alpha"utf8)?
  file.close()?
  /File/write<Write, string>(file, "beta"utf8)
  return(Result.ok())
}

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(Result.why(FileError.status(err)))
}
)";
  std::string program = source;
  const std::string placeholder = "__PATH__";
  const size_t pathPos = program.find(placeholder);
  REQUIRE(pathPos != std::string::npos);
  program.replace(pathPos, placeholder.size(), escape(filePath));
  const std::string srcPath = writeTemp("vm_stdlib_file_close_helper.prime", program);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(filePath) == "alpha");
}

TEST_CASE("vm uses stdlib File string helper wrappers") {
  const std::string filePath =
      (testScratchPath("") / "primec_vm_stdlib_file_string_helpers.txt").string();
  const auto escape = [](const std::string &text) {
    std::string out;
    out.reserve(text.size());
    for (char c : text) {
      if (c == '\\' || c == '"') {
        out.push_back('\\');
      }
      out.push_back(c);
    }
    return out;
  };
  const std::string source = R"(
import /std/file/*

[return<Result<FileError>> effects(file_write) on_error<FileError, /log_file_error>]
main() {
  [File<Write>] file{ File<Write>("__PATH__"utf8)? }
  [string] text{"alpha"utf8}
  /File/write<Write, string>(file, text)?
  /File/write_line<Write, string>(file, "omega"utf8)?
  file.close()?
  return(Result.ok())
}

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(Result.why(FileError.status(err)))
}
)";
  std::string program = source;
  const std::string placeholder = "__PATH__";
  const size_t pathPos = program.find(placeholder);
  REQUIRE(pathPos != std::string::npos);
  program.replace(pathPos, placeholder.size(), escape(filePath));
  const std::string srcPath = writeTemp("vm_stdlib_file_string_helpers.prime", program);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(filePath) == "alphaomega\n");
}

TEST_CASE("vm uses stdlib File multi-value helper wrappers") {
  const std::string filePath =
      (testScratchPath("") / "primec_vm_stdlib_file_multi_helpers.txt").string();
  const auto escape = [](const std::string &text) {
    std::string out;
    out.reserve(text.size());
    for (char c : text) {
      if (c == '\\' || c == '"') {
        out.push_back('\\');
      }
      out.push_back(c);
    }
    return out;
  };
  const std::string source = R"(
import /std/file/*

[return<Result<FileError>> effects(file_write) on_error<FileError, /log_file_error>]
main() {
  [File<Write>] file{ File<Write>("__PATH__"utf8)? }
  /File/write<Write, string, i32, string>(file, "alpha"utf8, 7i32, "omega"utf8)?
  /File/write_line<Write, i32, string, i32, string, i32>(file, 255i32, " "utf8, 0i32, " "utf8, 0i32)?
  file.write("left"utf8, 1i32, "right"utf8)?
  file.write_line()?
  file.close()?
  return(Result.ok())
}

[effects(io_err)]
log_file_error([FileError] err) {
  print_line_error(Result.why(FileError.status(err)))
}
)";
  std::string program = source;
  const std::string placeholder = "__PATH__";
  const size_t pathPos = program.find(placeholder);
  REQUIRE(pathPos != std::string::npos);
  program.replace(pathPos, placeholder.size(), escape(filePath));
  const std::string srcPath = writeTemp("vm_stdlib_file_multi_helpers.prime", program);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(filePath) == "alpha7omega255 0 0\nleft1right\n");
}

TEST_CASE("vm resolves templated helper overload families by exact arity") {
  const std::string source = R"(
[struct]
Marker() {}

[return<i32>]
/helper/value<T>([T] value) {
  return(1i32)
}

[return<i32>]
/helper/value<A, B>([A] first, [B] second) {
  return(2i32)
}

[return<i32>]
/Marker/mark<T>([Marker] self, [T] value) {
  return(/helper/value(value))
}

[return<i32>]
/Marker/mark<A, B>([Marker] self, [A] first, [B] second) {
  return(/helper/value(first, second))
}

[effects(io_out), return<int>]
main() {
  [Marker] marker{Marker()}
  print_line(/helper/value(9i32))
  print_line(/helper/value(9i32, true))
  print_line(marker.mark(7i32))
  print_line(marker.mark(7i32, false))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_helper_overload_families.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_vm_helper_overload_families_out.txt").string();

  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "1\n2\n1\n2\n");
}

TEST_CASE("vm supports graphics-style int return propagation with on_error") {
  const std::string source = R"(
[struct]
GfxError() {
  [i32] code{0i32}
}

namespace GfxError {
  [return<string>]
  why([GfxError] err) {
    return(if(equal(err.code, 7i32), then() { "frame_acquire_failed"utf8 }, else() { "queue_submit_failed"utf8 }))
  }
}

[struct]
Frame() {
  [i32] token{0i32}
}

[return<Result<i32, GfxError>>]
acquire_frame_ok() {
  return(Result.ok(9i32))
}

[return<Result<i32, GfxError>>]
acquire_frame_fail() {
  return(7i64)
}

[return<Result<GfxError>>]
submit_frame([i32] token) {
  return(Result.ok())
}

[effects(io_err)]
log_gfx_error([GfxError] err) {
  print_line_error(err.why())
}

[return<int> on_error<GfxError, /log_gfx_error>]
main_ok() {
  [i32] frameToken{acquire_frame_ok()?}
  submit_frame(frameToken)?
  return(frameToken)
}

[return<int> effects(io_err) on_error<GfxError, /log_gfx_error>]
main_fail() {
  [i32] frameToken{acquire_frame_fail()?}
  return(frameToken)
}
)";
  const std::string srcPath = writeTemp("vm_graphics_int_on_error.prime", source);
  const std::string errPath =
      (testScratchPath("") / "primec_vm_graphics_int_on_error_err.txt").string();
  const std::string okCmd = "./primec --emit=vm " + srcPath + " --entry /main_ok";
  const std::string failCmd = "./primec --emit=vm " + srcPath + " --entry /main_fail 2> " + errPath;
  CHECK(runCommand(okCmd) == 9);
  CHECK(runCommand(failCmd) == 7);
  CHECK(readFile(errPath).empty());
}

TEST_CASE("vm supports string Result.ok payloads through try") {
  const std::string source = R"(
[struct]
ParseError() {
  [i32] code{0i32}
}

namespace ParseError {
  [return<string>]
  why([ParseError] err) {
    return("parse failed"utf8)
  }
}

[return<Result<string, ParseError>>]
greeting() {
  return(Result.ok("alpha"utf8))
}

[effects(io_err)]
log_parse_error([ParseError] err) {
  print_line_error(err.why())
}

[return<int> effects(io_out, io_err) on_error<ParseError, /log_parse_error>]
main() {
  [string] text{greeting()?}
  print_line(text)
  return(count(text))
}
)";
  const std::string srcPath = writeTemp("vm_result_ok_string.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_vm_result_ok_string_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 5);
  CHECK(readFile(outPath) == "alpha\n");
}

TEST_CASE("vm supports direct string Result combinator consumers") {
  const std::string source = R"(
[struct]
ParseError() {
  [i32] code{0i32}
}

namespace ParseError {
  [return<string>]
  why([ParseError] err) {
    return("parse failed"utf8)
  }
}

swallow_parse_error([ParseError] err) {}

[return<int> effects(io_out) on_error<ParseError, /swallow_parse_error>]
main() {
  print_line(try(Result.map(Result.ok("alpha"utf8), []([string] value) { return(value) })))
  print_line(try(Result.and_then(Result.ok("beta"utf8), []([string] value) { return(Result.ok(value)) })))
  print_line(try(Result.map2(Result.ok("gamma"utf8), Result.ok("delta"utf8), []([string] left, [string] right) {
    return(left)
  })))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_result_combinators_string.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_vm_result_combinators_string_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "alpha\nbeta\ngamma\n");
}

TEST_CASE("vm supports definition-backed string Result combinator sources") {
  const std::string source = R"(
[struct]
ParseError() {
  [i32] code{0i32}
}

namespace ParseError {
  [return<string>]
  why([ParseError] err) {
    return("parse failed"utf8)
  }
}

[struct]
Reader() {
  [i32] marker{0i32}
}

[return<Result<string, ParseError>>]
greeting() {
  return(Result.ok("alpha"utf8))
}

[return<Result<string, ParseError>>]
/Reader/read([Reader] self) {
  return(Result.ok("beta"utf8))
}

swallow_parse_error([ParseError] err) {}

[return<int> effects(io_out) on_error<ParseError, /swallow_parse_error>]
main() {
  [Reader] reader{Reader()}
  print_line(try(Result.map(greeting(), []([string] value) { return(value) })))
  print_line(try(Result.and_then(reader.read(), []([string] value) { return(Result.ok(value)) })))
  print_line(try(Result.map2(greeting(), reader.read(), []([string] left, [string] right) { return(left) })))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_result_combinators_string_definition_sources.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_vm_result_combinators_string_definition_sources_out.txt")
          .string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "alpha\nbeta\nalpha\n");
}

#if defined(EACCES)
TEST_CASE("vm maps FileError.why codes") {
  const std::string source =
      "[return<Result<FileError>>]\n"
      "make_error() {\n"
      "  return(" + std::to_string(EACCES) + "i32)\n"
      "}\n"
      "\n"
      "[return<int> effects(io_out)]\n"
      "main() {\n"
      "  print_line(Result.why(make_error()))\n"
      "  return(0i32)\n"
      "}\n";
  const std::string srcPath = writeTemp("vm_file_error_why.prime", source);
  const std::string outPath = (testScratchPath("") / "primec_vm_file_error_why_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "EACCES\n");
}
#endif

TEST_CASE("vm uses stdlib ImageError result helpers") {
  const std::string source = R"(
import /std/image/*

[return<int> effects(io_out)]
main() {
  print_line(Result.why(/ImageError/status(imageReadUnsupported())))
  print_line(Result.why(/ImageError/result<i32>(imageWriteUnsupported())))
  print_line(Result.why(/ImageError/result<i32>(imageInvalidOperation())))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_image_error_result_helpers.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_vm_image_error_result_helpers.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) ==
        "image_read_unsupported\n"
        "image_write_unsupported\n"
        "image_invalid_operation\n");
}

TEST_CASE("vm uses stdlib ImageError why wrapper") {
  const std::string source = R"(
import /std/image/*

[return<int> effects(io_out)]
main() {
  [ImageError] err{imageReadUnsupported()}
  [Result<ImageError>] methodStatus{/ImageError/status(err)}
  [Result<i32, ImageError>] methodValueStatus{/ImageError/result<i32>(err)}
  print_line(/ImageError/why(err))
  print_line(/ImageError/why(err))
  print_line(/ImageError/why(err))
  print_line(Result.why(imageErrorStatus(err)))
  print_line(Result.why(methodStatus))
  print_line(Result.why(methodValueStatus))
  print_line(Result.why(methodStatus))
  print_line(Result.why(methodValueStatus))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_image_error_why_wrapper.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_vm_image_error_why_wrapper.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) ==
        "image_read_unsupported\n"
        "image_read_unsupported\n"
        "image_read_unsupported\n"
        "image_read_unsupported\n"
        "image_read_unsupported\n"
        "image_read_unsupported\n"
        "image_read_unsupported\n"
        "image_read_unsupported\n");
}

TEST_CASE("vm uses stdlib ImageError constructor wrappers") {
  const std::string source = R"(
import /std/image/*

[return<int> effects(io_out)]
main() {
  print_line(Result.why(imageErrorStatus(/ImageError/read_unsupported())))
  print_line(Result.why(imageErrorStatus(/ImageError/write_unsupported())))
  print_line(Result.why(imageErrorStatus(/ImageError/invalid_operation())))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_image_error_constructor_wrappers.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_vm_image_error_constructor_wrappers.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) ==
        "image_read_unsupported\n"
        "image_write_unsupported\n"
        "image_invalid_operation\n");
}

TEST_CASE("vm uses stdlib GfxError result helpers") {
  const std::string source = R"(
import /std/gfx/experimental/*

[return<int> effects(io_out)]
main() {
  [GfxError] err{queueSubmitFailed()}
  print_line(Result.why(GfxError.status(queueSubmitFailed())))
  print_line(Result.why(GfxError.result<i32>(framePresentFailed())))
  print_line(Result.why(GfxError.status(err)))
  print_line(Result.why(GfxError.status(err)))
  print_line(Result.why(GfxError.result<i32>(err)))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_gfx_error_result_helpers.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_vm_gfx_error_result_helpers.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) ==
        "queue_submit_failed\n"
        "frame_present_failed\n"
        "queue_submit_failed\n"
        "queue_submit_failed\n"
        "queue_submit_failed\n");
}

TEST_CASE("vm uses canonical stdlib GfxError result helpers") {
  const std::string source = R"(
import /std/gfx/*

[return<int> effects(io_out)]
main() {
  [GfxError] err{queueSubmitFailed()}
  print_line(Result.why(GfxError.status(queueSubmitFailed())))
  print_line(Result.why(GfxError.result<i32>(framePresentFailed())))
  print_line(Result.why(GfxError.status(err)))
  print_line(Result.why(GfxError.result<i32>(err)))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_canonical_gfx_error_result_helpers.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_vm_canonical_gfx_error_result_helpers.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) ==
        "queue_submit_failed\n"
        "frame_present_failed\n"
        "queue_submit_failed\n"
        "queue_submit_failed\n");
}

TEST_CASE("vm uses canonical stdlib GfxError why helpers") {
  const std::string source = R"(
import /std/gfx/*

[return<int> effects(io_out)]
main() {
  [GfxError] err{queueSubmitFailed()}
  [Result<GfxError>] methodStatus{GfxError.status(err)}
  [Result<i32, GfxError>] methodValueStatus{GfxError.result<i32>(err)}
  print_line(GfxError.why(err))
  print_line(GfxError.why(err))
  print_line(err.why())
  print_line(Result.why(GfxError.status(err)))
  print_line(Result.why(methodStatus))
  print_line(Result.why(methodValueStatus))
  print_line(Result.why(methodStatus))
  print_line(Result.why(methodValueStatus))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_canonical_gfx_error_why_wrapper.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_vm_canonical_gfx_error_why_wrapper.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) ==
        "queue_submit_failed\n"
        "queue_submit_failed\n"
        "queue_submit_failed\n"
        "queue_submit_failed\n"
        "queue_submit_failed\n"
        "queue_submit_failed\n"
        "queue_submit_failed\n"
        "queue_submit_failed\n");
}

TEST_CASE("vm uses canonical stdlib GfxError constructors") {
  const std::string source = R"(
import /std/gfx/*

[return<int> effects(io_out)]
main() {
  print_line(Result.why(GfxError.status(GfxError.window_create_failed())))
  print_line(Result.why(GfxError.status(GfxError.device_create_failed())))
  print_line(Result.why(GfxError.status(GfxError.frame_present_failed())))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_canonical_gfx_error_constructor_wrappers.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_vm_canonical_gfx_error_constructor_wrappers.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) ==
        "window_create_failed\n"
        "device_create_failed\n"
        "frame_present_failed\n");
}

TEST_CASE("vm uses stdlib experimental Buffer helper methods") {
  const std::string source = R"(
import /std/gfx/experimental/*

[return<int>]
main() {
  [Buffer<i32>] emptyBuffer{Buffer<i32>([token] 0i32, [elementCount] 0i32)}
  [Buffer<i32>] fullBuffer{Buffer<i32>([token] 7i32, [elementCount] 4i32)}
  if(not(emptyBuffer.empty())) {
    return(90i32)
  }
  if(emptyBuffer.is_valid()) {
    return(91i32)
  }
  if(fullBuffer.empty()) {
    return(92i32)
  }
  if(not(fullBuffer.is_valid())) {
    return(93i32)
  }
  return(plus(fullBuffer.count(), /std/gfx/experimental/Buffer/count(emptyBuffer)))
}
)";
  const std::string srcPath = writeTemp("vm_experimental_gfx_buffer_helpers.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 4);
}

TEST_CASE("vm uses canonical stdlib Buffer helper methods") {
  const std::string source = R"(
import /std/gfx/*

[return<int>]
main() {
  [Buffer<i32>] emptyBuffer{Buffer<i32>([token] 0i32, [elementCount] 0i32)}
  [Buffer<i32>] fullBuffer{Buffer<i32>([token] 7i32, [elementCount] 5i32)}
  if(not(emptyBuffer.empty())) {
    return(90i32)
  }
  if(emptyBuffer.is_valid()) {
    return(91i32)
  }
  if(fullBuffer.empty()) {
    return(92i32)
  }
  if(not(fullBuffer.is_valid())) {
    return(93i32)
  }
  return(plus(fullBuffer.count(), /std/gfx/Buffer/count(emptyBuffer)))
}
)";
  const std::string srcPath = writeTemp("vm_canonical_gfx_buffer_helpers.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 5);
}

TEST_CASE("vm uses stdlib experimental Buffer readback helpers") {
  const std::string source = R"(
import /std/gfx/experimental/*

[effects(gpu_dispatch), return<int>]
main() {
  [Buffer<i32>] data{/std/gpu/buffer<i32>(3i32)}
  [array<i32>] viaMethod{data.readback()}
  [array<i32>] viaDirect{/std/gfx/experimental/Buffer/readback(data)}
  return(plus(plus(viaMethod.count(), viaDirect.count()), plus(viaMethod[0i32], viaDirect[2i32])))
}
)";
  const std::string srcPath = writeTemp("vm_experimental_gfx_buffer_readback_helpers.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 6);
}

TEST_CASE("vm uses canonical stdlib Buffer readback helpers") {
  const std::string source = R"(
import /std/gfx/*

[effects(gpu_dispatch), return<int>]
main() {
  [Buffer<i32>] data{/std/gpu/buffer<i32>(3i32)}
  [array<i32>] viaMethod{data.readback()}
  [array<i32>] viaDirect{/std/gfx/Buffer/readback(data)}
  return(plus(plus(viaMethod.count(), viaDirect.count()), plus(viaMethod[0i32], viaDirect[2i32])))
}
)";
  const std::string srcPath = writeTemp("vm_canonical_gfx_buffer_readback_helpers.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 6);
}

TEST_CASE("vm uses stdlib experimental Buffer allocation helpers") {
  const std::string source = R"(
import /std/gfx/experimental/*

[effects(gpu_dispatch), return<int>]
main() {
  [Buffer<i32>] data{/std/gfx/experimental/Buffer/allocate<i32>(3i32)}
  [array<i32>] out{data.readback()}
  return(plus(/std/gfx/experimental/Buffer/count(data), out.count()))
}
)";
  const std::string srcPath = writeTemp("vm_experimental_gfx_buffer_allocate_helpers.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 6);
}

TEST_CASE("vm uses canonical stdlib Buffer allocation helpers") {
  const std::string source = R"(
import /std/gfx/*

[effects(gpu_dispatch), return<int>]
main() {
  [Buffer<i32>] data{/std/gfx/Buffer/allocate<i32>(3i32)}
  [array<i32>] out{data.readback()}
  return(plus(/std/gfx/Buffer/count(data), out.count()))
}
)";
  const std::string srcPath = writeTemp("vm_canonical_gfx_buffer_allocate_helpers.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 6);
}

TEST_CASE("vm uses stdlib experimental Buffer constructor entry point") {
  const std::string source = R"(
import /std/gfx/experimental/*

[effects(gpu_dispatch), return<int>]
main() {
  [Buffer<i32>] data{Buffer<i32>(3i32)}
  [array<i32>] out{data.readback()}
  return(plus(/std/gfx/experimental/Buffer/count(data), out.count()))
}
)";
  const std::string srcPath = writeTemp("vm_experimental_gfx_buffer_constructor.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 6);
}

TEST_CASE("vm uses canonical stdlib Buffer constructor entry point") {
  const std::string source = R"(
import /std/gfx/*

[effects(gpu_dispatch), return<int>]
main() {
  [Buffer<i32>] data{Buffer<i32>(3i32)}
  [array<i32>] out{data.readback()}
  return(plus(/std/gfx/Buffer/count(data), out.count()))
}
)";
  const std::string srcPath = writeTemp("vm_canonical_gfx_buffer_constructor.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 6);
}

TEST_CASE("vm uses stdlib experimental Buffer upload helpers") {
  const std::string source = R"(
import /std/gfx/experimental/*

[effects(gpu_dispatch), return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32, 3i32)}
  [Buffer<i32>] data{/std/gfx/experimental/Buffer/upload(values)}
  [array<i32>] out{data.readback()}
  return(plus(plus(out.count(), out[0i32]), out[2i32]))
}
)";
  const std::string srcPath = writeTemp("vm_experimental_gfx_buffer_upload_helpers.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 7);
}

TEST_CASE("vm uses canonical stdlib Buffer upload helpers") {
  const std::string source = R"(
import /std/gfx/*

[effects(gpu_dispatch), return<int>]
main() {
  [array<i32>] values{array<i32>(1i32, 2i32, 3i32)}
  [Buffer<i32>] data{/std/gfx/Buffer/upload(values)}
  [array<i32>] out{data.readback()}
  return(plus(plus(out.count(), out[0i32]), out[2i32]))
}
)";
  const std::string srcPath = writeTemp("vm_canonical_gfx_buffer_upload_helpers.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 7);
}

TEST_CASE("vm uses stdlib FileError why wrapper") {
  const std::string source = R"(
import /std/file/*

[return<int> effects(io_out)]
main() {
  [FileError] err{fileReadEof()}
  if(not(fileErrorIsEof(err))) {
    return(1i32)
  }
  print_line(FileError.why(err))
  print_line(FileError.why(err))
  print_line(err.why())
  print_line(Result.why(FileError.status(err)))
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_file_error_why.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_vm_stdlib_file_error_why_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "EOF\nEOF\nEOF\nEOF\n");
}

TEST_CASE("vm uses stdlib FileError eof wrapper") {
  const std::string source = R"(
import /std/file/*

[return<int>]
main() {
  [FileError] eofErr{fileReadEof()}
  [FileError] otherErr{1i32}
  if(not(FileError.is_eof(eofErr))) {
    return(1i32)
  }
  if(not(FileError.is_eof(eofErr))) {
    return(2i32)
  }
  if(not(eofErr.is_eof())) {
    return(3i32)
  }
  if(FileError.is_eof(otherErr)) {
    return(4i32)
  }
  if(FileError.is_eof(otherErr)) {
    return(5i32)
  }
  if(otherErr.is_eof()) {
    return(6i32)
  }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_file_error_is_eof.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 0);
}

TEST_CASE("vm uses stdlib FileError eof constructor wrapper") {
  const std::string source = R"(
import /std/file/*

[return<int> effects(io_out)]
main() {
  [FileError] err{FileError.eof()}
  print_line(FileError.why(err))
  print_line(FileError.why(err))
  if(not(FileError.is_eof(err))) {
    return(1i32)
  }
  if(not(err.is_eof())) {
    return(2i32)
  }
  return(0i32)
}
)";
  const std::string srcPath = writeTemp("vm_stdlib_file_error_eof_wrapper.prime", source);
  const std::string outPath =
      (testScratchPath("") / "primec_vm_stdlib_file_error_eof_wrapper_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "EOF\nEOF\n");
}

TEST_CASE("vm rejects recursive calls") {
  const std::string source = R"(
[return<int>]
main() {
  return(main())
}
)";
  const std::string srcPath = writeTemp("vm_recursive_call.prime", source);
  const std::string errPath = (testScratchPath("") / "primec_vm_recursive_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 2);
  CHECK(readFile(errPath) == "VM lowering error: vm backend does not support recursive calls: /main\n");
}

TEST_CASE("vm accepts string pointers") {
  const std::string source = R"(
[return<void>]
main() {
  [string] name{"hi"utf8}
  [Pointer<string>] ptr{location(name)}
}
)";
  const std::string srcPath = writeTemp("vm_string_pointer.prime", source);
  const std::string errPath = (testScratchPath("") / "primec_vm_string_pointer_err.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main 2> " + errPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(errPath).empty());
}

TEST_CASE("vm ignores top-level executions") {
  const std::string source = R"(
[return<bool>]
unused() {
  return(equal("a"utf8, "b"utf8))
}

[return<int>]
main() {
  return(0i32)
}

unused()
)";
  const std::string srcPath = writeTemp("vm_exec_ignored.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 0);
}

TEST_CASE("vm materializes variadic experimental map packs with indexed canonical count calls") {
  const std::string source = R"(
import /std/collections/*
import /std/collections/experimental_map/*

[return<int> effects(heap_alloc)]
score_maps([args<Map<i32, i32>>] values) {
  [Map<i32, i32>] head{at(values, 0i32)}
  [Map<i32, i32>] tail{at(values, 2i32)}
  return(plus(/std/collections/experimental_map/mapCount<i32, i32>(head),
              /std/collections/experimental_map/mapCount<i32, i32>(tail)))
}

[return<int> effects(heap_alloc)]
forward([args<Map<i32, i32>>] values) {
  return(score_maps([spread] values))
}

[return<int> effects(heap_alloc)]
forward_mixed([args<Map<i32, i32>>] values) {
  return(score_maps(mapSingle(1i32, 2i32), [spread] values))
}

[return<int> effects(heap_alloc)]
main() {
  return(plus(score_maps(mapPair(1i32, 2i32, 3i32, 4i32),
                         mapSingle(5i32, 6i32),
                         mapTriple(7i32, 8i32, 9i32, 10i32, 11i32, 12i32)),
              plus(forward(mapSingle(13i32, 14i32),
                           mapPair(15i32, 16i32, 17i32, 18i32),
                           mapSingle(19i32, 20i32)),
                   forward_mixed(mapPair(21i32, 22i32, 23i32, 24i32),
                                 mapTriple(25i32, 26i32, 27i32, 28i32, 29i32, 30i32)))))
}
)";
  const std::string srcPath = writeTemp("vm_variadic_args_experimental_map_count.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 11);
}

TEST_CASE("vm materializes variadic pointer uninitialized scalar packs with indexed init and take") {
  const std::string source = R"(
[return<int>]
score_ptrs([args<Pointer<uninitialized<i32>>>] values) {
  init(dereference(values[0i32]), 2i32)
  init(dereference(values.at(1i32)), 3i32)
  init(dereference(values.at_unsafe(2i32)), 4i32)
  return(plus(take(dereference(values[0i32])),
              plus(take(dereference(values.at(1i32))),
                   take(dereference(values.at_unsafe(2i32))))))
}

[return<int>]
forward([args<Pointer<uninitialized<i32>>>] values) {
  return(score_ptrs([spread] values))
}

[return<int>]
forward_mixed([args<Pointer<uninitialized<i32>>>] values) {
  [uninitialized<i32>] extra{uninitialized<i32>()}
  return(score_ptrs(location(extra), [spread] values))
}

[return<int>]
main() {
  [uninitialized<i32>] a0{uninitialized<i32>()}
  [uninitialized<i32>] a1{uninitialized<i32>()}
  [uninitialized<i32>] a2{uninitialized<i32>()}

  [uninitialized<i32>] b0{uninitialized<i32>()}
  [uninitialized<i32>] b1{uninitialized<i32>()}
  [uninitialized<i32>] b2{uninitialized<i32>()}

  [uninitialized<i32>] c0{uninitialized<i32>()}
  [uninitialized<i32>] c1{uninitialized<i32>()}

  return(plus(score_ptrs(location(a0), location(a1), location(a2)),
              plus(forward(location(b0), location(b1), location(b2)),
                   forward_mixed(location(c0), location(c1)))))
}
)";
  const std::string srcPath = writeTemp("vm_variadic_args_pointer_uninitialized_scalar.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 27);
}

TEST_CASE("vm materializes variadic borrowed uninitialized scalar packs with indexed init and take") {
  const std::string source = R"(
[return<int>]
score_refs([args<Reference<uninitialized<i32>>>] values) {
  init(dereference(values[0i32]), 2i32)
  init(dereference(values.at(1i32)), 3i32)
  init(dereference(values.at_unsafe(2i32)), 4i32)
  return(plus(take(dereference(values[0i32])),
              plus(take(dereference(values.at(1i32))),
                   take(dereference(values.at_unsafe(2i32))))))
}

[return<int>]
forward([args<Reference<uninitialized<i32>>>] values) {
  return(score_refs([spread] values))
}

[return<int>]
forward_mixed([args<Reference<uninitialized<i32>>>] values) {
  [uninitialized<i32>] extra{uninitialized<i32>()}
  return(score_refs(location(extra), [spread] values))
}

[return<int>]
main() {
  [uninitialized<i32>] a0{uninitialized<i32>()}
  [uninitialized<i32>] a1{uninitialized<i32>()}
  [uninitialized<i32>] a2{uninitialized<i32>()}

  [uninitialized<i32>] b0{uninitialized<i32>()}
  [uninitialized<i32>] b1{uninitialized<i32>()}
  [uninitialized<i32>] b2{uninitialized<i32>()}

  [uninitialized<i32>] c0{uninitialized<i32>()}
  [uninitialized<i32>] c1{uninitialized<i32>()}

  return(plus(score_refs(location(a0), location(a1), location(a2)),
              plus(forward(location(b0), location(b1), location(b2)),
                   forward_mixed(location(c0), location(c1)))))
}
)";
  const std::string srcPath = writeTemp("vm_variadic_args_reference_uninitialized_scalar.prime", source);
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main";
  CHECK(runCommand(runCmd) == 27);
}

TEST_SUITE_END();
