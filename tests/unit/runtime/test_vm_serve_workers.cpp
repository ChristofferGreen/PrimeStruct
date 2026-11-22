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

// `serveWorkers` and `serve` on the VM with a scripted net (docs/spec/stdlib-reference.md, Workers). The VM has no
// sockets: the `/std/net` and `/std/os` host functions the server calls are answered from a fixed list of connections
// and their request bytes, and the bytes the server sends are recorded, so nothing here opens a port, reads a real
// clock or sleeps. A connection's script is the requests of one client: the next one arrives once the server has
// answered the one before. The clock is a number the `poll` that finds nothing ready advances by its timeout, so an
// idle timeout runs out after a count of polls.
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

constexpr int64_t StartClockMs = 1000;

// What a run serves: the script of each connection and the server's request cap per connection (0: none) and idle
// timeout.
struct Scenario {
  std::vector<std::vector<std::string>> scripts;
  int32_t cap = 0;
  int32_t idleMs = 5000;
  // The requests the handler is to see in all when the poller waits for ticks (see ServerSource); 0 when the
  // connections stay open and the poller spins instead.
  int32_t answers = 0;
};

Scenario oneRequestEach() {
  Scenario scenario;
  for (const std::string &request : Requests) {
    scenario.scripts.push_back({request});
  }
  scenario.answers = static_cast<int32_t>(Requests.size());
  return scenario;
}

constexpr int32_t ListenerFd = 3;
constexpr int32_t WakeReaderFd = 4;
constexpr int32_t WakeWriterFd = 5;
constexpr int32_t FirstConnectionFd = 10;

// The server's whole world: the listener, one connection per request, a wake pipe and a clock that stands still.
// Every host function runs under the VM's host lock (`Serialized`), so the state needs none of its own.
struct ScriptedNet {
  struct Connection {
    std::vector<std::string> script;
    size_t next = 0;
    // The bytes that have arrived so far: the first request, then each next one once a response has been sent.
    std::string request;
    size_t consumed = 0;
    std::string response;
    bool closed = false;
    int64_t lastSendMs = 0;
    int64_t closeMs = 0;
  };

  std::vector<Connection> connections;
  size_t accepted = 0;
  size_t closedCount = 0;
  bool wakePending = false;
  // Whether the last `poll` found nothing ready: the poller has nothing to do until a worker hands something back.
  bool lastPollIdle = false;
  int64_t clockMs = StartClockMs;
  const Scenario &scenario;

  explicit ScriptedNet(const Scenario &scenarioToServe) : scenario(scenarioToServe) {
    for (const std::vector<std::string> &script : scenario.scripts) {
      Connection connection;
      connection.script = script;
      connection.request = script.empty() ? std::string() : script[0];
      connection.next = 1;
      connections.push_back(std::move(connection));
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
  int32_t poll(uint8_t *interest, uint64_t length, int32_t timeoutMs) {
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
    if (lastPollIdle && timeoutMs > 0) {
      clockMs += timeoutMs;
    }
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
    open->lastSendMs = clockMs;
    if (open->next < open->script.size()) {
      open->request += open->script[open->next++];
    }
    return static_cast<int64_t>(length);
  }

  int32_t close(int32_t fd) {
    if (Connection *open = connection(fd)) {
      if (!open->closed) {
        open->closed = true;
        open->closeMs = clockMs;
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
    return slot(net.poll(bytes(args[0]), args[1], static_cast<int32_t>(args[2])));
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
  bind(
      hosts, "ps_os_monotonic_ms", {}, I64, [&net](const uint64_t *) { return slot(net.clockMs); });
  bind(hosts, "ps_os_stop_requested", {}, IrHostValueKind::Bool, [&net](const uint64_t *) {
    return static_cast<uint64_t>(net.done() ? 1 : 0);
  });
  bind(hosts, "ps_os_stop_signal_fd", {}, I32, [](const uint64_t *) { return slot(-1); });
  bind(hosts, "ps_os_cpu_count", {}, I32, [](const uint64_t *) { return slot(2); });
  bind(hosts, "ps_os_block_stop_signals", {}, I32, [](const uint64_t *) { return slot(0); });
  bind(hosts, "ps_os_log_line", {IrHostValueKind::Text}, I32, [](const uint64_t *) {
    return slot(0);
  });
  bind(hosts, "scripted_idle", {}, I32, [&net](const uint64_t *) { return slot(net.lastPollIdle ? 1 : 0); });
  bind(hosts, "scripted_total", {}, I32, [&net](const uint64_t *) {
    return slot(net.scenario.answers);
  });
  bind(hosts, "scripted_cap", {}, I32, [&net](const uint64_t *) { return slot(net.scenario.cap); });
  bind(hosts, "scripted_idle_ms", {}, I32, [&net](const uint64_t *) {
    return slot(net.scenario.idleMs);
  });
  return hosts;
}

// The server: a handler that counts every request in an `Atomic`, and a stop condition that ends the poller once every
// connection is closed. On the VM a host call cannot wait, and the poller never blocks, so with the eager default
// schedule no worker would ever run: when the last `poll` found nothing ready, the stop condition has to wait for
// something a worker does. In the `answers` scenarios (one request on each connection) that is a tick the handler
// sends, and the poller needs nothing more once the worker has answered. Where a connection stays open the poller
// has to take it back and watch it again after the worker has answered, and a tick cannot say that: the send of
// a tick to the waiting poller switches to it at once, before the worker has handed the connection back, and the
// eager schedule runs the poller, the lowest-numbered task, until it blocks. There the stop condition spins on an
// atomic instead, which is a switch point of a seeded schedule and nothing on the eager one, so those scenarios run
// on seeds and task threads only (a host call that yields is the open work that would let the eager schedule
// drive them). A run that never ends because a connection is never closed stops after 100000 rounds and then fails
// its checks.
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

[host return<i32>]
scripted_cap() {
}

[host return<i32>]
scripted_idle_ms() {
}

[effects(io_err)]
log_serve_error([ServeError] err) {
  print_line_error(err.why())
}

[return<int> effects(heap_alloc, io_out, io_err, net_io, task, lock, channel, atomic) on_error<ServeError, log_serve_error>]
main() {
  [Atomic<i32>] hits{Atomic<i32>{0i32}}
  [Atomic<i32>] spins{Atomic<i32>{0i32}}
  [Channel<i32>] ticks{Channel<i32>{[capacity] 16i32}}
  [i32] total{scripted_total()}
  [mut] handler{[hits, ticks, total]([Request] request, [Reference<Response>] response) {
    hits.add(1i32)
    if(total > 0i32) {
      ticks.send(1i32)
    }
    response.body.appendBytes(request.method.data, request.method.count())
    response.body.append(" ")
    response.body.appendBytes(request.target.data, request.target.count())
    response.body.append(" ")
    response.body.appendI64(convert<i64>(request.body.count()))
  }}
  [mut] stop{[ticks, spins, total, mut waited{0i32}, mut rounds{0i32}]() {
    if(stopRequested() || rounds > 100000i32) {
      return(true)
    }
    if(scripted_idle() != 0i32) {
      if(total == 0i32) {
        rounds = rounds + 1i32
        spins.add(1i32)
      } else {
        if(waited < total) {
          pick(ticks.receive()) {
            none {
            }
            some(tick) {
              waited = waited + tick
            }
          }
        }
      }
    }
    return(stopRequested())
  }}
  [ServerConfig] config{ServerConfig{[port] 0i32, [workers] 2i32, [announce] false,
                                     [maxRequestsPerConnection] scripted_cap(), [keepAliveTimeoutMs] scripted_idle_ms()}}
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

// The single-threaded server over the same net.
const char *SerialSource = R"(
import /std/http/*
import /std/http/server/*
import /std/text/*
import /std/text/view/*

[host return<i32>]
scripted_cap() {
}

[host return<i32>]
scripted_idle_ms() {
}

[effects(io_err)]
log_serve_error([ServeError] err) {
  print_line_error(err.why())
}

[return<int> effects(heap_alloc, io_out, io_err, net_io) on_error<ServeError, log_serve_error>]
main() {
  [mut] handler{[]([Request] request, [Reference<Response>] response) {
    response.body.appendBytes(request.method.data, request.method.count())
    response.body.append(" ")
    response.body.appendBytes(request.target.data, request.target.count())
    response.body.append(" ")
    response.body.appendI64(convert<i64>(request.body.count()))
  }}
  [mut] stop{[]() { return(stopRequested()) }}
  [ServerConfig] config{ServerConfig{[port] 0i32, [announce] false,
                                     [maxRequestsPerConnection] scripted_cap(), [keepAliveTimeoutMs] scripted_idle_ms()}}
  serveUntil(handler, config, stop)?
  return(0i32)
}
)";

// An `AppRouter` with `get`, `post` and `view` routes on one counter, served by `serveWorkers` (HANDLER is `router` or
// `withRequestLog(router)`) or by `serve`. "/meet" is a view that signals that it is inside and then waits until
// another view is inside too: a server that ran views one at a time would answer the first "alone". The wait is a
// count of rounds, not a clock, and long enough that the other worker is certain to arrive.
const char *ViewServerTemplate = R"(
import /std/http/*
import /std/http/server/*
import /std/sync/*
import /std/text/*
import /std/text/view/*

[host return<i32>]
scripted_idle() {
}

[host return<i32>]
scripted_cap() {
}

[host return<i32>]
scripted_idle_ms() {
}

[struct]
Counter() {
  [i32 mut] hits{0i32}
}

[effects(io_err)]
log_serve_error([ServeError] err) {
  print_line_error(err.why())
}

[effects(heap_alloc)]
show_get([Reference<Counter>] counter, [Request] request, [Reference<Response>] response) {
  response.body.append("get ")
  response.body.appendI64(convert<i64>(counter.hits))
}

[effects(heap_alloc)]
count_hit([Reference<Counter>] counter, [Request] request, [Reference<Response>] response) {
  counter.hits = counter.hits + 1i32
  response.body.append("ok")
}

[effects(heap_alloc)]
show_view([Counter] counter, [Request] request, [Reference<Response>] response) {
  response.body.append("view ")
  response.body.appendI64(convert<i64>(counter.hits))
}

[return<AppRouter<Counter>> effects(heap_alloc, atomic)]
counter_router() {
  [AppRouter<Counter> mut] router{AppRouter<Counter>{Counter{}}}
  [Atomic<i32>] arrived{Atomic<i32>{0i32}}
  router.get("/count", show_get)
  router.post("/hit", count_hit)
  router.view("/seen", show_view)
  router.view("/meet", [arrived]([Counter] counter, [Request] request, [Reference<Response>] response) {
    arrived.add(1i32)
    [i32 mut] rounds{0i32}
    while(arrived.load() < 2i32 && rounds < 5000000i32) {
      rounds = rounds + 1i32
    }
    if(arrived.load() >= 2i32) {
      response.body.append("met")
    } else {
      response.body.append("alone")
    }
  })
  return(router)
}

[return<int> effects(heap_alloc, io_out, io_err, net_io, task, lock, channel, atomic) on_error<ServeError, log_serve_error>]
main() {
  [AppRouter<Counter> mut] router{counter_router()}
  [Atomic<i32>] spins{Atomic<i32>{0i32}}
  [mut] stop{[spins, mut rounds{0i32}]() {
    if(stopRequested() || rounds > 100000i32) {
      return(true)
    }
    if(scripted_idle() != 0i32) {
      rounds = rounds + 1i32
      spins.add(1i32)
    }
    return(stopRequested())
  }}
  [ServerConfig] config{ServerConfig{[port] 0i32, [workers] 2i32, [announce] false,
                                     [maxRequestsPerConnection] scripted_cap(), [keepAliveTimeoutMs] scripted_idle_ms()}}
  @SERVE@
  return(0i32)
}
)";

std::string viewServer(const std::string &entry) {
  std::string source = ViewServerTemplate;
  source.replace(source.find("@SERVE@"), 7, entry);
  return source;
}

const char *PlainWorkers = "serveWorkersUntil(router, config, stop)?";
const char *LoggedWorkers = "serveWorkersUntil(withRequestLog(router), config, stop)?";
const char *SerialView = "serveUntil(router, config, stop)?";

struct Served {
  bool ok = false;
  std::string error;
  std::string out;
  std::vector<std::string> responses;
  size_t closed = 0;
  // How long each connection stayed open after its last response, on the scripted clock (-1: never closed).
  std::vector<int64_t> idleMs;
};

void appendOutput(int fd, std::string_view chunk, void *userData) {
  if (fd == 1) {
    static_cast<std::string *>(userData)->append(chunk);
  }
}

// Serves `Requests` once: `seed` picks the task schedule (none is the eager default), `threads` runs each task on an
// OS thread instead.
Served serve(const IrModule &module,
             const Scenario &scenario,
             std::optional<uint64_t> seed,
             bool threads,
             bool withClose = true) {
  ScriptedNet net(scenario);
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
    served.idleMs.push_back(connection.closed ? connection.closeMs - connection.lastSendMs : -1);
  }
  served.closed = net.closedCount;
  return served;
}

bool endsWith(const std::string &text, const std::string &suffix) {
  return text.size() >= suffix.size() && text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// The servers compiled once for the whole suite: compiling the HTTP stdlib takes longer than serving.
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

const IrModule &serialModule() {
  static IrModule module;
  static bool compiled = false;
  if (!compiled) {
    std::string error;
    REQUIRE_MESSAGE(compileVm(SerialSource, module, error), error);
    compiled = true;
  }
  return module;
}

// The responses on one connection, one string each (every response starts with the status line).
std::vector<std::string> splitResponses(const std::string &all) {
  std::vector<std::string> responses;
  size_t at = 0;
  while (at < all.size()) {
    size_t next = all.find("HTTP/1.1 ", at + 1);
    if (next == std::string::npos) {
      next = all.size();
    }
    responses.push_back(all.substr(at, next - at));
    at = next;
  }
  return responses;
}

std::string bodyOf(const std::string &response) {
  const size_t at = response.find("\r\n\r\n");
  return at == std::string::npos ? std::string() : response.substr(at + 4);
}

// Whether the response says `Connection: <value>` and its Content-Length is the length of its body.
bool wellFormed(const std::string &response, const std::string &connection) {
  const size_t headEnd = response.find("\r\n\r\n");
  if (headEnd == std::string::npos ||
      response.find("Connection: " + connection + "\r\n") == std::string::npos) {
    return false;
  }
  const std::string head = response.substr(0, headEnd + 2);
  const std::string marker = "Content-Length: ";
  const size_t at = head.find(marker);
  if (at == std::string::npos) {
    return false;
  }
  return std::stoul(head.substr(at + marker.size())) == bodyOf(response).size();
}

std::string get(const std::string &target,
                const std::string &extraHeaders = {},
                const std::string &version = "1.1") {
  return "GET " + target + " HTTP/" + version + "\r\nHost: test\r\n" + extraHeaders + "\r\n";
}

// One client that sends `count` requests, each after the answer to the one before.
Scenario oneClient(int count, int32_t cap, int32_t idleMs) {
  Scenario scenario;
  std::vector<std::string> script;
  for (int i = 0; i < count; ++i) {
    script.push_back(get("/n" + std::to_string(i)));
  }
  scenario.scripts.push_back(script);
  scenario.cap = cap;
  scenario.idleMs = idleMs;
  return scenario;
}

// The connections of the "who closes" scenario: `Connection: close`, HTTP/1.0 alone, HTTP/1.0 asking for
// keep-alive and then closing, two requests in one read, and a client that outlasts the cap of 3.
Scenario closingClients() {
  Scenario scenario;
  scenario.scripts.push_back({get("/a", "Connection: close\r\n")});
  scenario.scripts.push_back({get("/b", "", "1.0")});
  scenario.scripts.push_back(
      {get("/c", "Connection: Keep-Alive\r\n", "1.0"), get("/d", "Connection: close\r\n")});
  scenario.scripts.push_back({get("/e") + get("/f")});
  scenario.scripts.push_back({get("/g"), get("/h"), get("/i"), get("/j"), get("/k")});
  scenario.cap = 3;
  scenario.idleMs = 5000;
  return scenario;
}

void checkClosingClients(const Served &served, const std::string &label) {
  REQUIRE_MESSAGE(served.ok, served.error);
  REQUIRE_MESSAGE(served.responses.size() == 5, label);
  CHECK_MESSAGE(served.closed == 5, label);
  const std::vector<std::vector<std::pair<std::string, std::string>>> expected = {
      {{"close", "GET /a 0"}},
      {{"close", "GET /b 0"}},
      {{"keep-alive", "GET /c 0"}, {"close", "GET /d 0"}},
      {{"close", "GET /e 0"}},
      {{"keep-alive", "GET /g 0"}, {"keep-alive", "GET /h 0"}, {"close", "GET /i 0"}},
  };
  for (size_t i = 0; i < expected.size(); ++i) {
    const std::vector<std::string> responses = splitResponses(served.responses[i]);
    REQUIRE_MESSAGE(responses.size() == expected[i].size(),
                    label << " connection " << i << ": " << served.responses[i]);
    for (size_t at = 0; at < responses.size(); ++at) {
      CHECK_MESSAGE(wellFormed(responses[at], expected[i][at].first),
                    label << " connection " << i << ": " << responses[at]);
      CHECK_MESSAGE(bodyOf(responses[at]) == expected[i][at].second, label << " connection " << i);
    }
  }
}

// One client reusing a connection: every request is answered `keep-alive` with a right Content-Length, and the
// connection closes only after it has been idle for the whole timeout.
void checkReused(const Served &served, int count, int32_t idleMs, const std::string &label) {
  REQUIRE_MESSAGE(served.ok, served.error);
  REQUIRE_MESSAGE(served.responses.size() == 1, label);
  const std::vector<std::string> responses = splitResponses(served.responses[0]);
  REQUIRE_MESSAGE(responses.size() == static_cast<size_t>(count),
                  label << ": " << responses.size());
  for (int i = 0; i < count; ++i) {
    CHECK_MESSAGE(wellFormed(responses[i], "keep-alive"),
                  label << " " << i << ": " << responses[i]);
    CHECK_MESSAGE(bodyOf(responses[i]) == "GET /n" + std::to_string(i) + " 0", label << " " << i);
  }
  CHECK_MESSAGE(served.closed == 1, label);
  CHECK_MESSAGE(served.idleMs[0] >= idleMs,
                label << ": closed after " << served.idleMs[0] << " ms");
}

// The view servers compiled once each for the whole suite.
const IrModule &viewModule(const std::string &entry) {
  static std::map<std::string, IrModule> modules;
  auto found = modules.find(entry);
  if (found == modules.end()) {
    IrModule module;
    std::string error;
    REQUIRE_MESSAGE(compileVm(viewServer(entry), module, error), error);
    found = modules.emplace(entry, std::move(module)).first;
  }
  return found->second;
}

std::string post(const std::string &target) {
  return "POST " + target + " HTTP/1.1\r\nHost: test\r\nContent-Length: 0\r\n\r\n";
}

// One client that sends a GET and a POST in an order that shows each answer: the answer to a request reads the
// counter every earlier request changed.
Scenario countingClient() {
  Scenario scenario;
  scenario.scripts.push_back({get("/seen"),
                              get("/count"),
                              post("/hit"),
                              get("/count"),
                              get("/seen"),
                              post("/seen"),
                              post("/hit"),
                              get("/nowhere"),
                              get("/seen")});
  scenario.cap = 0;
  scenario.idleMs = 3000;
  return scenario;
}

// Two clients whose only request is a view that waits for the other one.
Scenario twoMeetings() {
  Scenario scenario;
  scenario.scripts.push_back({get("/meet")});
  scenario.scripts.push_back({get("/meet")});
  scenario.cap = 0;
  scenario.idleMs = 3000;
  return scenario;
}

void checkCountingClient(const Served &served, const std::string &label) {
  REQUIRE_MESSAGE(served.ok, served.error);
  REQUIRE_MESSAGE(served.responses.size() == 1, label);
  const std::vector<std::string> responses = splitResponses(served.responses[0]);
  const std::vector<std::pair<std::string, std::string>> expected = {
      {"200 OK", "view 0"},
      {"200 OK", "get 0"},
      {"200 OK", "ok"},
      {"200 OK", "get 1"},
      {"200 OK", "view 1"},
      {"405 Method Not Allowed", ""},
      {"200 OK", "ok"},
      {"404 Not Found", ""},
      {"200 OK", "view 2"},
  };
  REQUIRE_MESSAGE(responses.size() == expected.size(), label << ": " << served.responses[0]);
  for (size_t i = 0; i < expected.size(); ++i) {
    CHECK_MESSAGE(responses[i].rfind("HTTP/1.1 " + expected[i].first + "\r\n", 0) == 0,
                  label << " " << i << ": " << responses[i]);
    CHECK_MESSAGE(bodyOf(responses[i]) == expected[i].second,
                  label << " " << i << ": " << responses[i]);
  }
  CHECK_MESSAGE(served.closed == 1, label);
}

void checkMeetings(const Served &served, const std::string &label) {
  REQUIRE_MESSAGE(served.ok, served.error);
  REQUIRE_MESSAGE(served.responses.size() == 2, label);
  for (const std::string &response : served.responses) {
    CHECK_MESSAGE(bodyOf(response) == "met", label << ": " << response);
  }
}

} // namespace

TEST_CASE("serveWorkers with 2 workers answers a fixed set of requests on the eager schedule") {
  const IrModule &module = serverModule();
  const Served served = serve(module, oneRequestEach(), std::nullopt, false);
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
  const Served eager = serve(module, oneRequestEach(), std::nullopt, false);
  REQUIRE_MESSAGE(eager.ok, eager.error);
  for (uint64_t seed = 1; seed <= 3; ++seed) {
    const Served seeded = serve(module, oneRequestEach(), seed, false);
    REQUIRE_MESSAGE(seeded.ok, seeded.error);
    CHECK_MESSAGE(seeded.out == eager.out, "seed " << seed);
    CHECK_MESSAGE(seeded.responses == eager.responses, "seed " << seed);
    CHECK_MESSAGE(seeded.closed == eager.closed, "seed " << seed);
  }
}

TEST_CASE("serveWorkers on the VM gives the same responses and counter on task threads") {
  const IrModule &module = serverModule();
  const Served eager = serve(module, oneRequestEach(), std::nullopt, false);
  REQUIRE_MESSAGE(eager.ok, eager.error);
  for (int run = 0; run < ThreadRuns; ++run) {
    const Served threaded = serve(module, oneRequestEach(), std::nullopt, true);
    REQUIRE_MESSAGE(threaded.ok, threaded.error);
    CHECK(threaded.out == eager.out);
    CHECK(threaded.responses == eager.responses);
    CHECK(threaded.closed == eager.closed);
  }
}

TEST_CASE("a /std/net function the VM does not answer is reported by name") {
  const IrModule &module = serverModule();
  const Served missing = serve(module, oneRequestEach(), std::nullopt, false, false);
  CHECK_FALSE(missing.ok);
  CHECK_MESSAGE(missing.error.find("unbound host function: net_close") != std::string::npos, missing.error);
  CHECK(missing.out.empty());
}

// The connections of these scenarios stay open, so they run on seeded schedules and task threads (see ServerSource).
TEST_CASE(
    "serveWorkers keeps one connection for 100 requests and closes it after the idle timeout") {
  const IrModule &module = serverModule();
  const Scenario scenario = oneClient(100, 0, 3000);
  const Served first = serve(module, scenario, 1, false);
  checkReused(first, 100, 3000, "seed 1");
  CHECK(first.out == "100\n");
  for (uint64_t seed = 2; seed <= 3; ++seed) {
    const Served seeded = serve(module, scenario, seed, false);
    checkReused(seeded, 100, 3000, "seed " + std::to_string(seed));
    CHECK_MESSAGE(seeded.responses == first.responses, "seed " << seed);
  }
  for (int run = 0; run < ThreadRuns; ++run) {
    const Served threaded = serve(module, scenario, std::nullopt, true);
    checkReused(threaded, 100, 3000, "threads");
    CHECK(threaded.responses == first.responses);
  }
}

TEST_CASE(
    "serveWorkers closes on Connection: close, HTTP/1.0, a pipelined request and the request cap") {
  const IrModule &module = serverModule();
  const Scenario scenario = closingClients();
  for (uint64_t seed = 1; seed <= 3; ++seed) {
    const Served seeded = serve(module, scenario, seed, false);
    checkClosingClients(seeded, "seed " + std::to_string(seed));
    CHECK_MESSAGE(seeded.out == "8\n", "seed " << seed);
  }
  for (int run = 0; run < ThreadRuns; ++run) {
    checkClosingClients(serve(module, scenario, std::nullopt, true), "threads");
  }
}

TEST_CASE(
    "serveWorkers closes an idle connection only after keepAliveTimeoutMs on the scripted clock") {
  const IrModule &module = serverModule();
  for (const int32_t idleMs : {700, 2000, 4500}) {
    const Scenario scenario = oneClient(2, 0, idleMs);
    const Served seeded = serve(module, scenario, 1, false);
    checkReused(seeded, 2, idleMs, "idle " + std::to_string(idleMs));
    const Served threaded = serve(module, scenario, std::nullopt, true);
    checkReused(threaded, 2, idleMs, "threads idle " + std::to_string(idleMs));
  }
}

TEST_CASE("serve keeps one connection for 100 requests and closes it after the idle timeout") {
  const IrModule &module = serialModule();
  const Scenario scenario = oneClient(100, 0, 3000);
  checkReused(serve(module, scenario, std::nullopt, false), 100, 3000, "serve");
  const Scenario brief = oneClient(2, 0, 800);
  checkReused(serve(module, brief, std::nullopt, false), 2, 800, "serve idle");
}

TEST_CASE("serve closes on Connection: close, HTTP/1.0, a pipelined request and the request cap") {
  const IrModule &module = serialModule();
  checkClosingClients(serve(module, closingClients(), std::nullopt, false), "serve");
}

// The views of an `AppRouter` (docs/spec/stdlib-reference.md, /std/http/server). The connection stays open, so these
// run on seeded schedules and task threads.
TEST_CASE("serveWorkers answers a view and a get, and a get after a post sees the change") {
  for (const char *entry : {PlainWorkers, LoggedWorkers}) {
    const IrModule &module = viewModule(entry);
    for (uint64_t seed = 1; seed <= 3; ++seed) {
      checkCountingClient(serve(module, countingClient(), seed, false),
                          std::string(entry) + " seed " + std::to_string(seed));
    }
    for (int run = 0; run < ThreadRuns; ++run) {
      checkCountingClient(serve(module, countingClient(), std::nullopt, true),
                          std::string(entry) + " threads");
    }
  }
}

TEST_CASE("serve answers a view and a get, and a get after a post sees the change") {
  const IrModule &module = viewModule(SerialView);
  checkCountingClient(serve(module, countingClient(), std::nullopt, false), "serve");
}

TEST_CASE("two views are inside the read lock together, with and without RequestLog") {
  for (const char *entry : {PlainWorkers, LoggedWorkers}) {
    const IrModule &module = viewModule(entry);
    for (uint64_t seed = 1; seed <= 3; ++seed) {
      checkMeetings(serve(module, twoMeetings(), seed, false),
                    std::string(entry) + " seed " + std::to_string(seed));
    }
    for (int run = 0; run < ThreadRuns; ++run) {
      checkMeetings(serve(module, twoMeetings(), std::nullopt, true),
                    std::string(entry) + " threads");
    }
  }
}

TEST_SUITE_END();
