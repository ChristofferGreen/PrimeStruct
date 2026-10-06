#include "program_matrix.h"

TEST_SUITE_BEGIN("primestruct.program_matrix.lifecycle");

// Destructors run automatically (docs/spec/value-lifecycle.md): an owning local is destroyed
// when its scope ends on every exit path, unless its value was moved or returned; field
// initializers and assignments copy from places so no two values own the same storage.

TEST_CASE("Destroy runs at scope end in reverse order, and for a moved value in the callee") {
  program_matrix::ProgramCase program;
  program.name = "destroy_scope_end";
  program.source = R"(
[struct]
Noisy() {
  [i32 mut] id{0i32}
  [i64 mut] pad{0i64}

  [public effects(io_out)]
  Destroy() {
    print_line(this.id)
  }
}

[effects(io_out) return<void>]
scoped() {
  [Noisy] first{Noisy{1i32, 0i64}}
  [Noisy] second{Noisy{2i32, 0i64}}
  if(true) {
    [Noisy] inner{Noisy{3i32, 0i64}}
  }
}

[effects(io_out) return<i32>]
early([bool] leave) {
  [Noisy] guard{Noisy{4i32, 0i64}}
  if(leave) {
    return(1i32)
  }
  [Noisy] later{Noisy{5i32, 0i64}}
  return(2i32)
}

[effects(io_out) return<void>]
take([Noisy move] value) {
}

[effects(io_out) return<Noisy>]
make() {
  [Noisy] made{Noisy{6i32, 0i64}}
  return(made)
}

[effects(io_out) return<int>]
main() {
  scoped()
  print_line(100i32)
  early(true)
  print_line(101i32)
  early(false)
  print_line(102i32)
  [Noisy] moved{Noisy{7i32, 0i64}}
  take(move(moved))
  print_line(103i32)
  for([i32 mut] i{0i32}, i < 2i32, i++) {
    [Noisy] each{Noisy{8i32, 0i64}}
  }
  print_line(104i32)
  [Noisy] got{make()}
  print_line(105i32)
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "3\n2\n1\n100\n4\n101\n5\n4\n102\n7\n103\n8\n8\n104\n105\n6\n";
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("a return from a nested block destroys the enclosing locals") {
  program_matrix::ProgramCase program;
  program.name = "destroy_nested_return";
  program.source = R"(
[struct]
Noisy() {
  [i32 mut] id{0i32}
  [i64 mut] pad{0i64}

  [public effects(io_out)]
  Destroy() {
    print_line(this.id)
  }
}


[effects(io_out) return<i32>]
nested_return() {
  [Noisy] outer{Noisy{1i32, 0i64}}
  if(true) {
    [Noisy] inner{Noisy{2i32, 0i64}}
    return(1i32)
  }
  return(0i32)
}

[effects(io_out) return<int>]
main() {
  nested_return()
  print_line(100i32)
  for([i32 mut] i{0i32}, i < 3i32, i++) {
    [Noisy] each{Noisy{3i32, 0i64}}
    if(i == 1i32) {
      [Noisy] deep{Noisy{4i32, 0i64}}
    }
  }
  print_line(101i32)
  if(true) {
    [Noisy] top{Noisy{5i32, 0i64}}
    return(0i32)
  }
  return(1i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "2\n1\n100\n3\n4\n3\n3\n101\n5\n";
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("a struct field initialized from a local owns its own copy") {
  program_matrix::ProgramCase program;
  program.name = "destroy_constructor_field_copy";
  program.source = R"(
import /std/collections/*

[struct]
Bag() {
  [Vector<i32> mut] items{vector<i32>()}
  [i32 mut] tag{0i32}
}

[effects(io_out heap_alloc) return<int>]
main() {
  [Vector<i32> mut] items{vector<i32>()}
  vectorPush<i32>(items, 4i32)
  [Bag] bag{Bag{items, 1i32}}
  print_line(vectorCount<i32>(bag.items))
  print_line(vectorCount<i32>(items))
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "1\n1\n";
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("assigning an owning value destroys the old one and copies the new one") {
  program_matrix::ProgramCase program;
  program.name = "destroy_owned_assignment";
  program.source = R"(
import /std/collections/*

[struct]
Bag() {
  [Vector<i32> mut] items{vector<i32>()}
  [i32 mut] tag{0i32}
}

[effects(io_out heap_alloc) return<int>]
main() {
  [Vector<i32> mut] a{vector<i32>()}
  vectorPush<i32>(a, 4i32)
  [Vector<i32> mut] b{vector<i32>()}
  assign(b, a)
  vectorPush<i32>(b, 5i32)
  print_line(vectorCount<i32>(a))
  print_line(vectorCount<i32>(b))
  [Bag mut] bag{Bag{}}
  assign(bag.items, a)
  print_line(vectorCount<i32>(bag.items))
  b = vector<i32>()
  print_line(vectorCount<i32>(b))
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "1\n2\n1\n0\n";
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("a container destroys its elements when they are dropped") {
  program_matrix::ProgramCase program;
  program.name = "destroy_container_elements";
  program.source = R"(
import /std/collections/*

[struct]
Noisy() {
  [i32 mut] id{0i32}
  [i64 mut] pad{0i64}

  [public effects(io_out)]
  Destroy() {
    print_line(this.id)
  }
}

[effects(io_out heap_alloc) return<int>]
main() {
  [Vector<Noisy> mut] items{vector<Noisy>()}
  vectorPush<Noisy>(items, Noisy{1i32, 0i64})
  vectorPush<Noisy>(items, Noisy{2i32, 0i64})
  vectorPush<Noisy>(items, Noisy{3i32, 0i64})
  print_line(100i32)
  vectorPop<Noisy>(items)
  print_line(101i32)
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "100\n3\n101\n1\n2\n";
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("a callee destroys its copy and move parameters") {
  program_matrix::ProgramCase program;
  program.name = "destroy_owned_parameters";
  program.source = R"(
import /std/collections/*

[struct]
Noisy() {
  [i32 mut] id{0i32}
  [i64 mut] pad{0i64}

  [public]
  Copy([Reference<Self>] other) {
    assign(this.id, plus(other.id, 10i32))
    assign(this.pad, other.pad)
  }

  [public effects(io_out)]
  Destroy() {
    print_line(this.id)
  }
}

[effects(io_out) return<void>]
take_copy([Noisy copy] value) {
  print_line(200i32)
}

[effects(io_out) return<void>]
take_move([Noisy move] value) {
  print_line(201i32)
}

[effects(io_out heap_alloc) return<int>]
main() {
  [Noisy] kept{Noisy{1i32, 0i64}}
  take_copy(kept)
  print_line(100i32)
  [Noisy] given{Noisy{2i32, 0i64}}
  take_move(given)
  print_line(101i32)
  [Noisy] explicit{Noisy{3i32, 0i64}}
  take_move(move(explicit))
  print_line(102i32)
  [Vector<Noisy> mut] items{vector<Noisy>()}
  [Noisy] pushed{Noisy{4i32, 0i64}}
  vectorPush<Noisy>(items, pushed)
  print_line(103i32)
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "200\n11\n100\n201\n2\n101\n201\n3\n102\n103\n4\n1\n";
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("a struct whose fields share one scalar type is copied and destroyed") {
  program_matrix::ProgramCase program;
  program.name = "destroy_uniform_field_struct";
  program.source = R"(
[struct]
Solo() {
  [i32 mut] id{0i32}
  [i32 mut] tag{0i32}

  [public]
  Copy([Reference<Self>] other) {
    assign(this.id, plus(other.id, 10i32))
    assign(this.tag, other.tag)
  }

  [public effects(io_out)]
  Destroy() {
    print_line(this.id)
  }
}

[effects(io_out) return<void>]
take_copy([Solo copy] value) {
  print_line(200i32)
}

[effects(io_out) return<int>]
main() {
  [Solo mut] a{Solo{1i32, 0i32}}
  [Solo] b{a}
  print_line(100i32)
  assign(a, Solo{2i32, 0i32})
  print_line(101i32)
  take_copy(b)
  print_line(102i32)
  if(true) {
    [Solo] inner{Solo{3i32, 0i32}}
  }
  print_line(103i32)
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "100\n1\n101\n200\n21\n102\n3\n103\n11\n2\n";
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("a single-field struct is copied and destroyed, also through try on a map lookup") {
  program_matrix::ProgramCase program;
  program.name = "destroy_single_field_struct";
  program.source = R"(
import /std/collections/*
import /std/collections/map/*

[struct]
Solo() {
  [i32 mut] id{0i32}

  [public]
  Copy([Reference<Self>] other) {
    assign(this.id, plus(other.id, 10i32))
  }

  [public effects(io_out)]
  Destroy() {
    print_line(this.id)
  }
}

[effects(io_err)]
report_missing([ContainerError] err) {
  print_line_error(1i32)
}

[effects(io_out heap_alloc) return<Result<int, ContainerError>> on_error<ContainerError, /report_missing>]
main() {
  [Solo mut] a{Solo{1i32}}
  [Solo] b{a}
  print_line(100i32)
  assign(a, Solo{2i32})
  print_line(101i32)
  [Map<i32, Solo> mut] solos{mapSingle<i32, Solo>(7i32, Solo{3i32})}
  [Solo] found{try(solos.tryAt(7i32))}
  print_line(found.id)
  print_line(102i32)
  return(Result.ok(0i32))
}
)";
  program.exitCode = 0;
  program.stdoutText = "100\n1\n101\n13\n102\n13\n3\n11\n2\n";
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("returns, while bodies and block temporaries destroy exactly the values they own") {
  program_matrix::ProgramCase program;
  program.name = "destroy_returns_loops_blocks";
  program.source = R"(
import /std/collections/*

[struct]
Noisy() {
  [i32 mut] id{0i32}

  [public]
  Copy([Reference<Self>] other) {
    assign(this.id, plus(other.id, 10i32))
  }

  [public effects(io_out)]
  Destroy() {
    print_line(this.id)
  }
}

[struct]
Pair() {
  [Noisy mut] left{Noisy{1i32}}
  [Noisy mut] right{Noisy{2i32}}
}

[effects(io_out) return<i32>]
scalar_from_local() {
  [Noisy] a{Noisy{7i32}}
  return(plus(a.id, 1i32))
}

[effects(io_out heap_alloc) return<i32>]
count_of_local() {
  [Vector<Noisy> mut] v{vector<Noisy>()}
  vectorPush<Noisy>(v, Noisy{9i32})
  return(vectorCount<Noisy>(v))
}

[effects(io_out) return<Noisy>]
field_of_local() {
  [Pair] p{Pair{}}
  return(p.left)
}

[effects(io_out) return<Noisy>]
returned_local() {
  [Noisy] kept{Noisy{3i32}}
  return(kept)
}

[effects(io_out) return<i32>]
while_body([i32] limit) {
  [i32 mut] i{0i32}
  while(i < limit) {
    [Noisy] each{Noisy{plus(20i32, i)}}
    if(i == 1i32) {
      return(i)
    }
    i++
  }
  return(-1i32)
}

[effects(io_out heap_alloc) return<int>]
main() {
  print_line(scalar_from_local())
  print_line(100i32)
  print_line(count_of_local())
  print_line(101i32)
  [Noisy] left{field_of_local()}
  print_line(left.id)
  [Noisy] kept{returned_local()}
  print_line(kept.id)
  print_line(102i32)
  print_line(while_body(5i32))
  print_line(103i32)
  if(true) {
    [Vector<Noisy> mut] w{vector<Noisy>()}
    vectorPush<Noisy>(w, Noisy{6i32})
  }
  print_line(104i32)
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "7\n8\n100\n9\n1\n101\n2\n1\n11\n3\n102\n20\n21\n1\n103\n6\n104\n3\n11\n";
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("builtin vector locals destroy their elements in blocks, loops and callees") {
  program_matrix::ProgramCase program;
  program.name = "builtin_vector_locals_destroy_elements";
  program.source = R"(
import /std/collections/*

[struct]
Noisy() {
  [i32 mut] id{0i32}
  [public effects(io_out)] Destroy() { print_line(this.id) }
}

[effects(io_out heap_alloc)]
fill([i32] base) {
  [vector<Noisy> mut] inner{vector<Noisy>()}
  inner.push(Noisy{base})
  inner.push(Noisy{base + 1i32})
}

[effects(io_out heap_alloc) return<int>]
main() {
  if(true) {
    [vector<Noisy> mut] v{vector<Noisy>()}
    v.push(Noisy{5i32})
  }
  print_line(100i32)
  fill(10i32)
  print_line(101i32)
  [i32 mut] i{0i32}
  while(i < 2i32) {
    [vector<Noisy> mut] w{vector<Noisy>()}
    w.push(Noisy{20i32 + i})
    i = i + 1i32
  }
  print_line(102i32)
  [vector<Noisy> mut] last{vector<Noisy>()}
  vectorPush<Noisy>(last, Noisy{7i32})
  print_line(103i32)
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "5\n100\n10\n11\n101\n20\n21\n102\n103\n7\n";
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("returned and aliased builtin vectors are destroyed once by their owner") {
  program_matrix::ProgramCase program;
  program.name = "builtin_vector_owner_destroys_once";
  program.source = R"(
import /std/collections/*

[struct]
Noisy() {
  [i32 mut] id{0i32}
  [public effects(io_out)] Destroy() { print_line(this.id) }
}

[effects(io_out heap_alloc) return<vector<Noisy>>]
make([i32] base) {
  [vector<Noisy> mut] made{vector<Noisy>()}
  made.push(Noisy{base})
  return(made)
}

[effects(io_out heap_alloc) return<int>]
main() {
  if(true) {
    [vector<Noisy> mut] got{make(30i32)}
    print_line(got.count())
  }
  print_line(200i32)
  if(true) {
    [vector<Noisy> mut] v{vector<Noisy>()}
    v.push(Noisy{40i32})
    [vector<Noisy>] alias{v}
    print_line(alias.count())
  }
  print_line(201i32)
  [vector<i32> mut] total{vector<i32>()}
  [i32 mut] k{0i32}
  while(k < 50i32) {
    [vector<i32> mut] scratch{vector<i32>(k, k)}
    total.push(scratch.count())
    k = k + 1i32
  }
  print_line(total.count())
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "1\n30\n200\n1\n40\n201\n50\n";
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("unbound temporaries are destroyed once at the end of their expression") {
  program_matrix::ProgramCase program;
  program.name = "unbound_temporaries_destroyed";
  program.source = R"(
import /std/collections/*

[struct]
Noisy() {
  [i32 mut] id{0i32}
  [public] Copy([Reference<Self>] other) { assign(this.id, other.id + 1000i32) }
  [public effects(io_out)] Destroy() { print_line(this.id) }
}

[return<Noisy>]
make([i32] id) {
  return(Noisy{id})
}

[return<i32>]
peek([Noisy] n) {
  return(n.id)
}

[effects(io_out)]
bump([Noisy mut] n) {
  n.id = n.id + 1i32
}

[return<i32>]
keep([Noisy copy] n) {
  return(n.id)
}

[effects(io_out heap_alloc) return<int>]
main() {
  make(1i32)
  print_line(100i32)
  print_line(101i32)
  print_line(peek(make(3i32)))
  print_line(102i32)
  print_line(make(4i32).id)
  print_line(103i32)
  print_line(keep(make(5i32)))
  print_line(104i32)
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "1\n100\n101\n3\n3\n102\n4\n4\n103\n5\n5\n104\n";
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("temporaries passed to mut parameters and copied vectors run each helper once") {
  program_matrix::ProgramCase program;
  program.name = "temporaries_mut_params_vector_copy";
  program.source = R"(
import /std/collections/*

[struct]
Noisy() {
  [i32 mut] id{0i32}
  [public] Copy([Reference<Self>] other) { assign(this.id, other.id + 1000i32) }
  [public effects(io_out)] Destroy() { print_line(this.id) }
}

[effects(io_out)]
bump([Noisy mut] n) {
  n.id = n.id + 1i32
  print_line(n.id)
}

[effects(io_out heap_alloc) return<Vector<Noisy>>]
make_vec() {
  [Vector<Noisy> mut] made{vector<Noisy>()}
  vectorPush<Noisy>(made, Noisy{7i32})
  return(made)
}

[effects(io_out heap_alloc) return<int>]
main() {
  bump(Noisy{1i32})
  print_line(100i32)
  print_line(vectorCount<Noisy>(make_vec()))
  print_line(101i32)
  [Vector<Noisy> mut] src{vector<Noisy>()}
  vectorPush<Noisy>(src, Noisy{3i32})
  print_line(102i32)
  [Vector<Noisy> mut] dup{src}
  print_line(103i32)
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "2\n2\n100\n7\n1\n101\n102\n103\n1003\n3\n";
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("a user Copy helper starts from fields it owns") {
  program_matrix::ProgramCase program;
  program.name = "user_copy_owns_fields";
  program.source = R"(
import /std/collections/*

[struct]
Bag() {
  [Vector<i32> mut] items{vector<i32>()}
  [i32 mut] tag{0i32}

  [public effects(heap_alloc)]
  Copy([Reference<Self>] other) {
    assign(this.items, other.items)
    assign(this.tag, other.tag + 100i32)
  }
}

[effects(io_out heap_alloc) return<int>]
main() {
  [Bag mut] a{Bag{}}
  vectorPush<i32>(a.items, 4i32)
  vectorPush<i32>(a.items, 5i32)
  a.tag = 1i32
  [Bag mut] b{a}
  vectorPush<i32>(b.items, 6i32)
  print_line(vectorCount<i32>(a.items))
  print_line(vectorCount<i32>(b.items))
  print_line(b.tag)
  print_line(vectorAt<i32>(b.items, 2i32))
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "2\n3\n101\n6\n";
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("a user Copy helper that skips owning fields still copies them") {
  program_matrix::ProgramCase program;
  program.name = "user_copy_skipped_fields";
  program.source = R"(
import /std/collections/*

[struct]
Noisy() {
  [i32 mut] id{0i32}
  [public] Copy([Reference<Self>] other) { assign(this.id, other.id + 10i32) }
  [public effects(io_out)] Destroy() { print_line(this.id) }
}

[struct]
Pair() {
  [Noisy mut] left{Noisy{1i32}}
  [Vector<i32> mut] items{vector<i32>()}
  [i32 mut] copies{0i32}

  [public]
  Copy([Reference<Self>] other) {
    assign(this.copies, other.copies + 1i32)
  }
}

[effects(io_out heap_alloc) return<int>]
main() {
  [Pair mut] a{Pair{}}
  vectorPush<i32>(a.items, 7i32)
  [Pair mut] b{a}
  vectorPush<i32>(b.items, 8i32)
  print_line(vectorCount<i32>(a.items))
  print_line(vectorCount<i32>(b.items))
  print_line(b.copies)
  print_line(b.left.id)
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "1\n2\n1\n11\n11\n1\n";
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("assigning a value to itself copies before destroying") {
  program_matrix::ProgramCase program;
  program.name = "self_assignment_copies_first";
  program.source = R"(
import /std/collections/*

[struct]
Noisy() {
  [i32 mut] id{0i32}
  [public] Copy([Reference<Self>] other) { assign(this.id, other.id + 10i32) }
  [public effects(io_out)] Destroy() { print_line(this.id) }
}

[effects(io_out heap_alloc) return<int>]
main() {
  [Noisy mut] x{Noisy{1i32}}
  assign(x, x)
  print_line(x.id)
  [Vector<Noisy> mut] v{vector<Noisy>()}
  vectorPush<Noisy>(v, Noisy{2i32})
  assign(v, v)
  print_line(vectorCount<Noisy>(v))
  print_line(100i32)
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "1\n11\n2\n1\n100\n12\n11\n";
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("fields of a struct with a user Destroy are destroyed after it") {
  program_matrix::ProgramCase program;
  program.name = "user_destroy_then_fields";
  program.source = R"(
import /std/collections/*

[struct]
Noisy() {
  [i32 mut] id{0i32}
  [public] Copy([Reference<Self>] other) { assign(this.id, other.id + 10i32) }
  [public effects(io_out)] Destroy() { print_line(this.id) }
}

[struct]
Bag() {
  [Noisy mut] inner{Noisy{3i32}}
  [Vector<Noisy> mut] items{vector<Noisy>()}
  [public effects(io_out)] Destroy() { print_line(200i32) }
}

[effects(io_out heap_alloc) return<int>]
main() {
  if(true) {
    [Bag mut] b{Bag{}}
    vectorPush<Noisy>(b.items, Noisy{4i32})
  }
  print_line(100i32)
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "200\n4\n3\n100\n";
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("a Maybe payload is copied in and destroyed once") {
  program_matrix::ProgramCase program;
  program.name = "maybe_payload_destroyed_once";
  program.source = R"(
import /std/maybe/*

[struct]
Noisy() {
  [i32 mut] id{0i32}
  [public] Copy([Reference<Self>] other) { assign(this.id, other.id + 10i32) }
  [public effects(io_out)] Destroy() { print_line(this.id) }
}

[effects(io_out) return<int>]
main() {
  if(true) {
    [Maybe<Noisy>] m{some<Noisy>(Noisy{5i32})}
    print_line(100i32)
  }
  print_line(101i32)
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "5\n100\n15\n101\n";
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("sum payloads are destroyed once across returns, loops and pick") {
  program_matrix::ProgramCase program;
  program.name = "sum_payloads_destroyed_once";
  program.source = R"(
import /std/maybe/*
import /std/collections/*

[struct]
Noisy() {
  [i32 mut] id{0i32}
  [public] Copy([Reference<Self>] other) { assign(this.id, other.id + 10i32) }
  [public effects(io_out)] Destroy() { print_line(this.id) }
}

[sum]
Shape {
  [Noisy] circle
  [i32] square
}

[effects(io_out) return<Maybe<Noisy>>]
find([bool] hit) {
  if(hit) {
    return(some<Noisy>(Noisy{20i32}))
  }
  return(none<Noisy>())
}

[effects(io_out) return<i32>]
peek([Maybe<Noisy>] m) {
  return(pick(m) {
    none { -1i32 }
    some(v) { v.id }
  })
}

[effects(io_out heap_alloc) return<int>]
main() {
  if(true) {
    [Maybe<Noisy>] a{find(true)}
    print_line(peek(a))
    [Maybe<Noisy>] b{find(false)}
    print_line(peek(b))
  }
  print_line(100i32)
  [i32 mut] i{0i32}
  while(i < 2i32) {
    [Maybe<Noisy>] c{some<Noisy>(Noisy{40i32 + i})}
    i = i + 1i32
  }
  print_line(101i32)
  if(true) {
    [Shape] s{[circle] Noisy{50i32}}
    [Shape] t{[square] 3i32}
    print_line(102i32)
  }
  print_line(103i32)
  if(true) {
    [Maybe<Vector<i32>>] mv{some<Vector<i32>>(vector<i32>(1i32, 2i32))}
    print_line(pick(mv) { none { 0i32 } some(v) { vectorCount<i32>(v) } })
  }
  print_line(104i32)
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "20\n30\n-1\n30\n100\n40\n50\n41\n51\n101\n102\n50\n103\n2\n104\n";
  program_matrix::runProgramMatrix(program);
}

TEST_CASE("a move parameter is destroyed by the callee on every exit") {
  program_matrix::ProgramCase program;
  program.name = "move_parameter_destroyed_on_every_exit";
  program.source = R"(
[struct]
Noisy() {
  [i32 mut] id{0i32}
  [public] Copy([Reference<Self>] other) { assign(this.id, other.id + 10i32) }
  [public effects(io_out)] Destroy() { print_line(this.id) }
}

[effects(io_out) return<i32>]
consume([Noisy move] n, [bool] early) {
  if(early) {
    return(1i32)
  }
  print_line(50i32)
  return(2i32)
}

[effects(io_out) return<int>]
main() {
  if(true) {
    [Noisy mut] a{Noisy{6i32}}
    print_line(consume(move(a), true))
    print_line(100i32)
  }
  print_line(101i32)
  if(true) {
    [Noisy mut] b{Noisy{7i32}}
    print_line(consume(move(b), false))
    print_line(102i32)
  }
  print_line(103i32)
  return(0i32)
}
)";
  program.exitCode = 0;
  program.stdoutText = "6\n1\n100\n101\n50\n7\n2\n102\n103\n";
  program_matrix::runProgramMatrix(program);
}

TEST_SUITE_END();
