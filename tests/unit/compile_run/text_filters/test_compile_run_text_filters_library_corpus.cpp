// The differential standard for `primec --library` (docs/LibrarySymbolManifestLazyImports.md, docs/spec/integration-and-
// tooling.md, Compiling against a library): every program of the corpus is built as one whole program and as a library
// plus a consumer, and the two must give the same output and the same exit code, on the VM and on optexe.
#include "test_compile_run_text_filters_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.text_filters");

namespace {

// `lib` is the module (`ui.prime`: namespaces `ui`, sometimes `util`); `app` is a consumer that starts with the import
// line `@IMPORT@`, which is `import<"ui.prime">` for the whole build and `import</ui>` for the library build.
struct CorpusProgram {
  const char *name;
  const char *lib;
  const char *app;
};

const CorpusProgram Corpus[] = {
    {"scalar_add",
     R"([namespace] ui {
  [public return<i32>] add([i32] a, [i32] b) { return(a + b) }
})",
     R"(@IMPORT@
import /ui/*
[return<i32>] main() { return(add(40i32, 2i32)) })"},
    {"private_helpers",
     R"([namespace] ui {
  [return<i32>] twice([i32] a) { return(add(a, a)) }
  [return<i32>] add([i32] a, [i32] b) { return(a + b) }
  [public return<i32>] quad([i32] a) { return(twice(a) + twice(a)) }
})",
     R"(@IMPORT@
import /ui/*
[return<i32>] main() { return(quad(5i32)) })"},
    {"struct_method",
     R"([namespace] ui {
  [public struct] Point() {
    [public i32] x{1i32}
    [public i32] y{2i32}
    [public return<i32>] sum() { return(this.x + this.y) }
  }
})",
     R"(@IMPORT@
import /ui/*
[return<i32>] main() {
  [Point] p{Point{20i32, 22i32}}
  return(p.sum())
})"},
    {"mut_method",
     R"([namespace] ui {
  [public struct] Counter() {
    [public i32] n{0i32}
    [public mut] bump([i32] by) { this.n = this.n + by }
    [public return<i32>] value() { return(this.n) }
  }
})",
     R"(@IMPORT@
import /ui/*
[return<i32>] main() {
  [Counter mut] c{Counter{}}
  c.bump(3i32)
  c.bump(4i32)
  return(c.value())
})"},
    {"struct_param_and_result",
     R"([namespace] ui {
  [public struct] Point() {
    [public i32] x{0i32}
    [public i32] y{0i32}
  }
  [public return<Point>] moved([Point] p, [i32] dx) { return(Point{p.x + dx, p.y}) }
  [public return<i32>] dot([Point] a, [Point] b) { return(a.x * b.x + a.y * b.y) }
})",
     R"(@IMPORT@
import /ui/*
[return<i32>] main() {
  [Point] p{moved(Point{1i32, 2i32}, 2i32)}
  return(dot(p, Point{4i32, 5i32}))
})"},
    {"default_parameter",
     R"([namespace] ui {
  [public return<i32>] scaled([i32] a, [i32] k{3i32}) { return(a * k) }
})",
     R"(@IMPORT@
import /ui/*
[return<i32>] main() { return(scaled(7i32) + scaled(1i32, 2i32)) })"},
    {"effect_print",
     R"([namespace] ui {
  [public effects(io_out) return<i32>] announce([i32] n) {
    print_line(n)
    return(n + 1i32)
  }
})",
     R"(@IMPORT@
import /ui/*
[return<i32> effects(io_out)] main() {
  print_line(announce(1i32))
  return(announce(5i32))
})"},
    {"string_literal",
     R"([namespace] ui {
  [public effects(io_out) return<void>] greet() { print_line("hello from the library"utf8) }
})",
     R"(@IMPORT@
import /ui/*
[return<i32> effects(io_out)] main() {
  print_line("before"utf8)
  greet()
  greet()
  print_line("after"utf8)
  return(0i32)
})"},
    {"string_two_functions",
     R"([namespace] ui {
  [public effects(io_out) return<void>] first() { print_line("one"utf8) }
  [public effects(io_out) return<void>] second() { print_line("two"utf8) }
})",
     R"(@IMPORT@
import /ui/*
[return<i32> effects(io_out)] main() {
  second()
  print_line("main"utf8)
  first()
  return(2i32)
})"},
    {"task",
     R"([namespace] ui {
  [public return<i32>] work([i32] n) { return(n * n) }
})",
     R"(@IMPORT@
import /ui/*
[return<i32> effects(task)] main() {
  [Task<i32>] a{[spawn] work(6i32)}
  [Task<i32>] b{[spawn] work(7i32)}
  return(wait(a) + wait(b))
})"},
    {"recursion",
     R"([namespace] ui {
  [public return<i32>] fact([i32] n) {
    if(n <= 1i32) {
      return(1i32)
    }
    return(n * fact(n - 1i32))
  }
})",
     R"(@IMPORT@
import /ui/*
[return<i32>] main() { return(fact(5i32)) })"},
    {"loop",
     R"([namespace] ui {
  [public return<i32>] sum_to([i32] n) {
    [i32 mut] total{0i32}
    [i32 mut] i{1i32}
    while(i <= n) {
      total = total + i
      i = i + 1i32
    }
    return(total)
  }
})",
     R"(@IMPORT@
import /ui/*
[return<i32>] main() { return(sum_to(10i32)) })"},
    {"nested_namespace",
     R"([namespace] ui {
  [namespace] inner {
    [public return<i32>] deep() { return(9i32) }
  }
  [public return<i32>] shallow() { return(/ui/inner/deep() + 1i32) }
})",
     R"(@IMPORT@
import /ui/*
[return<i32>] main() { return(shallow() + /ui/inner/deep()) })"},
    {"wide_integers",
     R"([namespace] ui {
  [public return<i64>] big([i64] a) { return(a * 1000000i64) }
  [public return<u64>] ubig([u64] a) { return(a + 5u64) }
})",
     R"(@IMPORT@
import /ui/*
[return<i32> effects(io_out)] main() {
  print_line(big(3000i64))
  print_line(ubig(7u64))
  return(0i32)
})"},
    {"float_math",
     R"([namespace] ui {
  [public return<f64>] half([f64] a) { return(a / 2.0f64) }
})",
     R"(@IMPORT@
import /ui/*
[return<i32>] main() { return(convert<i32>(half(9.0f64))) })"},
    {"bool_branch",
     R"([namespace] ui {
  [public return<bool>] positive([i32] a) { return(a > 0i32) }
})",
     R"(@IMPORT@
import /ui/*
[return<i32>] main() {
  if(positive(3i32) && !positive(0i32 - 1i32)) {
    return(11i32)
  }
  return(12i32)
})"},
    {"nested_structs",
     R"([namespace] ui {
  [public struct] Point() {
    [public i32] x{0i32}
    [public i32] y{0i32}
  }
  [public struct] Line() {
    [public Point] a{Point{}}
    [public Point] b{Point{}}
    [public return<i32>] span() { return(this.b.x - this.a.x + this.b.y - this.a.y) }
  }
})",
     R"(@IMPORT@
import /ui/*
[return<i32>] main() {
  [Line] l{Line{Point{1i32, 2i32}, Point{11i32, 22i32}}}
  return(l.span())
})"},
    {"library_uses_stdlib",
     R"(import /std/collections/vector
[namespace] ui {
  [public effects(heap_alloc) return<i32>] count_items([i32] n) {
    [vector<i32> mut] items{vector<i32>{}}
    items.push(n)
    items.push(n + 1i32)
    items.push(n + 2i32)
    return(items.count())
  }
})",
     R"(@IMPORT@
import /ui/*
[return<i32> effects(heap_alloc)] main() { return(count_items(4i32)) })"},
    {"program_and_library_share_stdlib",
     R"(import /std/collections/vector
[namespace] ui {
  [public effects(heap_alloc) return<i32>] total([i32] n) {
    [vector<i32> mut] items{vector<i32>{}}
    items.push(n)
    items.push(n * 2i32)
    return(items.count() + items.at(1i32))
  }
})",
     R"(@IMPORT@
import /std/collections/vector
import /ui/*
[return<i32> effects(heap_alloc)] main() {
  [vector<i32> mut] mine{vector<i32>{}}
  mine.push(total(5i32))
  mine.push(1i32)
  return(mine.count() + mine.at(0i32))
})"},
    {"two_roots",
     R"([namespace] ui {
  [public return<i32>] one() { return(1i32) }
}
[namespace] util {
  [public return<i32>] ten() { return(10i32) }
})",
     R"(@IMPORT@
import /ui/*
import /util/*
[return<i32>] main() { return(one() + ten()) })"},
    {"nonzero_exit",
     R"([namespace] ui {
  [public return<i32>] code() { return(3i32) }
})",
     R"(@IMPORT@
import /ui/*
[return<i32>] main() { return(code()) })"},
    {"consumer_struct_wraps_library_struct",
     R"([namespace] ui {
  [public struct] Point() {
    [public i32] x{1i32}
    [public mut] shift([i32] dx) { this.x = this.x + dx }
    [public return<i32>] get() { return(this.x) }
  }
})",
     R"(@IMPORT@
import /ui/*
[struct] Sprite() {
  [Point] where{Point{5i32}}
  [mut] nudge() { this.where.shift(2i32) }
}
[return<i32>] main() {
  [Sprite mut] s{Sprite{}}
  s.nudge()
  s.nudge()
  return(s.where.get())
})"},
    {"methods_in_a_loop",
     R"([namespace] ui {
  [public struct] Counter() {
    [public i32] n{0i32}
    [public mut] bump([i32] by) { this.n = this.n + by }
    [public return<i32>] value() { return(this.n) }
  }
  [public return<i32>] triple([i32] a) { return(a * 3i32) }
})",
     R"(@IMPORT@
import /ui/*
[return<i32>] main() {
  [Counter mut] c{Counter{}}
  [i32 mut] i{0i32}
  while(i < 4i32) {
    c.bump(triple(i))
    i = i + 1i32
  }
  return(c.value())
})"},
    {"generic_box",
     R"([namespace] ui {
  [public struct]
  Box<T>() {
    [public T] value{}
    [public return<T>] get() { return(this.value) }
  }
  [public return<i32>] inner() {
    [Box<i32>] b{Box<i32>{3i32}}
    return(b.get())
  }
})",
     R"(@IMPORT@
import /ui/*
[return<i32> effects(io_out)] main() {
  [Box<string>] s{Box<string>{"hello"utf8}}
  print_line(s.get())
  [Box<i32>] b{Box<i32>{4i32}}
  return(b.get() + inner())
})"},
    {"generic_private_reach",
     R"([namespace] ui {
  [return<i32>] helper([i32] a) { return(a + 1i32) }
  [struct] Hidden() {
    [i32] n{3i32}
    [return<i32>] value() { return(this.n) }
  }
  [return<T>]
  pass_through<T>([T] a) { return(a) }
  [public struct]
  Box<T>() {
    [public T] value{}
    [public return<T>] get() { return(pass_through<T>(this.value)) }
    [public return<i32>] bumped() {
      [Hidden] h{Hidden{}}
      return(helper(h.value()))
    }
  }
  [public return<i32>]
  count_up<T>([T] a, [i32] n) {
    [i32 mut] total{0i32}
    [i32 mut] i{0i32}
    while(i < n) {
      if(i > 1i32) {
        total = total + helper(i)
      } else {
        total = total + 1i32
      }
      i = i + 1i32
    }
    return(total)
  }
})",
     R"(@IMPORT@
import /ui/*
[return<i32>] main() {
  [Box<i64>] b{Box<i64>{7i64}}
  [Box<i32>] c{Box<i32>{1i32}}
  return(convert<i32>(b.get()) + count_up<f64>(1.0f64, 6i32) + c.bumped())
})"},
    {"generic_instantiated_in_both",
     R"([namespace] ui {
  [return<i32>] helper([i32] a) { return(a * 2i32) }
  [public struct]
  Box<T>() {
    [public T] value{}
    [public return<T>] get() { return(this.value) }
  }
  [public return<i32>]
  count_up<T>([T] a, [i32] n) {
    [i32 mut] total{0i32}
    [i32 mut] i{0i32}
    while(i < n) {
      total = total + helper(i)
      i = i + 1i32
    }
    return(total)
  }
  [public return<i32>] inner() {
    [Box<i32>] b{Box<i32>{3i32}}
    return(b.get() + count_up<i64>(1i64, 4i32))
  }
})",
     R"(@IMPORT@
import /ui/*
[return<i32>] main() {
  [Box<i32>] b{Box<i32>{5i32}}
  return(b.get() + inner() + count_up<i64>(2i64, 3i32) + count_up<i32>(2i32, 2i32))
})"},
    {"struct_with_destroy",
     R"([namespace] ui {
  [public struct] Noisy() {
    [public i32 mut] id{0i32}
    [public effects(io_out)] Destroy() { print_line(this.id) }
    [public effects(io_out) return<i32>] touch() { return(this.id + 1i32) }
  }
  [public effects(io_out) return<Noisy>] make([i32] id) { return(Noisy{id}) }
  [public effects(io_out) return<void>] scoped([i32] id) {
    [Noisy] n{Noisy{id}}
    print_line(n.touch())
  }
})",
     R"(@IMPORT@
import /ui/*
[return<i32> effects(io_out)] main() {
  scoped(1i32)
  [Noisy] a{make(5i32)}
  if(true) {
    [Noisy] b{Noisy{6i32}}
    print_line(b.touch())
  }
  print_line(99i32)
  return(a.touch())
})"},
    {"destroy_reaches_private_helper",
     R"([namespace] ui {
  [effects(io_out) return<void>] shout([i32] id) { print_line(id * 10i32) }
  [public struct] Guard() {
    [public i32] id{0i32}
    [public effects(io_out)] Create() { shout(1i32) }
    [public effects(io_out)] Destroy() { shout(this.id) }
  }
  [public return<i32>] id_of([Guard] g) { return(g.id) }
})",
     R"(@IMPORT@
import /ui/*
[return<i32> effects(io_out)] main() {
  [Guard] g{Guard{4i32}}
  return(id_of(g))
})"},
    {"explicit_self_methods",
     R"([namespace] ui {
  [public struct summary("A pair.")] Pair() {
    [public i32] a{1i32}
    [public i32] b{2i32}
  }
  [namespace] Pair {
    [public return<i32>] dot([Pair] self, [Pair] other) { return(self.a * other.a + self.b * other.b) }
    [public return<Pair>] add([Pair] self, [Pair] other) { return(Pair{self.a + other.a, self.b + other.b}) }
  }
})",
     R"(@IMPORT@
import /ui/*
[return<i32>] main() {
  [Pair] p{Pair{3i32, 4i32}}
  [Pair] q{Pair{1i32, 2i32}}
  return(p.dot(q) + p.add(q).dot(q))
})"},
    // A public sum is printed in the interface and a function returns one in a heap block the caller owns.
    {"sum_result",
     R"([namespace] ui {
  [public sum summary("Which kind of pick.")] Pick {
    none
    [i32] number
    [bool] flag
  }
  [public return<Pick>] make_pick([i32] value) { return(Pick{[number] value}) }
  [public return<Pick>] make_none() { return(Pick{none}) }
  [public return<Pick>] make_flag([bool] value) { return(Pick{[flag] value}) }
})",
     R"(@IMPORT@
import /ui/*
[return<i32> effects(io_out)] main() {
  [Pick] a{make_pick(5i32)}
  [Pick] b{make_none()}
  [Pick] c{make_flag(true)}
  print_line(pick(a) { none { 0i32 } number(n) { n } flag(f) { 100i32 } })
  print_line(pick(b) { none { 10i32 } number(n) { n } flag(f) { 100i32 } })
  return(pick(c) { none { 0i32 } number(n) { n } flag(f) { 7i32 } })
})"},
    {"sum_result_owned_payload",
     R"([namespace] ui {
  [public struct] Noisy() {
    [public i32 mut] id{0i32}
    [public effects(io_out)] Destroy() { print_line(this.id) }
  }
  [public sum] Slot {
    empty
    [Noisy] held
  }
  [public effects(io_out) return<Slot>] fill([i32] id) { return(Slot{[held] Noisy{id}}) }
  [public effects(io_out) return<Slot>] empty_slot() { return(Slot{empty}) }
})",
     R"(@IMPORT@
import /ui/*
[return<i32> effects(io_out)] main() {
  [Slot] a{fill(3i32)}
  [Slot] b{empty_slot()}
  [i32] total{pick(a) { empty { 0i32 } held(n) { n.id } }}
  print_line(total + pick(b) { empty { 40i32 } held(n) { n.id } })
  return(total)
})"},
    // A vector result is the function's own `mut` vector local, copied into a block of the caller that owns it.
    {"vector_result",
     R"(import /std/collections/vector

[namespace] ui {
  [public effects(heap_alloc) return<vector<i32>>] make_numbers([i32] count) {
    [vector<i32> mut] out{vector<i32>{}}
    [i32 mut] next{0i32}
    while(next < count) {
      out.push(next * 3i32)
      next = next + 1i32
    }
    return(out)
  }
  [public effects(heap_alloc) return<vector<i32>>] make_empty() {
    [vector<i32> mut] out{vector<i32>{}}
    return(out)
  }
})",
     R"(@IMPORT@
import /ui/*
import /std/collections/vector
[return<i32> effects(io_out, heap_alloc)] main() {
  [vector<i32> mut] a{make_numbers(5i32)}
  [vector<i32>] b{make_empty()}
  a.push(100i32)
  print_line(a.count())
  print_line(b.count())
  print_line(a[4i32] + a[5i32])
  return(a.count() + b.count())
})"},
};

constexpr size_t CorpusSize = sizeof(Corpus) / sizeof(Corpus[0]);

std::string replaced(std::string text, const std::string &from, const std::string &to) {
  const size_t at = text.find(from);
  REQUIRE(at != std::string::npos);
  return text.replace(at, from.size(), to);
}

struct Run {
  int exitCode = 0;
  std::string out;
  std::string err;
};

void writeIn(const std::filesystem::path &dir, const std::string &name, const std::string &text) {
  std::filesystem::create_directories((dir / name).parent_path());
  std::ofstream file(dir / name, std::ios::binary);
  REQUIRE(file.good());
  file << text;
}

Run shell(const std::filesystem::path &dir, const std::string &command) {
  Run run;
  run.exitCode = runCommand(command + " > " + quoteShellArg((dir / "o.txt").string()) + " 2> " +
                            quoteShellArg((dir / "e.txt").string()));
  run.out = readFile((dir / "o.txt").string());
  run.err = readFile((dir / "e.txt").string());
  return run;
}

// Compiles and runs `app.prime` (vm runs it; optexe writes an executable that is run).
Run runApp(const std::filesystem::path &dir, const std::string &emit, const std::string &extra) {
  const std::string app = quoteShellArg((dir / "app.prime").string());
  if (emit == "vm") {
    return shell(dir, "./primec --emit=vm " + extra + " " + app + " --entry /main");
  }
  const std::string exe = (dir / "app_exe").string();
  const Run built = shell(dir,
                          "./primec --emit=" + emit + " " + extra + " " + app + " -o " +
                              quoteShellArg(exe) + " --entry /main");
  if (built.exitCode != 0) {
    return built;
  }
  return shell(dir, quoteShellArg(exe));
}

// Builds one program whole and as library plus consumer, and compares the runs.
void checkProgram(const CorpusProgram &program, const std::string &emit) {
  CAPTURE(program.name);
  CAPTURE(emit);
  const std::filesystem::path dir =
      testScratchPath(std::string("library_corpus_") + program.name + "_" + emit);
  writeIn(dir, "ui.prime", program.lib);
  writeIn(dir, "app.prime", replaced(program.app, "@IMPORT@", "import<\"ui.prime\">"));
  const Run whole = runApp(dir, emit, "");
  REQUIRE_MESSAGE(whole.err.empty(), whole.err);

  const std::string archive = (dir / "ui.psa").string();
  const Run built = shell(dir,
                          "./primec --emit=lib " + quoteShellArg((dir / "ui.prime").string()) +
                              " -o " + quoteShellArg(archive));
  REQUIRE_MESSAGE(built.exitCode == 0, built.err);
  writeIn(dir, "app.prime", replaced(program.app, "@IMPORT@", "import</ui>"));
  const Run linked = runApp(dir, emit, "--library " + quoteShellArg(archive));
  CHECK_MESSAGE(linked.err.empty(), linked.err);
  CHECK(linked.exitCode == whole.exitCode);
  CHECK(linked.out == whole.out);
}

void checkRange(size_t first, size_t last, const std::string &emit) {
  REQUIRE(last <= CorpusSize);
  for (size_t index = first; index < last; ++index) {
    checkProgram(Corpus[index], emit);
  }
}

} // namespace

TEST_CASE("library corpus has at least 20 programs") {
  CHECK(CorpusSize >= 20);
}

TEST_CASE("library corpus on the vm: programs 0 to 2") {
  checkRange(0, 3, "vm");
}

TEST_CASE("library corpus on the vm: programs 3 to 5") {
  checkRange(3, 6, "vm");
}

TEST_CASE("library corpus on the vm: programs 6 to 8") {
  checkRange(6, 9, "vm");
}

TEST_CASE("library corpus on the vm: programs 9 to 11") {
  checkRange(9, 12, "vm");
}

TEST_CASE("library corpus on the vm: programs 12 to 14") {
  checkRange(12, 15, "vm");
}

TEST_CASE("library corpus on the vm: programs 15 to 17") {
  checkRange(15, 18, "vm");
}

TEST_CASE("library corpus on the vm: programs 18 to 20") {
  checkRange(18, 21, "vm");
}

TEST_CASE("library corpus on the vm: the rest") {
  checkRange(21, 23, "vm");
}

TEST_CASE("library corpus on the vm: generic struct") {
  checkRange(23, 24, "vm");
}

TEST_CASE("library corpus on the vm: generic reaching private definitions") {
  checkRange(24, 25, "vm");
}

TEST_CASE("library corpus on the vm: generic instantiated in both") {
  checkRange(25, 26, "vm");
}

TEST_CASE("library corpus on the vm: struct with a Destroy helper") {
  checkRange(26, 27, "vm");
}

TEST_CASE("library corpus on the vm: lifecycle helpers reaching a private function") {
  checkRange(27, 28, "vm");
}

TEST_CASE("library corpus on the vm: methods with an explicit self") {
  checkRange(28, 29, "vm");
}

TEST_CASE("library corpus on the vm: a sum result") {
  checkRange(29, 30, "vm");
}

TEST_CASE("library corpus on the vm: a sum result with an owned payload") {
  checkRange(30, 31, "vm");
}

TEST_CASE("library corpus on the vm: a vector result") {
  checkRange(31, CorpusSize, "vm");
}

TEST_CASE("library corpus on optexe: structs and methods") {
  checkRange(2, 3, "optexe");
}

TEST_CASE("library corpus on optexe: mut methods") {
  checkRange(3, 4, "optexe");
}

TEST_CASE("library corpus on optexe: struct parameters and results") {
  checkRange(4, 5, "optexe");
}

TEST_CASE("library corpus on optexe: effects and strings") {
  checkRange(7, 8, "optexe");
}

TEST_CASE("library corpus on optexe: tasks") {
  checkRange(9, 10, "optexe");
}

TEST_CASE("library corpus on optexe: library and program share stdlib routines") {
  checkRange(18, 19, "optexe");
}

TEST_CASE("library corpus on optexe: wrapped library struct") {
  checkRange(21, 22, "optexe");
}

TEST_CASE("library corpus on optexe: generic struct") {
  checkRange(23, 24, "optexe");
}

TEST_CASE("library corpus on optexe: generic reaching private definitions") {
  checkRange(24, 25, "optexe");
}

TEST_CASE("library corpus on optexe: generic instantiated in both") {
  checkRange(25, 26, "optexe");
}

TEST_CASE("library corpus on optexe: struct with a Destroy helper") {
  checkRange(26, 27, "optexe");
}

TEST_CASE("library corpus on optexe: lifecycle helpers reaching a private function") {
  checkRange(27, 28, "optexe");
}

TEST_CASE("library corpus on optexe: methods with an explicit self") {
  checkRange(28, 29, "optexe");
}

TEST_CASE("library corpus on optexe: a sum result") {
  checkRange(29, 30, "optexe");
}

TEST_CASE("library corpus on optexe: a sum result with an owned payload") {
  checkRange(30, 31, "optexe");
}

TEST_CASE("library corpus on optexe: a vector result") {
  checkRange(31, CorpusSize, "optexe");
}

TEST_SUITE_END();
