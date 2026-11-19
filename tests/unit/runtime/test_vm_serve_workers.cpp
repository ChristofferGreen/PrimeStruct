#include "primec/ir/Ir.h"
#include "primec/ir/IrPreparation.h"
#include "primec/pipeline/CompilePipeline.h"
#include "primec/runtime/Vm.h"
#include "primec/runtime/VmHost.h"
#include "primec/support/Options.h"

#include "third_party/doctest.h"

#include <cstdint>
#include <cstring>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace primec;

// `serveWorkers` on the VM with a scripted net (docs/spec/stdlib-reference.md, Workers). The VM has no sockets: the
// `/std/net` and `/std/os` host functions the server calls are answered from a fixed list of connections and their
// request bytes, and the bytes the server sends are recorded, so nothing here opens a port, reads a clock or sleeps.
// The poller and the two workers run on the VM's task scheduler (the eager default, seeds 1 to 3 and, repeated, task
// threads), and every run must give the same responses and the same counter.
TEST_SUITE_BEGIN("primestruct.runtime.vm_serve_workers");

namespace {

constexpr int ThreadRuns = 5;

// What the server serves: GET and POST requests that each arrive in one read.
const std::vector<std::string> Requests = {
    "GET /alpha HTTP/1.1\r\nHost: test\r\n\r\n",
    "GET /beta?x=1 HTTP/1.1\r\nHost: test\r\n\r\n",
    "POST /gamma HTTP/1.1\r\nHost: test\r\nContent-Length: 3\r\n\r\nabc",
    "GET /delta HTTP/1.1\r\nHost: test\r\n\r\n",
    "GET /epsilon HTTP/1.1\r\nHost: test\r\n\r\n",
};

constexpr int32_t ListenerFd = 3;
constexpr int32_t WakeReaderFd = 4;
constexpr int32_t WakeWriterFd = 5;
constexpr int32_t FirstConnectionFd = 10;

// The server's whole world: the listener, one connection per request, a wake pipe and a clock that stands still.
// Every host function runs under the VM's host lock (`Serialized`), so the state needs none of its own.
struct ScriptedNet {
  struct Connection {
    std::string request;
    size_t consumed = 0;
    std::string response;
    bool closed = false;
  };

  std::vector<Connection> connections;
  size_t accepted = 0;
  size_t closedCount = 0;
  bool wakePending = false;
  // Whether the last `poll` found nothing ready: the poller has nothing to do until a worker hands something back.
  bool lastPollIdle = false;

  explicit ScriptedNet(const std::vector<std::string> &requests) {
    for (const std::string &request : requests) {
      connections.push_back({request, 0, {}, false});
    }
  }

  Connection *connection(int32_t fd) {
    const int64_t index = static_cast<int64_t>(fd) - FirstConnectionFd;
    if (index < 0 || static_cast<size_t>(index) >= accepted) {
      return nullptr;
    }
    return &connections[static_cast<size_t>(index)];
  }

  int32_t accept() {
    if (accepted >= connections.size()) {
      return -1;
    }
    return FirstConnectionFd + static_cast<int32_t>(accepted++);
  }

  // Marks the watched descriptors of `interest` (one byte each) 2 when ready and 1 when not; returns how many are
  // ready, or -1 (timeoutCode) when none.
  int32_t poll(uint8_t *interest, uint64_t length) {
    int32_t ready = 0;
    for (uint64_t fd = 0; fd < length; ++fd) {
      if (interest[fd] == 0) {
        continue;
      }
      bool isReady = false;
      if (static_cast<int32_t>(fd) == ListenerFd) {
        isReady = accepted < connections.size();
      } else if (static_cast<int32_t>(fd) == WakeReaderFd) {
        isReady = wakePending;
      } else if (Connection *open = connection(static_cast<int32_t>(fd))) {
        isReady = !open->closed && open->consumed < open->request.size();
      }
      interest[fd] = isReady ? 2 : 1;
      ready += isReady ? 1 : 0;
    }
    lastPollIdle = ready == 0;
    return ready == 0 ? -1 : ready;
  }

  int64_t recv(int32_t fd, uint8_t *buffer, uint64_t length) {
    Connection *open = connection(fd);
    if (open == nullptr || open->closed) {
      return -2;
    }
    const size_t left = open->request.size() - open->consumed;
    const size_t count = left < length ? left : static_cast<size_t>(length);
    std::memcpy(buffer, open->request.data() + open->consumed, count);
    open->consumed += count;
    return static_cast<int64_t>(count);
  }

  int64_t send(int32_t fd, const uint8_t *data, uint64_t length) {
    Connection *open = connection(fd);
    if (open == nullptr || open->closed) {
      return -2;
    }
    open->response.append(reinterpret_cast<const char *>(data), static_cast<size_t>(length));
    return static_cast<int64_t>(length);
  }

  int32_t close(int32_t fd) {
    if (Connection *open = connection(fd)) {
      if (!open->closed) {
        open->closed = true;
        ++closedCount;
      }
    }
    return 0;
  }

  bool done() const { return closedCount == connections.size(); }
};

template <class F>
void bind(VmHostFunctions &hosts,
          const std::string &name,
          std::vector<IrHostValueKind> parameters,
          IrHostValueKind returnKind,
          F body) {
  VmHostBinding binding;
  binding.parameters = std::move(parameters);
  binding.returnKind = returnKind;
  binding.invoke = [body = std::move(body)](const uint64_t *args, uint64_t &result, std::string &) mutable {
    result = body(args);
    return true;
  };
  hosts.bind(name, std::move(binding));
}

uint64_t slot(int64_t value) { return static_cast<uint64_t>(value); }

uint8_t *bytes(uint64_t argument) { return reinterpret_cast<uint8_t *>(static_cast<uintptr_t>(argument)); }

constexpr IrHostValueKind I32 = IrHostValueKind::I32;
constexpr IrHostValueKind I64 = IrHostValueKind::I64;
constexpr IrHostValueKind U64 = IrHostValueKind::U64;
constexpr IrHostValueKind Buffer = IrHostValueKind::BytePointer;

// The host functions of /std/net and /std/os that the server calls, over `net`.
VmHostFunctions scriptedHosts(ScriptedNet &net, bool withClose = true) {
  VmHostFunctions hosts;
  bind(hosts, "net_listen", {I32, I32, I32}, I32, [](const uint64_t *) { return slot(ListenerFd); });
  bind(hosts, "net_port", {I32}, I32, [](const uint64_t *) { return slot(8080); });
  bind(hosts, "net_accept", {I32, I32}, I32, [&net](const uint64_t *) { return slot(net.accept()); });
  bind(hosts, "net_recv", {I32, Buffer, U64, I32}, I64, [&net](const uint64_t *args) {
    return slot(net.recv(static_cast<int32_t>(args[0]), bytes(args[1]), args[2]));
  });
  bind(hosts, "net_send", {I32, Buffer, U64, I32}, I64, [&net](const uint64_t *args) {
    return slot(net.send(static_cast<int32_t>(args[0]), bytes(args[1]), args[2]));
  });
  bind(hosts, "net_poll", {Buffer, U64, I32}, I32, [&net](const uint64_t *args) {
    return slot(net.poll(bytes(args[0]), args[1]));
  });
  if (withClose) {
    bind(hosts, "net_close", {I32}, I32, [&net](const uint64_t *args) {
      return slot(net.close(static_cast<int32_t>(args[0])));
    });
  }
  bind(hosts, "net_wake_open", {}, I64, [](const uint64_t *) {
    return slot((static_cast<int64_t>(WakeReaderFd) << 32) | WakeWriterFd);
  });
  bind(hosts, "net_wake", {I32}, I32, [&net](const uint64_t *) {
    net.wakePending = true;
    return slot(0);
  });
  bind(hosts, "net_wake_drain", {I32}, I32, [&net](const uint64_t *) {
    net.wakePending = false;
    return slot(0);
  });
  bind(hosts, "ps_os_monotonic_ms", {}, I64, [](const uint64_t *) { return slot(1000); });
  bind(hosts, "ps_os_stop_requested", {}, IrHostValueKind::Bool, [&net](const uint64_t *) {
    return static_cast<uint64_t>(net.done() ? 1 : 0);
  });
  bind(hosts, "ps_os_stop_signal_fd", {}, I32, [](const uint64_t *) { return slot(-1); });
  bind(hosts, "ps_os_cpu_count", {}, I32, [](const uint64_t *) { return slot(2); });
  bind(hosts, "ps_os_block_stop_signals", {}, I32, [](const uint64_t *) { return slot(0); });
  bind(hosts, "scripted_idle", {}, I32, [&net](const uint64_t *) { return slot(net.lastPollIdle ? 1 : 0); });
  bind(hosts, "scripted_total", {}, I32, [&net](const uint64_t *) {
    return slot(static_cast<int64_t>(net.connections.size()));
  });
  return hosts;
}

// The server: a handler that counts every request in an `Atomic` and tells `ticks` it ran, and a stop condition that
// ends the poller once every connection is closed. On the VM a host call cannot wait, and the poller never blocks, so
// with the eager default schedule no worker would ever run: when the last `poll` found nothing ready, the stop
// condition waits for a tick, which is a wait the scheduler can hand to a worker.
const char *ServerSource = R"(
import /std/http/*
import /std/http/server/*
import /std/sync/*
import /std/text/*
import /std/text/view/*

[host return<i32>]
scripted_idle() {
}

[host return<i32>]
scripted_total() {
}

[effects(io_err)]
log_serve_error([ServeError] err) {
  print_line_error(err.why())
}

[return<int> effects(heap_alloc, io_out, io_err, net_io, task, lock, channel, atomic) on_error<ServeError, log_serve_error>]
main() {
  [Atomic<i32>] hits{Atomic<i32>{0i32}}
  [Channel<i32>] ticks{Channel<i32>{[capacity] 16i32}}
  [mut] handler{[hits, ticks]([Request] request, [Reference<Response>] response) {
    hits.add(1i32)
    ticks.send(1i32)
    response.body.appendBytes(request.method.data, request.method.count())
    response.body.append(" ")
    response.body.appendBytes(request.target.data, request.target.count())
    response.body.append(" ")
    response.body.appendI64(convert<i64>(request.body.count()))
  }}
  [i32] total{scripted_total()}
  [mut] stop{[ticks, total, mut waited{0i32}]() {
    if(stopRequested()) {
      return(true)
    }
    if(scripted_idle() != 0i32 && waited < total) {
      pick(ticks.receive()) {
        none {
        }
        some(tick) {
          waited = waited + tick
        }
      }
    }
    return(stopRequested())
  }}
  [ServerConfig] config{ServerConfig{[port] 0i32, [workers] 2i32, [announce] false}}
  serveWorkersUntil(handler, config, stop)?
  print_line(hits.load())
  return(0i32)
}
)";

bool compileVm(const std::string &source, IrModule &module, std::string &error) {
  Options options;
  options.emitKind = "vm";
  options.inputPath = std::string(PRIMESTRUCT_SOURCE_DIR) + "/tests/unit/runtime/serve_workers_test.prime";
  options.entryPath = "/main";
  options.inMemorySource = source;
  options.importPaths = {std::string(PRIMESTRUCT_SOURCE_DIR) + "/stdlib"};
  CompilePipelineOutput output;
  CompilePipelineErrorStage stage = CompilePipelineErrorStage::None;
  if (!runCompilePipeline(options, output, stage, error)) {
    return false;
  }
  IrPreparationFailure failure;
  const SemanticProgram *semanticProgram = output.hasSemanticProgram ? &output.semanticProgram : nullptr;
  if (!prepareIrModule(output.program, semanticProgram, options, IrValidationTarget::Vm, module, failure,
                       &output.expandedSource)) {
    error = failure.message;
    return false;
  }
  return true;
}

struct Served {
  bool ok = false;
  std::string error;
  std::string out;
  std::vector<std::string> responses;
  size_t closed = 0;
};

void appendOutput(int fd, std::string_view chunk, void *userData) {
  if (fd == 1) {
    static_cast<std::string *>(userData)->append(chunk);
  }
}

// Serves `Requests` once: `seed` picks the task schedule (none is the eager default), `threads` runs each task on an
// OS thread instead.
Served serve(const IrModule &module, std::optional<uint64_t> seed, bool threads, bool withClose = true) {
  ScriptedNet net(Requests);
  const VmHostFunctions hosts = scriptedHosts(net, withClose);
  Served served;
  Vm vm;
  vm.setOutputSink({&appendOutput, &served.out});
  vm.setScheduleSeed(seed);
  vm.setTaskThreads(threads);
  uint64_t raw = 0;
  served.ok = vm.execute(module, raw, served.error, std::vector<std::string_view>{"prog"}, hosts);
  for (const ScriptedNet::Connection &connection : net.connections) {
    served.responses.push_back(connection.response);
  }
  served.closed = net.closedCount;
  return served;
}

bool endsWith(const std::string &text, const std::string &suffix) {
  return text.size() >= suffix.size() && text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// The server compiled once for the whole suite: compiling the HTTP stdlib takes longer than serving.
const IrModule &serverModule() {
  static IrModule module;
  static bool compiled = false;
  if (!compiled) {
    std::string error;
    REQUIRE_MESSAGE(compileVm(ServerSource, module, error), error);
    compiled = true;
  }
  return module;
}

} // namespace

TEST_CASE("serveWorkers with 2 workers answers a fixed set of requests on the eager schedule") {
  const IrModule &module = serverModule();
  const Served served = serve(module, std::nullopt, false);
  REQUIRE_MESSAGE(served.ok, served.error);
  CHECK(served.out == "5\n");
  CHECK(served.closed == Requests.size());
  const std::vector<std::string> bodies = {"GET /alpha 0", "GET /beta?x=1 0", "POST /gamma 3", "GET /delta 0",
                                           "GET /epsilon 0"};
  REQUIRE(served.responses.size() == bodies.size());
  for (size_t i = 0; i < bodies.size(); ++i) {
    CHECK_MESSAGE(served.responses[i].rfind("HTTP/1.1 200 OK\r\n", 0) == 0, served.responses[i]);
    CHECK_MESSAGE(endsWith(served.responses[i], bodies[i]), served.responses[i]);
  }
}

TEST_CASE("serveWorkers on the VM gives the same responses and counter on schedule seeds 1 to 3") {
  const IrModule &module = serverModule();
  const Served eager = serve(module, std::nullopt, false);
  REQUIRE_MESSAGE(eager.ok, eager.error);
  for (uint64_t seed = 1; seed <= 3; ++seed) {
    const Served seeded = serve(module, seed, false);
    REQUIRE_MESSAGE(seeded.ok, seeded.error);
    CHECK_MESSAGE(seeded.out == eager.out, "seed " << seed);
    CHECK_MESSAGE(seeded.responses == eager.responses, "seed " << seed);
    CHECK_MESSAGE(seeded.closed == eager.closed, "seed " << seed);
  }
}

TEST_CASE("serveWorkers on the VM gives the same responses and counter on task threads") {
  const IrModule &module = serverModule();
  const Served eager = serve(module, std::nullopt, false);
  REQUIRE_MESSAGE(eager.ok, eager.error);
  for (int run = 0; run < ThreadRuns; ++run) {
    const Served threaded = serve(module, std::nullopt, true);
    REQUIRE_MESSAGE(threaded.ok, threaded.error);
    CHECK(threaded.out == eager.out);
    CHECK(threaded.responses == eager.responses);
    CHECK(threaded.closed == eager.closed);
  }
}

TEST_CASE("a /std/net function the VM does not answer is reported by name") {
  const IrModule &module = serverModule();
  const Served missing = serve(module, std::nullopt, false, false);
  CHECK_FALSE(missing.ok);
  CHECK_MESSAGE(missing.error.find("unbound host function: net_close") != std::string::npos, missing.error);
  CHECK(missing.out.empty());
}

TEST_SUITE_END();
