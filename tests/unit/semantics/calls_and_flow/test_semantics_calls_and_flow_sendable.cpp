#include "third_party/doctest.h"

#include "../test_semantics_helpers.h"

// Spawn arguments cross to a task only when their type is sendable (docs/MultithreadingPrototype.md, Sendable
// Values): the check runs on the concrete parameter types of the spawned callee, and a diagnostic names the path
// from the argument to the value that is not sendable.

TEST_SUITE_BEGIN("primestruct.semantics.calls_flow.sendable");

namespace {

void checkAccepted(const std::string &source) {
  std::string error;
  CHECK_MESSAGE(validateProgram(source, "/main", error), error);
}

void checkRejected(const std::string &source, const std::string &message) {
  std::string error;
  CHECK_FALSE(validateProgram(source, "/main", error));
  CHECK_MESSAGE(error.find(message) != std::string::npos, error);
}

const std::string SharingNote = "; sharing between tasks needs Mutex, Atomic or Channel";

} // namespace

TEST_CASE("spawn takes a String, a vector and a struct of both") {
  checkAccepted(R"(
import /std/text/*
import /std/collections/*

[struct]
Job() {
  [String] name{String{}}
  [vector<i32>] values{vector<i32>()}
}

[return<i32>]
measure([String] text) {
  return(text.count())
}

[return<i32> effects(heap_alloc)]
total([vector<i32>] values) {
  return(values.count())
}

[return<i32>]
job_size([Job] job) {
  return(job.name.count())
}

[effects(task, heap_alloc), return<int>]
main() {
  [String] text{stringFrom("abc")}
  [Task<i32>] a{[spawn] measure(text)};
  [vector<i32> mut] values{vector<i32>()}
  values.push(3i32)
  [Task<i32>] b{[spawn] total(values)};
  [Job] job{Job{}}
  [Task<i32>] c{[spawn] job_size(job)};
  [i32] sum{wait(a) + wait(b) + wait(c)}
  return(sum)
}
)");
}

TEST_CASE("spawn takes an enum and a Function") {
  checkAccepted(R"(
[enum]
Mode() {
  fast
  slow
}

[return<i32>]
mode_code([Mode] mode) {
  return(1i32)
}

[return<i32>]
run([Function<i32, i32> mut] step, [i32] value) {
  return(step(value))
}

[return<i32>]
double([i32] x) {
  return(multiply(x, 2i32))
}

[effects(task, heap_alloc), return<int>]
main() {
  [Task<i32>] a{[spawn] mode_code(Mode.fast)};
  [Task<i32>] b{[spawn] run(Function<i32, i32>{/double}, 4i32)};
  [i32] first{wait(a)}
  [i32] second{wait(b)}
  return(plus(first, second))
}
)");
}

TEST_CASE("a mut binding is a spawn argument and the task works on its copy") {
  checkAccepted(R"(
[return<i32>]
bump([i32 copy mut] value) {
  assign(value, plus(value, 1i32))
  return(value)
}

[return<i32>]
echo([i32] value) {
  return(value)
}

[effects(task), return<int>]
main() {
  [i32 mut] counter{1i32}
  assign(counter, plus(counter, 1i32))
  [Task<i32>] left{[spawn] bump(counter)};
  [Task<i32>] right{[spawn] echo(counter)};
  [i32] bumped{wait(left)}
  [i32] echoed{wait(right)}
  return(plus(plus(bumped, echoed), counter))
}
)");
}

TEST_CASE("an unsafe sendable struct crosses to a task") {
  checkAccepted(R"(
[unsafe sendable struct]
PixelRows() {
  [Pointer<u8>] bytes
  [i32] stride{4i32}
}

[return<i32>]
stride_of([PixelRows] rows) {
  return(rows.stride)
}

[effects(task), return<int>]
main() {
  [u8 mut] byte{1u8}
  [PixelRows] rows{PixelRows{[bytes] location(byte)}}
  [Task<i32>] t{[spawn] stride_of(rows)};
  [i32] stride{wait(t)}
  return(stride)
}
)");
}

TEST_CASE("spawn rejects a Pointer argument") {
  checkRejected(R"(
[return<i32>]
fill([Pointer<u8>] data) {
  return(1i32)
}

[effects(task), return<int>]
main() {
  [u8 mut] value{1u8}
  [Pointer<u8>] p{location(value)}
  [Task<i32>] t{[spawn] fill(p)};
  return(wait(t))
}
)",
                "spawn argument `data` of /fill is not sendable: it is Pointer<u8>" + SharingNote);
}

TEST_CASE("spawn rejects a Reference argument") {
  checkRejected(R"(
[return<i32>]
read([Reference<i32>] valueRef) {
  return(dereference(valueRef))
}

[effects(task), return<int>]
main() {
  [i32 mut] value{1i32}
  [Reference<i32>] valueRef{location(value)}
  [Task<i32>] t{[spawn] read(valueRef)};
  return(wait(t))
}
)",
                "spawn argument `valueRef` of /read is not sendable: it is Reference<i32>" + SharingNote);
}

TEST_CASE("spawn rejects a StringView argument") {
  checkRejected(R"(
import /std/text/*
import /std/text/view/*

[return<i32>]
measure([StringView] view) {
  return(view.count())
}

[effects(task, heap_alloc), return<int>]
main() {
  [String] text{stringFrom("abc")}
  [Task<i32>] t{[spawn] measure(text.view())};
  return(wait(t))
}
)",
                "spawn argument `view` of /measure is not sendable: it is StringView" + SharingNote);
}

TEST_CASE("spawn rejects a closure with a ref capture") {
  checkRejected(R"(
[return<i32>]
run<F>([F] step) {
  return(step.call())
}

[effects(task), return<int>]
main() {
  [i32 mut] total{1i32}
  [auto] step{[ref total]() { return(total) }}
  [Task<i32>] t{[spawn] run(step)};
  return(wait(t))
}
)",
                "spawn argument `step` of /run<closure> is not sendable: capture `total` is Reference<i32>");
}

TEST_CASE("spawn rejects a struct with a Reference field") {
  checkRejected(R"(
[struct]
Cache() {
  [Reference<i32>] entries
}

[return<i32>]
size([Cache] cache) {
  return(1i32)
}

[effects(task), return<int>]
main() {
  [i32 mut] value{1i32}
  [Cache] cache{Cache{location(value)}}
  [Task<i32>] t{[spawn] size(cache)};
  return(wait(t))
}
)",
                "spawn argument `cache` of /size is not sendable: field `entries` of Cache is Reference<i32>" +
                    SharingNote);
}

TEST_CASE("spawn names both fields of a nested Pointer") {
  checkRejected(R"(
[struct]
PixelRows() {
  [Pointer<u8>] bytes
  [i32] stride{0i32}
}

[struct]
Job() {
  [i32] id{0i32}
  [PixelRows] rows
}

[return<i32>]
render([Job] job) {
  return(job.id)
}

[effects(task), return<int>]
main() {
  [u8 mut] byte{1u8}
  [Job] job{Job{[id] 1i32, [rows] PixelRows{[bytes] location(byte)}}}
  [Task<i32>] t{[spawn] render(job)};
  return(wait(t))
}
)",
                "spawn argument `job` of /render is not sendable: field `rows` of Job, field `bytes` of PixelRows is "
                "Pointer<u8>" +
                    SharingNote);
}

TEST_CASE("spawn names the instantiation chain of a generic callee") {
  checkRejected(R"(
import /std/collections/*

[return<i32>]
consume<T>([T] value) {
  return(1i32)
}

[effects(task, heap_alloc), return<int>]
relay<T>([T] value) {
  [Task<i32>] t{[spawn] consume(value)};
  return(wait(t))
}

[effects(task, heap_alloc), return<int>]
main() {
  [vector<i32> mut] values{vector<i32>()}
  values.push(1i32)
  [ReadSlice<i32>] view{/std/collections/vector/view<i32>(values)}
  return(relay(view))
}
)",
                "spawn argument `value` of /consume<ReadSlice<i32>> is not sendable: it is ReadSlice<i32> "
                "(instantiated from /relay<ReadSlice<i32>>, instantiated from /main)" +
                    SharingNote);
}

TEST_CASE("spawn rejects a borrowed mut parameter") {
  checkRejected(R"(
[return<i32>]
bump([i32 mut] value) {
  assign(value, plus(value, 1i32))
  return(value)
}

[effects(task), return<int>]
main() {
  [i32 mut] counter{1i32}
  [Task<i32>] t{[spawn] bump(counter)};
  return(wait(t))
}
)",
                "spawn argument `value` of /bump is a mutable borrow of the spawner's value: a task works on "
                "copies, take it as `[i32 copy mut]`");
}

TEST_CASE("sendable on a user struct needs unsafe") {
  checkRejected(R"(
[sendable struct]
PixelRows() {
  [Pointer<u8>] bytes
}

[return<int>]
main() {
  return(0i32)
}
)",
                "sendable transform outside the stdlib requires unsafe on /PixelRows");
}

TEST_CASE("sendable is only valid on structs") {
  checkRejected(R"(
[unsafe sendable return<int>]
main() {
  return(0i32)
}
)",
                "sendable transform is only valid on struct definitions: /main");
}

TEST_CASE("a String made in an open arena cannot be a spawn argument") {
  checkRejected(R"(
import /std/text/*

[return<i32>]
measure([String] text) {
  return(text.count())
}

[effects(task, heap_alloc), return<int>]
main() {
  [StringArena] arena{beginStringArena()}
  [String] made{stringFromI32(7i32)}
  [Task<i32>] t{[spawn] measure(made)};
  [i32] count{wait(t)}
  return(count)
}
)",
                "string escapes its arena via spawn (arena: arena)");
}

TEST_CASE("a spawned method checks its receiver by value") {
  checkAccepted(R"(
[struct]
Counter() {
  [i32 mut] value{1i32}

  [return<i32>]
  current() {
    return(this.value)
  }
}

[effects(task), return<int>]
main() {
  [Counter] counter{Counter{}}
  [Task<i32>] t{[spawn] counter.current()};
  [i32] value{wait(t)}
  return(value)
}
)");
  checkRejected(R"(
[struct]
Counter() {
  [i32 mut] value{1i32}

  [mut return<i32>]
  bump() {
    assign(this.value, plus(this.value, 1i32))
    return(this.value)
  }
}

[effects(task), return<int>]
main() {
  [Counter mut] counter{Counter{}}
  [Task<i32>] t{[spawn] counter.bump()};
  [i32] value{wait(t)}
  return(value)
}
)",
                "spawn receiver of /Counter/bump is a mutable borrow of the spawner's value: a task works on copies");
  checkRejected(R"(
[struct]
Holder() {
  [Reference<i32>] entry

  [return<i32>]
  read() {
    return(1i32)
  }
}

[effects(task), return<int>]
main() {
  [i32 mut] value{1i32}
  [Holder] holder{Holder{location(value)}}
  [Task<i32>] t{[spawn] holder.read()};
  [i32] got{wait(t)}
  return(got)
}
)",
                "spawn receiver of /Holder/read is not sendable: field `entry` of Holder is Reference<i32>" + SharingNote);
}

TEST_SUITE_END();
