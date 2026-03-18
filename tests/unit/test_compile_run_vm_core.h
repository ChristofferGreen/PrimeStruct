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
      plus(count(arr), plus(count(vec), plus(count(table), count(text))))
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
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_vm_raw_string_out.txt").string();
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
      (std::filesystem::temp_directory_path() / "primec_vm_raw_string_single_out.txt").string();
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
  [i32] len{/std/collections/vector/count(words)}
  for([i32 mut] index{0i32}, less_than(index, len), assign(index, plus(index, 1i32))) {
    if(greater_than(index, 0i32)) {
      print(","utf8)
    }
    print(words[index])
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
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_vm_ui_command_serialization.txt").string();
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
  [i32] len{/std/collections/vector/count(words)}
  for([i32 mut] index{0i32}, less_than(index, len), assign(index, plus(index, 1i32))) {
    if(greater_than(index, 0i32)) {
      print(","utf8)
    }
    print(words[index])
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
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_vm_ui_clip_command_serialization.txt").string();
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
  [i32] len{/std/collections/vector/count(words)}
  for([i32 mut] index{0i32}, less_than(index, len), assign(index, plus(index, 1i32))) {
    if(greater_than(index, 0i32)) {
      print(","utf8)
    }
    print(words[index])
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
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_vm_ui_clip_underflow_serialization.txt").string();
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
  [i32] len{/std/collections/vector/count(words)}
  for([i32 mut] index{0i32}, less_than(index, len), assign(index, plus(index, 1i32))) {
    if(greater_than(index, 0i32)) {
      print(","utf8)
    }
    print(words[index])
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
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_vm_ui_empty_text_serialization.txt").string();
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
  [i32] len{/std/collections/vector/count(words)}
  for([i32 mut] index{0i32}, less_than(index, len), assign(index, plus(index, 1i32))) {
    if(greater_than(index, 0i32)) {
      print(","utf8)
    }
    print(words[index])
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
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_vm_ui_layout_tree_serialization.txt").string();
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
  [i32] len{/std/collections/vector/count(words)}
  for([i32 mut] index{0i32}, less_than(index, len), assign(index, plus(index, 1i32))) {
    if(greater_than(index, 0i32)) {
      print(","utf8)
    }
    print(words[index])
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
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_vm_ui_layout_tree_empty_root.txt").string();
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
  [i32] len{/std/collections/vector/count(words)}
  for([i32 mut] index{0i32}, less_than(index, len), assign(index, plus(index, 1i32))) {
    if(greater_than(index, 0i32)) {
      print(","utf8)
    }
    print(words[index])
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
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_vm_ui_basic_widget_controls.txt").string();
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
  [i32] len{/std/collections/vector/count(words)}
  for([i32 mut] index{0i32}, less_than(index, len), assign(index, plus(index, 1i32))) {
    if(greater_than(index, 0i32)) {
      print(","utf8)
    }
    print(words[index])
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
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_vm_ui_panel_widget.txt").string();
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
  [i32] len{/std/collections/vector/count(words)}
  for([i32 mut] index{0i32}, less_than(index, len), assign(index, plus(index, 1i32))) {
    if(greater_than(index, 0i32)) {
      print(","utf8)
    }
    print(words[index])
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
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_vm_ui_empty_panel_widget.txt").string();
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
  [i32] len{/std/collections/vector/count(words)}
  for([i32 mut] index{0i32}, less_than(index, len), assign(index, plus(index, 1i32))) {
    if(greater_than(index, 0i32)) {
      print(","utf8)
    }
    print(words[index])
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
  [i32] len{/std/collections/vector/count(words)}
  for([i32 mut] index{0i32}, less_than(index, len), assign(index, plus(index, 1i32))) {
    if(greater_than(index, 0i32)) {
      print(","utf8)
    }
    print(words[index])
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
  [i32] len{/std/collections/vector/count(words)}
  for([i32 mut] index{0i32}, less_than(index, len), assign(index, plus(index, 1i32))) {
    if(greater_than(index, 0i32)) {
      print(","utf8)
    }
    print(words[index])
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
  [i32] len{/std/collections/vector/count(words)}
  for([i32 mut] index{0i32}, less_than(index, len), assign(index, plus(index, 1i32))) {
    if(greater_than(index, 0i32)) {
      print(","utf8)
    }
    print(words[index])
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
  [i32] len{/std/collections/vector/count(words)}
  for([i32 mut] index{0i32}, less_than(index, len), assign(index, plus(index, 1i32))) {
    if(greater_than(index, 0i32)) {
      print(","utf8)
    }
    print(words[index])
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
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_vm_heap_at_out_of_bounds.txt").string();
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
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_vm_heap_free_invalid_deref.txt").string();
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
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_vm_capabilities_out.txt").string();
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
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_vm_print_no_effects_err.txt").string();
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
      (std::filesystem::temp_directory_path() / "primec_vm_default_effects_no_io_err_err.txt").string();
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
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_vm_implicit_utf8_out.txt").string();
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
      (std::filesystem::temp_directory_path() / "primec_vm_implicit_utf8_single_out.txt").string();
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
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_vm_utf8_escaped_out.txt").string();
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
      (std::filesystem::temp_directory_path() / "primec_vm_utf8_escaped_single_out.txt").string();
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
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_vm_string_return_out.txt").string();
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
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_vm_result_why_custom_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "custom error\n");
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
      (std::filesystem::temp_directory_path() / "primec_vm_graphics_int_on_error_err.txt").string();
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
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_vm_result_ok_string_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 5);
  CHECK(readFile(outPath) == "alpha\n");
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
  const std::string outPath = (std::filesystem::temp_directory_path() / "primec_vm_file_error_why_out.txt").string();
  const std::string runCmd = "./primec --emit=vm " + srcPath + " --entry /main > " + outPath;
  CHECK(runCommand(runCmd) == 0);
  CHECK(readFile(outPath) == "EACCES\n");
}
#endif

TEST_CASE("vm rejects recursive calls") {
  const std::string source = R"(
[return<int>]
main() {
  return(main())
}
)";
  const std::string srcPath = writeTemp("vm_recursive_call.prime", source);
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_vm_recursive_err.txt").string();
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
  const std::string errPath = (std::filesystem::temp_directory_path() / "primec_vm_string_pointer_err.txt").string();
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

TEST_SUITE_END();
