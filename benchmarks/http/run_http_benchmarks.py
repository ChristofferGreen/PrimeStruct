#!/usr/bin/env python3
"""Measures the web servers under load: `serve` against `serveWorkers` (benchmarks/http/README.md).

    python3 benchmarks/http/run_http_benchmarks.py --build-dir build-release

Builds examples/apps/todo_web, a copy of it that calls `serve`, and benchmarks/http/bench_server.prime with
`primec --emit=optexe`, then runs scripts/http_load.py against each scenario on `serve`, 1 worker and --workers
workers, back to back, and prints a Markdown table with the load average before each scenario.

Run it by hand on an otherwise idle machine. It is not a test: nothing in the gate measures time or throughput.
"""

from __future__ import annotations

import argparse
import json
import os
import signal
import subprocess
import sys
import time
from pathlib import Path

Repo = Path(__file__).resolve().parents[2]
Shims = ["stdlib/std/net/native/net_shim.cpp", "stdlib/std/os/os_posix.cpp"]


def build(primec: Path, source: Path, exe: Path) -> None:
    command = [str(primec), "--emit=optexe", str(source), "-o", str(exe)]
    for shim in Shims:
        command += ["--link-arg", str(Repo / shim)]
    print(f"building {exe.name}", file=sys.stderr)
    subprocess.run(command, check=True)


def serve_copy(out: Path) -> Path:
    """examples/apps/todo_web with `serve` where the example calls `serveWorkers`; everything else is the same."""
    source = (Repo / "examples/apps/todo_web/main.prime").read_text(encoding="utf-8")
    call = "  serveWorkers(withRequestLog("
    if source.count(call) != 1:
        raise SystemExit("examples/apps/todo_web/main.prime no longer calls serveWorkers(withRequestLog(...")
    copy = out / "todo_web_serve.prime"
    copy.write_text(source.replace(call, "  serve(withRequestLog("), encoding="utf-8")
    return copy


class Server:
    """A server process on a free loopback port; its stderr (the request log) goes to a file."""

    def __init__(self, exe: Path, environment: dict[str, str], log: Path) -> None:
        self.log = log
        self.errors = open(log, "wb")
        self.process = subprocess.Popen([str(exe)], env={**os.environ, **environment, "PORT": "0"},
                                        stderr=self.errors, stdout=subprocess.DEVNULL)
        self.port = 0
        deadline = time.monotonic() + 10
        while self.port == 0:
            text = log.read_text(encoding="utf-8", errors="replace")
            at = text.find("listening on ")
            if at >= 0 and "\n" in text[at:]:
                self.port = int(text[at + 13:text.index("\n", at)])
            elif self.process.poll() is not None or time.monotonic() > deadline:
                raise SystemExit(f"{exe.name} did not start: {text}")
            else:
                time.sleep(0.01)

    def stop(self) -> None:
        self.process.send_signal(signal.SIGTERM)
        code = self.process.wait()
        self.errors.close()
        if code != 0:
            raise SystemExit(f"the server exited with {code} after SIGTERM; see {self.log}")


def load(port: int, path: str, *options: str) -> dict:
    command = [sys.executable, str(Repo / "scripts/http_load.py"), f"http://127.0.0.1:{port}{path}", "--json"]
    done = subprocess.run(command + list(options), capture_output=True, text=True, check=False)
    if done.returncode != 0:
        raise SystemExit(f"http_load failed ({done.returncode}): {done.stderr}")
    return json.loads(done.stdout)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--build-dir", type=Path, default=Repo / "build-release")
    parser.add_argument("--seconds", type=float, default=10.0, help="how long each run lasts (default 10)")
    parser.add_argument("--connections", type=int, default=16, help="concurrent clients (default 16)")
    parser.add_argument("--workers", type=int, default=4, help="the larger worker count (default 4)")
    parser.add_argument("--items", type=int, default=200, help="todos on the list page (default 200)")
    parser.add_argument("--skip-build", action="store_true", help="use the programs of an earlier run")
    options = parser.parse_args()
    build_dir = options.build_dir.resolve()
    out = build_dir / "benchmarks" / "http"
    out.mkdir(parents=True, exist_ok=True)
    todo, todo_serve, bench = out / "todo_web", out / "todo_web_serve", out / "bench_server"
    if not options.skip_build:
        primec = build_dir / "primec"
        build(primec, Repo / "examples/apps/todo_web/main.prime", todo)
        build(primec, serve_copy(out), todo_serve)
        build(primec, Repo / "benchmarks/http/bench_server.prime", bench)

    workers = str(options.workers)
    servers = [
        ("serve", todo_serve, {"SERVER": "serve"}),
        ("1 worker", todo, {"SERVER": "workers", "WORKERS": "1"}),
        (f"{workers} workers", todo, {"SERVER": "workers", "WORKERS": workers}),
    ]
    scenarios = [
        (f"list page, {options.items} items (todo_web GET /)", True, "/", []),
        ("32 KB form, small handler (POST /form)", False, "/form", ["--form-bytes", "32768"]),
        ("about 1 ms of work in the handler (GET /work)", False, "/work", []),
    ]
    run_options = ["--connections", str(options.connections), "--seconds", str(options.seconds)]
    print(f"{os.cpu_count()} processors, {options.connections} connections, {options.seconds:g} s per run\n")
    print("| Scenario | Load average before | Server | Requests/s | p50 ms | p99 ms | Against `serve` |")
    print("| --- | --- | --- | ---: | ---: | ---: | ---: |")
    for title, is_todo, path, extra in scenarios:
        before = "%.2f %.2f %.2f" % os.getloadavg()
        baseline = 0.0
        for name, todo_exe, environment in servers:
            server = Server(todo_exe if is_todo else bench, {**environment, "BIND": "loopback"},
                            out / "server.log")
            try:
                if is_todo:
                    load(server.port, "/todos", "--body", "title=buy+more+milk+on+the+way+home",
                         "--connections", "1", "--requests", str(options.items))
                result = load(server.port, path, *extra, *run_options)
            finally:
                server.stop()
            rate = result["requests_per_second"]
            baseline = rate if name == "serve" else baseline
            ratio = "1.00x" if name == "serve" else "%.2fx" % (rate / baseline)
            print(f"| {title} | {before} | {name} | {rate:.0f} | {result['p50_ms']:.2f} | {result['p99_ms']:.2f} "
                  f"| {ratio} |", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
