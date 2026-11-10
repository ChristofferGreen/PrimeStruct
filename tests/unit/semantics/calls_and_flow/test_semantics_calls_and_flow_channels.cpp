#include "third_party/doctest.h"

#include "../test_semantics_helpers.h"

// `/std/sync` `Channel<T>` (docs/spec/stdlib-reference.md, std/sync): `send`, `receive` and `close` need the
// `channel` effect, the element must be sendable, a String made in an open arena cannot be sent, and a `Channel`
// crosses to a task.

TEST_SUITE_BEGIN("primestruct.semantics.calls_flow.channels");

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

} // namespace

TEST_CASE("a producer and a consumer task share a channel with the channel effect") {
  checkAccepted(R"(
import /std/sync/*

[effects(channel)]
produce([Channel<i64>] jobs, [i64] count) {
  [i64 mut] i{0i64}
  while(i < count) {
    jobs.send(i)
    i = i + 1i64
  }
  jobs.close()
}

[effects(channel), return<i64>]
sum_jobs([Channel<i64>] jobs) {
  [i64 mut] sum{0i64}
  [bool mut] open{true}
  while(open) {
    pick(jobs.receive()) {
      none {
        open = false
      }
      some(job) {
        sum = sum + job
      }
    }
  }
  return(sum)
}

[effects(task, channel, heap_alloc), return<int>]
main() {
  jobs{Channel<i64>{[capacity] 4i32}}
  [Task<void>] producer{[spawn] produce(jobs, 100i64)}
  [Task<i64>] consumer{[spawn] sum_jobs(jobs)}
  wait(producer)
  [i64] sum{wait(consumer)}
  return(0i32)
}
)");
}

TEST_CASE("send, receive and close without the channel effect are rejected") {
  checkRejected(R"(
import /std/sync/*

[effects(heap_alloc), return<int>]
main() {
  jobs{Channel<i64>{[capacity] 2i32}}
  jobs.send(1i64)
  return(0i32)
}
)",
                "channel operation requires channel effect: /std/sync/Channel");
  checkRejected(R"(
import /std/sync/*

[effects(heap_alloc), return<int>]
main() {
  jobs{Channel<i64>{[capacity] 2i32}}
  [Maybe<i64>] job{jobs.receive()}
  return(0i32)
}
)",
                "/receive in /main");
  checkRejected(R"(
import /std/sync/*

[effects(heap_alloc), return<int>]
main() {
  jobs{Channel<i64>{[capacity] 2i32}}
  jobs.close()
  return(0i32)
}
)",
                "/close in /main");
}

TEST_CASE("a Channel of a non-sendable builtin is rejected") {
  checkRejected(R"(
import /std/sync/*

[effects(heap_alloc), return<int>]
main() {
  jobs{Channel<Pointer<u8>>{[capacity] 2i32}}
  return(0i32)
}
)",
                "Channel element Pointer<u8> is not sendable");
}

TEST_CASE("a String made in an open arena cannot be sent") {
  const std::string source = R"(
import /std/sync/*
import /std/text/*

[effects(channel, heap_alloc), return<int>]
main() {
  CHANNEL
  [StringArena] arena{beginStringArena()}
  [String] made{stringFromI32(7i32)}
  lines.send(SENT)
  return(0i32)
}
)";
  const auto with = [&](const std::string &channel, const std::string &sent) {
    std::string text = source;
    text.replace(text.find("CHANNEL"), 7, channel);
    text.replace(text.find("SENT"), 4, sent);
    return text;
  };
  const std::string message = "string escapes its arena via channel (arena: arena)";
  checkRejected(with("lines{Channel<String>{[capacity] 2i32}}", "made"), message);
  checkRejected(with("[Channel<String>] lines{Channel<String>{[capacity] 2i32}}", "move(made)"),
                message);
  // A copy of a channel is the same channel.
  checkRejected(with("first{Channel<String>{[capacity] 2i32}}\n  lines{first}", "made"), message);
}

TEST_CASE("a String made before the arena opens is sent") {
  checkAccepted(R"(
import /std/sync/*
import /std/text/*

[effects(channel, heap_alloc), return<int>]
main() {
  lines{Channel<String>{[capacity] 2i32}}
  [String] made{stringFromI32(7i32)}
  [StringArena] arena{beginStringArena()}
  lines.send(made)
  return(0i32)
}
)");
}

TEST_CASE(
    "a spawned callee that declares a scalar return and returns from a closure is a Task of it") {
  checkAccepted(R"(
import /std/sync/*

[effects(lock), return<i32>]
read([Mutex<i32>] cell) {
  return(cell.locked([]([Reference<i32>] value) { return(value + 0i32) }))
}

[effects(task, lock, heap_alloc), return<int>]
main() {
  cell{Mutex<i32>{3i32}}
  [Task<i32>] reader{[spawn] read(cell)}
  return(wait(reader))
}
)");
}

TEST_SUITE_END();
