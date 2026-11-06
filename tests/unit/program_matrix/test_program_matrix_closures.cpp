#include "program_matrix.h"

TEST_SUITE_BEGIN("primestruct.program_matrix.closures");

// Lambdas are closure structs (docs/spec/value-lifecycle.md, Lambdas and closures): captures are fields, the body is
// a `call` method, and `f(x)` on a closure value calls it. These programs run on every execution config plus optexe,
// the backend web servers are built with.

namespace {

std::vector<std::string> closureConfigs() {
  std::vector<std::string> configs = {"vm-step-O0", "vm-O0", "vm-O2", "optexe-O2"};
  if (program_matrix::nativeSupported()) {
    configs.push_back("native-O0");
    configs.push_back("native-O2");
  }
  return configs;
}

void runClosureProgram(const std::string &name, const std::string &source, int exitCode) {
  program_matrix::ProgramCase program;
  program.name = name;
  program.source = source;
  program.exitCode = exitCode;
  program.onlyConfigs = closureConfigs();
  program_matrix::runProgramMatrix(program);
}

} // namespace

TEST_CASE("closure in a local reads a by-value capture") {
  runClosureProgram("closure_local_value_capture", R"(
[return<int>]
main() {
  [i32] base{3i32}
  add{[base]([i32] x) { return(x + base) }}
  return(add(4i32))
}
)",
                    7);
}

TEST_CASE("init capture keeps state across calls") {
  runClosureProgram("closure_init_capture_counter", R"(
[return<int>]
main() {
  [mut] counter{[mut calls{0i32}]() {
    calls = calls + 1i32
    return(calls)
  }}
  counter()
  counter()
  return(counter())
}
)",
                    3);
}

TEST_CASE("mut value capture changes only the closure's copy") {
  runClosureProgram("closure_mut_value_capture", R"(
[return<int>]
main() {
  [i32 mut] count{10i32}
  [mut] bump{[mut count]() {
    count = count + 1i32
    return(count)
  }}
  [i32] first{bump()}
  [i32] second{bump()}
  return(first + second + count)
}
)",
                    33);
}

TEST_CASE("stateful closure passed to a generic mut parameter") {
  runClosureProgram("closure_generic_mut_parameter", R"(
[return<i32>]
drive<Handler>([Handler mut] handler, [i32] times) {
  [i32 mut] total{0i32}
  [i32 mut] i{0i32}
  while(i < times) {
    total = total + handler(i)
    i = i + 1i32
  }
  return(total)
}

[return<int>]
main() {
  [i32] step{2i32}
  [mut] scaled{[step, mut calls{0i32}]([i32] x) {
    calls = calls + 1i32
    return(x * step + calls)
  }}
  [i32] total{drive(scaled, 3i32)}
  return(total * 10i32 + scaled(10i32))
}
)",
                    144);
}

TEST_CASE("capture-all captures only the names the body uses") {
  runClosureProgram("closure_capture_all_used_names", R"(
[return<int>]
main() {
  [i32] scale{2i32}
  [i32] offset{5i32}
  [i32] unused{100i32}
  affine{[=]([i32] x) { return(x * scale + offset) }}
  return(affine(3i32))
}
)",
                    11);
}

TEST_CASE("nested lambda captures through the outer closure") {
  runClosureProgram("closure_nested_lambda", R"(
[return<int>]
main() {
  [i32] base{4i32}
  outer{[base]([i32] x) {
    inner{[base]([i32] y) { return(y + base) }}
    return(inner(x) * 2i32)
  }}
  return(outer(1i32))
}
)",
                    10);
}

TEST_CASE("closure stored in a generic struct field and in a vector") {
  runClosureProgram("closure_field_and_vector", R"(
import /std/collections/*

[struct]
Holder<Handler> {
  [i32] code{0i32}
  [Handler mut] handler
}

[return<vector<Handler>> effects(heap_alloc)]
twice<Handler>([Handler] handler) {
  [vector<Handler> mut] out{vector<Handler>()}
  out.push(handler)
  out.push(handler)
  return(out)
}

[return<int> effects(heap_alloc)]
main() {
  [i32] base{3i32}
  [mut] holder{Holder{[code] 1i32, [handler] [base, mut calls{0i32}]([i32] x) {
    calls = calls + 1i32
    return(x + base + calls)
  }}}
  holder.handler(1i32)
  [i32] second{holder.handler(1i32)}
  adder{[base]([i32] x) { return(x + base) }}
  [mut] handlers{twice(adder)}
  [i32] third{handlers[1i32](10i32)}
  return(second * 10i32 + third)
}
)",
                    73);
}

TEST_CASE("closure owning a captured vector") {
  runClosureProgram("closure_captured_vector", R"(
import /std/collections/*

[return<int> effects(heap_alloc)]
main() {
  [vector<i32> mut] values{vector<i32>(1i32, 2i32, 3i32)}
  total{[values]() { return(values.count()) }}
  values.push(4i32)
  return(total() * 10i32 + values.count())
}
)",
                    34);
}

TEST_CASE("hand-written struct with a call method is callable") {
  runClosureProgram("closure_callable_struct", R"(
[struct]
Counter {
  [i32 mut] calls{0i32}

  [mut return<i32>]
  call([i32] step) {
    this.calls = this.calls + step
    return(this.calls)
  }
}

[return<i32>]
apply_twice<Handler>([Handler mut] handler) {
  handler(2i32)
  return(handler(3i32))
}

[return<int>]
main() {
  [Counter mut] counter{Counter{}}
  return(apply_twice(counter) * 10i32 + counter.calls)
}
)",
                    55);
}

TEST_CASE("struct methods reach root-level generic functions") {
  runClosureProgram("closure_method_calls_root_generic", R"(
[return<T>]
identity<T>([T] value) {
  return(value)
}

[struct]
Box<T> {
  [T] value

  [return<T>]
  read() {
    return(identity(this.value))
  }
}

[return<int>]
main() {
  [Box<i32>] box{Box<i32>{[value] 6i32}}
  return(box.read())
}
)",
                    6);
}

TEST_CASE("Function values hold different closures in one vector") {
  runClosureProgram("function_router_vector", R"(
import /std/collections/*

[struct]
Route {
  [i32] code
  [Function<i32, i32> mut] handler
}

[return<i32> effects(heap_alloc)]
route_request([vector<Route> mut] routes, [i32] code, [i32] input) {
  [i32 mut] i{0i32}
  while(i < routes.count()) {
    if(routes[i].code == code) {
      return(routes[i].handler(input))
    }
    i = i + 1i32
  }
  return(404i32)
}

[return<int> effects(heap_alloc)]
main() {
  [i32] base{100i32}
  [mut] hits{[mut calls{0i32}]([i32] input) {
    calls = calls + 1i32
    return(calls)
  }}
  [vector<Route> mut] routes{vector<Route>()}
  routes.push(Route{[code] 1i32, [handler] Function<i32, i32>{hits}})
  routes.push(Route{[code] 2i32, [handler] Function<i32, i32>{[base]([i32] input) { return(input + base) }}})
  route_request(routes, 1i32, 0i32)
  [i32] second{route_request(routes, 1i32, 0i32)}
  [i32] offset{route_request(routes, 2i32, 5i32)}
  [i32] missing{route_request(routes, 3i32, 0i32)}
  return(second + offset + missing - 400i32)
}
)",
                    111);
}

TEST_CASE("Function copies clone the closure state") {
  runClosureProgram("function_copy_clones_state", R"(
[return<int> effects(heap_alloc)]
main() {
  [Function<i32, i32> mut] counter{Function<i32, i32>{[mut total{0i32}]([i32] step) {
    total = total + step
    return(total)
  }}}
  counter(5i32)
  [Function<i32, i32> mut] twin{counter}
  twin(10i32)
  [i32] original{counter(1i32)}
  [i32] copied{twin(1i32)}
  return(original * 100i32 + copied)
}
)",
                    616);
}

TEST_CASE("Function parameter and zero-argument hooks") {
  runClosureProgram("function_parameter_hooks", R"(
import /std/collections/*

[return<i32> effects(heap_alloc)]
run_all([vector<Function<i32>> mut] hooks) {
  [i32 mut] total{0i32}
  [i32 mut] i{0i32}
  while(i < hooks.count()) {
    total = total + hooks[i]()
    i = i + 1i32
  }
  return(total)
}

[return<i32> effects(heap_alloc)]
call_twice([Function<i32> mut] hook) {
  hook()
  return(hook())
}

[return<int> effects(heap_alloc)]
main() {
  [vector<Function<i32>> mut] hooks{vector<Function<i32>>()}
  hooks.push(Function<i32>{[]() { return(1i32) }})
  hooks.push(Function<i32>{[mut n{10i32}]() {
    n = n + 10i32
    return(n)
  }})
  [i32] first{run_all(hooks)}
  [i32] second{run_all(hooks)}
  [Function<i32> mut] ticker{Function<i32>{[mut n{0i32}]() {
    n = n + 1i32
    return(n)
  }}}
  return(first + second + call_twice(ticker))
}
)",
                    54);
}

TEST_CASE("ref capture writes the enclosing binding") {
  runClosureProgram("closure_ref_capture_writes", R"(
[return<i32>]
apply<Handler>([Handler mut] handler) {
  return(handler())
}

[return<int>]
main() {
  [i32 mut] count{0i32}
  [mut] bump{[ref count]() {
    count = count + 1i32
    return(count)
  }}
  bump()
  [i32] seen{apply(bump)}
  return(seen * 10i32 + count)
}
)",
                    22);
}

TEST_CASE("ref-captured vector of Function routes passes to a parameter") {
  runClosureProgram("closure_ref_capture_struct_vector_parameter", R"(
import /std/collections/*

[struct]
Route {
  [i32] code
  [Function<i32, i32> mut] handler
}

[return<i32> effects(heap_alloc)]
call_all([vector<Route> mut] routes, [i32] input) {
  [i32 mut] total{0i32}
  [i32 mut] i{0i32}
  while(i < routes.count()) {
    total = total + routes[i].handler(input)
    i = i + 1i32
  }
  return(total)
}

[return<i32> effects(heap_alloc)]
through([Reference<vector<Route>> mut] routes, [i32] input) {
  return(call_all(routes, input))
}

[return<int> effects(heap_alloc)]
main() {
  [vector<Route> mut] routes{vector<Route>()}
  routes.push(Route{[code] 1i32, [handler] Function<i32, i32>{[]([i32] x) { return(x + 1i32) }}})
  routes.push(Route{[code] 2i32, [handler] Function<i32, i32>{[]([i32] x) { return(x * 2i32) }}})
  [mut] app{[ref routes]([i32] x) { return(call_all(routes, x)) }}
  return(app(10i32) + through(location(routes), 1i32))
}
)",
                    35);
}

TEST_CASE("ref-captured vector is indexed in the lambda body and in a method") {
  runClosureProgram("closure_ref_capture_vector_index", R"(
import /std/collections/*

[struct]
Picker {
  [i32] base

  [return<i32>]
  pick([Reference<vector<i32>>] values, [i32] input) {
    return(values[input] + this.base)
  }
}

[return<int> effects(heap_alloc)]
main() {
  [vector<i32> mut] values{vector<i32>(5i32, 7i32, 9i32)}
  [mut] pick{[ref values]([i32] input) { return(values[input]) }}
  [Picker] picker{Picker{[base] 100i32}}
  return(pick(1i32) + picker.pick(location(values), 2i32))
}
)",
                    116);
}

TEST_CASE("method calls on a field of an indexed element") {
  runClosureProgram("indexed_element_field_method_chain", R"(
import /std/collections/*
import /std/text/*
import /std/text/view/*

[struct]
Named {
  [i32] code
  [String] name
}

[return<int> effects(heap_alloc)]
main() {
  [vector<Named> mut] all{vector<Named>()}
  all.push(Named{[code] 1i32, [name] stringFrom("abc")})
  all.push(Named{[code] 2i32, [name] stringFrom("hello")})
  [i32 mut] sum{0i32}
  [i32 mut] i{0i32}
  while(i < all.count()) {
    sum = sum + all[i].code + all[i].name.count()
    i = i + 1i32
  }
  return(sum * 10i32 + all[1i32].name.view().count())
}
)",
                    115);
}

TEST_CASE("view equality inside a function with a String parameter") {
  runClosureProgram("string_parameter_view_equality", R"(
import /std/text/*
import /std/text/view/*

[return<bool> effects(heap_alloc)]
same_field([String] owned, [StringView] wanted) {
  [StringView] mine{owned.view()}
  return(mine == wanted)
}

[return<bool> effects(heap_alloc)]
same_inline([String] owned, [StringView] wanted) {
  return(owned.view() == wanted)
}

[return<int> effects(heap_alloc)]
main() {
  [String] a{stringFrom("host")}
  [String] b{stringFrom("host")}
  [String] c{stringFrom("port")}
  [i32] first{if(same_field(a, b.view()), then() { 1i32 }, else() { 0i32 })}
  [i32] second{if(same_inline(a, b.view()), then() { 10i32 }, else() { 0i32 })}
  [i32] third{if(same_inline(a, c.view()), then() { 100i32 }, else() { 0i32 })}
  return(first + second + third)
}
)",
                    11);
}

TEST_CASE("struct vector fields keep pushes through the field and through this") {
  runClosureProgram("struct_vector_field_push_count", R"(
import /std/collections/*

[struct]
KeepV {
  [vector<i32> mut] seen

  [mut return<i32> effects(heap_alloc)]
  add([i32] x) {
    this.seen.push(x)
    return(this.seen.count())
  }
}

[struct]
Defaulted {
  [vector<i32> mut] seen{vector<i32>()}
}

[return<int> effects(heap_alloc)]
main() {
  [KeepV mut] k{KeepV{[seen] vector<i32>(5i32)}}
  k.seen.push(1i32)
  [i32] direct{k.seen.count()}
  [i32] viaThis{k.add(7i32)}
  [Defaulted mut] d{Defaulted{}}
  d.seen.push(1i32)
  return(direct * 100i32 + viaThis * 10i32 + d.seen.count())
}
)",
                    231);
}

TEST_CASE("a copied struct's vector field is independent and a closure can own a vector") {
  runClosureProgram("struct_vector_field_copy_and_closure", R"(
import /std/collections/*

[struct]
KeepV {
  [vector<i32> mut] seen
}

[return<int> effects(heap_alloc)]
main() {
  [KeepV mut] k{KeepV{[seen] vector<i32>(5i32)}}
  [KeepV mut] copy{k}
  copy.seen.push(1i32)
  [mut] log{[mut values{vector<i32>()}]([i32] x) {
    values.push(x)
    return(values.count())
  }}
  log(1i32)
  log(2i32)
  [i32] logged{log(3i32)}
  return(copy.seen.count() * 100i32 + k.seen.count() * 10i32 + logged)
}
)",
                    213);
}

TEST_CASE("a Router struct holds a vector of Function routes") {
  runClosureProgram("struct_vector_of_function_routes", R"(
import /std/collections/*

[struct]
Route {
  [i32] code
  [Function<i32, i32> mut] handler
}

[struct]
Router {
  [vector<Route> mut] routes

  [mut effects(heap_alloc)]
  add([i32] code, [Function<i32, i32> move] handler) {
    this.routes.push(Route{[code] code, [handler] handler})
  }

  [return<i32> effects(heap_alloc)]
  find([i32] code) {
    [i32 mut] i{0i32}
    while(i < this.routes.count()) {
      if(this.routes[i].code == code) {
        return(i)
      }
      i = i + 1i32
    }
    return(-1i32)
  }
}

[return<i32> effects(heap_alloc)]
route_to([Router mut] router, [i32] code, [i32] input) {
  [i32] at{router.find(code)}
  if(at < 0i32) {
    return(404i32)
  }
  return(router.routes[at].handler(input))
}

[return<int> effects(heap_alloc)]
main() {
  [Router mut] router{Router{[routes] vector<Route>()}}
  router.add(1i32, Function<i32, i32>{[]([i32] x) { return(x + 1i32) }})
  router.add(2i32, Function<i32, i32>{[]([i32] x) { return(x * 2i32) }})
  return(route_to(router, 2i32, 20i32) + route_to(router, 1i32, 1i32) + router.routes.count())
}
)",
                    44);
}

TEST_CASE("a struct field holds a vector of structs with String fields") {
  runClosureProgram("struct_vector_of_named_rows", R"(
import /std/collections/*
import /std/text/*

[struct]
Named {
  [i32] code
  [String] name
}

[struct]
Table {
  [vector<Named> mut] rows
}

[return<int> effects(heap_alloc)]
main() {
  [Table mut] t{Table{[rows] vector<Named>()}}
  t.rows.push(Named{[code] 3i32, [name] stringFrom("abc")})
  t.rows.push(Named{[code] 4i32, [name] stringFrom("hello")})
  [Table] copy{t}
  return(t.rows[1i32].code * 10i32 + t.rows[0i32].name.count() + copy.rows.count() * 100i32)
}
)",
                    243);
}

TEST_CASE("a Router method calls a route handler through this") {
  runClosureProgram("router_method_calls_route_handler", R"(
import /std/collections/*

[struct]
Route {
  [i32] code
  [Function<i32, i32> mut] handler
}

[struct]
Router {
  [vector<Route> mut] routes

  [mut effects(heap_alloc)]
  add([i32] code, [Function<i32, i32> move] handler) {
    this.routes.push(Route{[code] code, [handler] handler})
  }

  [return<i32> effects(heap_alloc)]
  find([i32] code) {
    [i32 mut] i{0i32}
    while(i < this.routes.count()) {
      if(this.routes[i].code == code) {
        return(i)
      }
      i = i + 1i32
    }
    return(-1i32)
  }

  [mut return<i32> effects(heap_alloc)]
  call([i32] code, [i32] input) {
    [i32] at{this.find(code)}
    if(at < 0i32) {
      return(404i32)
    }
    return(this.routes[at].handler(input))
  }
}

[return<i32> effects(heap_alloc)]
route_to([Router mut] router, [i32] code, [i32] input) {
  [i32] at{router.find(code)}
  if(at < 0i32) {
    return(404i32)
  }
  return(router.routes[at].handler(input))
}

[return<int> effects(heap_alloc)]
main() {
  [Router mut] router{Router{[routes] vector<Route>()}}
  router.add(1i32, Function<i32, i32>{[]([i32] x) { return(x + 1i32) }})
  router.add(2i32, Function<i32, i32>{[]([i32] x) { return(x * 2i32) }})
  return(router.call(2i32, 20i32) + router.call(1i32, 1i32) + router.routes.count())
}
)",
                    44);
}

TEST_CASE("return inside a Result.map lambda returns from the lambda") {
  runClosureProgram("closure_result_map_return", R"(
[return<int> effects(io_out, io_err) on_error<FileError, /log_file_error>]
main() {
  [i32] limit{5i32}
  [auto] mapped{Result.map(Result.ok(7i32), [limit]([i32] value) {
    if(value > limit) {
      return(100i32)
    }
    return(value)
  })}
  [i32] got{try(mapped)}
  return(got + 1i32)
}

[effects(io_err)]
log_file_error([FileError] err) {
}
)",
                    101);
}
