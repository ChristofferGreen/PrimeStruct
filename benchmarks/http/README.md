# HTTP server benchmarks

How many requests per second the web servers of `/std/http/server` answer under load, `serve` (one thread)
against `serveWorkers` (one polling thread and worker threads; docs/spec/stdlib-reference.md, Workers). The
recorded numbers and what they mean are in docs/WebAppsPlan.md, Concurrency.

These are measurements made by hand. Nothing in the test gate measures time or throughput; the gate only checks
that the load generator reports failures (`tests/scripts/test_http_load.py`) and that the example apps work.

## Files

- `scripts/http_load.py`: the load generator (Python standard library only). N concurrent clients, each a process,
  send one request per connection for a fixed time and report requests per second and the p50 and p99 latency.
  It exits 1 when a response is not 2xx or 3xx and 2 when a client cannot connect.
- `benchmarks/http/bench_server.prime`: a server with a route whose handler does little (`POST /form`) and a
  route whose handler computes for about 1 ms (`GET /work`). `SERVER=serve` or `SERVER=workers` with `WORKERS=n`.
- `benchmarks/http/run_http_benchmarks.py`: builds the programs, runs every scenario on `serve`, 1 worker and 4
  workers back to back and prints the table.

## Scenarios

| Scenario | Program | What it shows |
| --- | --- | --- |
| List page with 200 items, `GET /` | `examples/apps/todo_web` | A page written inside the handler, so inside the app's mutex |
| 32 KB form, `POST /form` | `bench_server` | Receiving and parsing, which workers do side by side |
| About 1 ms of work, `GET /work` | `bench_server` | A handler that holds the mutex: the baseline for read-only handlers under a read lock |

`serve` for the list page is a copy of the example with `serveWorkers(` replaced by `serve(`.

## Running

```bash
./scripts/compile.sh --release --skip-tests
python3 benchmarks/http/run_http_benchmarks.py --build-dir build-release
```

Run it alone on an idle machine (the table records the load average before each scenario) with at least 6
processors: 4 workers, the polling thread and the 16 client processes share them. The servers listen on
127.0.0.1 on a free port. `--seconds`, `--connections`, `--workers` and `--items` change the run;
`--skip-build` reuses the programs in `build-release/benchmarks/http/`.

One scenario by hand:

```bash
PORT=8080 SERVER=workers WORKERS=4 build-release/benchmarks/http/bench_server &
python3 scripts/http_load.py http://127.0.0.1:8080/form --form-bytes 32768 --connections 16 --seconds 10
kill %1
```

The clients open a new connection for every request, because the servers close each connection after its
response (no keep-alive). The numbers therefore include the cost of connecting, on both sides.
