#include "program_matrix.h"

TEST_SUITE_BEGIN("primestruct.program_matrix.element_copies");

// A binding from a vector element is a copy of a place (docs/spec/value-lifecycle.md, Copies): the
// element keeps owning its fields and the binding gets its own, through the fields' Copy helpers.
// String does not run on the native backend yet, so programs with String fields run on the VM and
// optexe; the others also run natively.

namespace {

std::vector<std::string> vmOptexeConfigs() {
  return {"vm-step-O0", "vm-O0", "vm-O2", "optexe-O2"};
}

std::vector<std::string> allBackendConfigs() {
  std::vector<std::string> configs = vmOptexeConfigs();
  if (program_matrix::nativeSupported()) {
    configs.push_back("native-O0");
    configs.push_back("native-O2");
  }
  return configs;
}

} // namespace

TEST_CASE("a binding from a vector element copies its String, vector and helper fields") {
  // `v[i]` and `v.at(i)`: the copies and the elements change apart, and each Noisy is destroyed
  // once (1 by remove_at; the copies 1002 and 1001 and the last element 2 at scope end). A String
  // freed twice stops the run.
  program_matrix::ProgramCase program;
  program.name = "element_binding_owned_fields";
  program.source = R"(
import /std/collections/*
import /std/text/*

[struct]
Noisy() {
  [i32 mut] id{0i32}
  [public] Copy([Reference<Self>] other) { assign(this.id, other.id + 1000i32) }
  [public effects(io_out)] Destroy() { print_line(this.id) }
}

[struct]
Todo() {
  [i32] id{0i32}
  [String mut] title{String{}}
  [vector<i32> mut] tags{vector<i32>()}
  [Noisy mut] tag{Noisy{}}
}

[effects(heap_alloc io_out) return<int>]
main() {
  [vector<Todo> mut] todos{vector<Todo>()}
  todos.push(Todo{[id] 1i32, [title] stringFrom("one"), [tag] Noisy{1i32}})
  todos.push(Todo{[id] 2i32, [title] stringFrom("two"), [tag] Noisy{2i32}})
  todos[0i32].tags.push(7i32)
  [Todo mut] indexed{todos[0i32]}
  indexed.title.append("!!")
  indexed.tags.push(8i32)
  todos[0i32].title.append("?")
  [Todo mut] called{todos.at(1i32)}
  called.title.append("+")
  print_line(indexed.title.count() * 10i32 + todos[0i32].title.count())
  print_line(indexed.tags.count() * 10i32 + todos[0i32].tags.count())
  print_line(called.title.count() * 10i32 + todos[1i32].title.count())
  print_line(indexed.tag.id + called.tag.id)
  todos.remove_at(0i32)
  print_line(100i32)
  print_line(indexed.title.count() + todos.count() * 10i32)
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "54\n21\n43\n2003\n1\n100\n15\n1002\n1001\n2\n";
  program.onlyConfigs = vmOptexeConfigs();
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("a binding from a field of a vector element copies it") {
  program_matrix::ProgramCase program;
  program.name = "element_field_binding";
  program.source = R"(
import /std/collections/*
import /std/text/*

[struct]
Noisy() {
  [i32 mut] id{0i32}
  [public] Copy([Reference<Self>] other) { assign(this.id, other.id + 1000i32) }
  [public effects(io_out)] Destroy() { print_line(this.id) }
}

[struct]
Label() {
  [String mut] text{String{}}
  [Noisy mut] tag{Noisy{}}
}

[struct]
Item() {
  [i32] id{0i32}
  [Label mut] label{Label{}}
}

[effects(heap_alloc io_out) return<int>]
main() {
  [vector<Item> mut] items{vector<Item>()}
  items.push(Item{[id] 1i32, [label] Label{[text] stringFrom("abc"), [tag] Noisy{5i32}}})
  [Label mut] label{items[0i32].label}
  label.text.append("d")
  print_line(label.text.count() * 10i32 + items[0i32].label.text.count())
  print_line(label.tag.id)
  items.remove_at(0i32)
  print_line(100i32)
  print_line(label.text.count())
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "43\n1005\n5\n100\n4\n1005\n";
  program.onlyConfigs = vmOptexeConfigs();
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("a brace field, an assignment and a return from a vector element copy it") {
  program_matrix::ProgramCase program;
  program.name = "element_brace_assign_return";
  program.source = R"(
import /std/collections/*
import /std/text/*

[struct]
Noisy() {
  [i32 mut] id{0i32}
  [public] Copy([Reference<Self>] other) { assign(this.id, other.id + 1000i32) }
  [public effects(io_out)] Destroy() { print_line(this.id) }
}

[struct]
Todo() {
  [i32] id{0i32}
  [String mut] title{String{}}
  [Noisy mut] tag{Noisy{}}
}

[struct]
Holder() {
  [Todo mut] todo{Todo{}}
}

[return<Todo> effects(heap_alloc)]
first([vector<Todo> mut] todos) {
  return(todos[0i32])
}

[effects(heap_alloc io_out) return<int>]
main() {
  [vector<Todo> mut] todos{vector<Todo>()}
  todos.push(Todo{[id] 1i32, [title] stringFrom("one"), [tag] Noisy{1i32}})
  todos.push(Todo{[id] 2i32, [title] stringFrom("two"), [tag] Noisy{2i32}})
  print_line(100i32)
  [Holder mut] h{Holder{[todo] todos[0i32]}}
  h.todo.title.append("!")
  print_line(101i32)
  [Todo mut] t{Todo{}}
  t = todos[1i32]
  t.title.append("?")
  print_line(102i32)
  [Todo] r{first(todos)}
  print_line(r.tag.id)
  print_line(103i32)
  todos.remove_at(0i32)
  print_line(104i32)
  print_line(h.todo.title.count() + t.title.count() * 10i32 + todos[0i32].title.count() * 100i32)
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "100\n101\n0\n102\n1001\n103\n1\n104\n344\n1001\n1002\n1001\n2\n";
  program.onlyConfigs = vmOptexeConfigs();
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("a temporary or moved initializer is not copied") {
  // A call result, `move(x)` and a brace constructor move in (no Copy, no 1000 added); only the
  // element binding copies.
  program_matrix::ProgramCase program;
  program.name = "element_binding_moves_stay_moves";
  program.source = R"(
import /std/collections/*
import /std/text/*

[struct]
Noisy() {
  [i32 mut] id{0i32}
  [public] Copy([Reference<Self>] other) { assign(this.id, other.id + 1000i32) }
  [public effects(io_out)] Destroy() { print_line(this.id) }
}

[struct]
Todo() {
  [String mut] title{String{}}
  [Noisy mut] tag{Noisy{}}
}

[return<Todo> effects(heap_alloc)]
make_todo([i32] id) {
  return(Todo{[title] stringFrom("new"), [tag] Noisy{id}})
}

[effects(heap_alloc io_out) return<int>]
main() {
  [vector<Todo> mut] todos{vector<Todo>()}
  todos.push(make_todo(1i32))
  [Todo mut] fresh{make_todo(2i32)}
  print_line(fresh.tag.id)
  [Todo mut] moved{move(fresh)}
  print_line(moved.tag.id)
  [Todo] built{Todo{[title] stringFrom("built"), [tag] Noisy{3i32}}}
  print_line(built.tag.id)
  [Todo] element{todos[0i32]}
  print_line(element.tag.id)
  print_line(100i32)
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "2\n2\n3\n1001\n100\n1001\n3\n2\n1\n";
  program.onlyConfigs = vmOptexeConfigs();
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("a user at that returns a new value is moved in, not copied") {
  // Shaped like an element access, but `Maker.at` and `at_unsafe` are definitions returning a new
  // Noisy: the binding takes it over (no 1000 added) and destroys it once.
  program_matrix::ProgramCase program;
  program.name = "user_at_result_is_a_temporary";
  program.source = R"(
import /std/collections/*

[struct]
Noisy() {
  [i32 mut] id{0i32}
  [public] Copy([Reference<Self>] other) { assign(this.id, other.id + 1000i32) }
  [public effects(io_out)] Destroy() { print_line(this.id) }
}

[struct]
Maker() {
  [i32] base{10i32}

  [return<Noisy>]
  at([i32] i) {
    return(Noisy{this.base + i})
  }
}

[return<Noisy>]
at_unsafe([i32] a, [i32] b) {
  return(Noisy{a + b})
}

[effects(heap_alloc io_out) return<int>]
main() {
  [Maker] maker{Maker{}}
  [Noisy] n{maker.at(1i32)}
  print_line(n.id)
  [Noisy] m{at_unsafe(20i32, 2i32)}
  print_line(m.id)
  print_line(100i32)
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "11\n22\n100\n22\n11\n";
  program.onlyConfigs = allBackendConfigs();
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("a binding from a Function-holding element clones the closure") {
  // The copy's closure state is its own: 5 + 5 on the copy, 1 on the element, and the copy still
  // runs after the element is removed.
  program_matrix::ProgramCase program;
  program.name = "element_binding_function_clone";
  program.source = R"(
import /std/collections/*

[struct]
Route() {
  [i32] code{0i32}
  [Function<i32, i32> mut] handler

  [mut return<i32>]
  run([i32] x) {
    return(this.handler(x))
  }
}

[effects(heap_alloc) return<int>]
main() {
  [vector<Route> mut] routes{vector<Route>()}
  routes.push(Route{[code] 1i32, [handler] Function<i32, i32>{[mut seen{0i32}]([i32] x) {
    seen = seen + x
    return(seen)
  }}})
  [Route mut] copy{routes[0i32]}
  copy.run(5i32)
  [i32] copied{copy.run(5i32)}
  [i32] original{routes[0i32].run(1i32)}
  routes.remove_at(0i32)
  [i32] after{copy.run(1i32)}
  return(copied * 100i32 + original * 10i32 + after - 1000i32)
}
)";
  program.exitCode = 21;
  program.onlyConfigs = allBackendConfigs();
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("a binding from an element of a plain int struct is a copy") {
  // A struct of one scalar kind (Point) lives as an array local; a binding from an element or a
  // local gets its own slots, like a mixed struct (Mark).
  program_matrix::ProgramCase program;
  program.name = "element_binding_scalar_struct";
  program.source = R"(
import /std/collections/*

[struct]
Point() {
  [i32 mut] x{0i32}
  [i32 mut] y{0i32}
}

[struct]
Mark() {
  [i32 mut] x{0i32}
  [bool mut] seen{false}
}

[effects(heap_alloc) return<int>]
main() {
  [vector<Point> mut] points{vector<Point>()}
  points.push(Point{[x] 1i32, [y] 2i32})
  [Point mut] indexed{points[0i32]}
  indexed.x = 5i32
  [Point mut] called{points.at(0i32)}
  called.y = 7i32
  [Point mut] local{indexed}
  local.x = 9i32
  [vector<Mark> mut] marks{vector<Mark>()}
  marks.push(Mark{[x] 3i32, [seen] true})
  [Mark mut] mark{marks[0i32]}
  mark.x = 4i32
  return(indexed.x + points[0i32].x * 10i32 + points[0i32].y * 20i32 + called.y + local.x * 10i32 +
         marks[0i32].x * 30i32 + mark.x)
}
)";
  program.exitCode = 246;
  program.onlyConfigs = allBackendConfigs();
  program_matrix::runProgramMatrix(program);
}

TEST_SUITE_END();
