#include "third_party/doctest.h"

#include "test_semantics_helpers.h"

TEST_SUITE_BEGIN("primestruct.semantics.type_aliases");

// `[alias<Type>] Name() {}` names a type (docs/spec/type-system.md, Type Aliases): the type-aliases pass replaces each
// use with the target before any other semantic pass reads the program.

TEST_CASE("an alias of a Function type and of a generic struct is used in every type position") {
  const std::string source = R"(
import /std/collections/*

[struct]
Box<T>() {
  [T] value{}
}

[alias<Box<i32>>]
IntBox() {}

[alias<IntBox>]
Cell() {}

[alias<Function<i32, i32>>]
Step() {}

[alias<Box<T>>]
Wrapped<T>() {}

[struct]
Holder() {
  [Step mut] step
  [Cell] cell{Cell{[value] 1i32}}
}

[return<Step> effects(heap_alloc)]
make_step() {
  return([]([i32] x) { return(x + 1i32) })
}

[return<IntBox>]
unwrap([Wrapped<i32>] wrapped) {
  return(IntBox{[value] wrapped.value})
}

[return<int> effects(heap_alloc)]
main() {
  [Holder mut] holder{Holder{[step] make_step()}}
  [vector<IntBox> mut] cells{vector<IntBox>()}
  cells.push(unwrap(Wrapped<i32>{[value] 4i32}))
  return(holder.step(holder.cell.value) + cells.at(0i32).value)
}
)";
  std::string error;
  CHECK_MESSAGE(validateProgram(source, "/main", error), error);
}

TEST_CASE("a public alias is visible through an import and a private one is not") {
  const std::string visible = R"(
namespace lib {
  [struct public]
  Box<T>() {
    [public T] value{}
  }

  [public alias<Box<i32>>]
  IntBox() {}

  [alias<Box<i64>>]
  Hidden() {}

  [public return<i64>]
  inner() {
    [Hidden] hidden{Hidden{[value] 9i64}}
    return(hidden.value)
  }
}

import /lib/*

[return<int>]
main() {
  [IntBox] box{IntBox{[value] 1i32}}
  [IntBox] other{box}
  return(plus(plus(box.value, other.value), convert<i32>(inner())))
}
)";
  std::string error;
  CHECK_MESSAGE(validateProgram(visible, "/main", error), error);

  const std::string hidden = R"(
namespace lib {
  [struct public]
  Thing() {
    [public i32] value{1i32}
  }

  [alias<i32>]
  Hidden() {}
}

import /lib/*

[return<int>]
main() {
  [Hidden] x{1i32}
  return(x)
}
)";
  CHECK_FALSE(validateProgram(hidden, "/main", error));
  CHECK_MESSAGE(error.find("unsupported binding type: Hidden") != std::string::npos, error);

  const std::string importedPrivate = R"(
namespace lib {
  [alias<i32>]
  Hidden() {}
}

import /lib/Hidden

[return<int>]
main() {
  return(0i32)
}
)";
  CHECK_FALSE(validateProgram(importedPrivate, "/main", error));
  CHECK_MESSAGE(error.find("import path refers to private definition: /lib/Hidden") != std::string::npos, error);
}

TEST_CASE("aliases that refer to each other are a cycle") {
  const std::string source = R"(
[alias<B>]
A() {}

[alias<A>]
B() {}

[return<int>]
main() {
  [A] x{1i32}
  return(0i32)
}
)";
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_MESSAGE(error.find("type alias cycle: /A -> /B -> /A") != std::string::npos, error);
}

TEST_CASE("an alias takes exactly the template arguments it declares") {
  const std::string common = R"(
[struct]
Box<T>() {
  [T] value{}
}

[alias<Box<T>>]
Wrapped<T>() {}

[alias<Box<i32>>]
IntBox() {}
)";
  std::string error;
  CHECK_FALSE(validateProgram(common + R"(
[return<int>]
main() {
  [Wrapped] x{Wrapped{[value] 1i32}}
  return(0i32)
}
)",
                              "/main", error));
  CHECK_MESSAGE(error.find("type alias /Wrapped takes 1 template argument, 0 given") != std::string::npos, error);

  CHECK_FALSE(validateProgram(common + R"(
[return<int>]
main() {
  [IntBox<i32>] x{IntBox{[value] 1i32}}
  return(0i32)
}
)",
                              "/main", error));
  CHECK_MESSAGE(error.find("type alias /IntBox takes 0 template arguments, 1 given") != std::string::npos, error);
}

TEST_CASE("an alias has a target and no parameters or body") {
  std::string error;
  CHECK_FALSE(validateProgram(R"(
[alias<i32>]
Bad([i32] x) {
  return(x)
}

[return<int>]
main() {
  return(0i32)
}
)",
                              "/main", error));
  CHECK_MESSAGE(error.find("type alias /Bad has no parameters and no body") != std::string::npos, error);
}

TEST_CASE("the handler aliases of /std/http name the route handler types beside a template parameter and a field") {
  const std::string source = R"(
import /std/http/*

[struct]
Counter() {
  [i32 mut] hits{0i32}
}

[struct]
Server() {
  [RequestHandler mut] handler
}

[return<int> effects(heap_alloc)]
call_with<Handler>([Handler mut] handler, [Request] request, [Reference<Response>] response) {
  handler(request, response)
  return(response.status)
}

[return<int> effects(heap_alloc)]
main() {
  [Router mut] router{Router{}}
  router.get("/", []([Request] request, [Reference<Response>] response) { response.status = 200i32 })
  [AppRouter<Counter> mut] app{AppRouter<Counter>{[state] Counter{}}}
  [StateRequestHandler<Counter> mut] counted{[]([Reference<Counter> mut] counter, [Request] request,
                                               [Reference<Response>] response) { counter.hits = counter.hits + 1i32 }}
  app.get("/count", move(counted))
  [Server mut] server{Server{[handler] []([Request] request, [Reference<Response>] response) {
    response.status = 204i32
  }}}
  [Response mut] response{Response{}}
  [Request] request{Request{}}
  return(call_with(server.handler, request, response))
}
)";
  std::string error;
  CHECK_MESSAGE(validateProgram(source, "/main", error), error);
}

TEST_SUITE_END();
