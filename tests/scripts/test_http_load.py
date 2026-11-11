#!/usr/bin/env python3
"""Self-test for scripts/http_load.py: its exit codes and messages, against servers on 127.0.0.1.

Nothing here measures time: every run asks for a fixed number of requests, and the server answers from this
process. The refused connections go to port 0, where nothing can listen.
"""

from __future__ import annotations

import argparse
import json
import sys
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from audit_selftest_support import Checks, run  # noqa: E402


class Handler(BaseHTTPRequestHandler):
    """/ok answers 200, /moved 303, /form the length of the posted body, anything else 500."""

    def answer(self, status: int, body: bytes) -> None:
        self.send_response(status)
        if status == 303:
            self.send_header("Location", "/ok")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Connection", "close")
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self) -> None:  # noqa: N802
        if self.path == "/ok":
            self.answer(200, b"ok")
        elif self.path == "/moved":
            self.answer(303, b"")
        else:
            self.answer(500, b"broken")

    def do_POST(self) -> None:  # noqa: N802
        length = int(self.headers.get("Content-Length", "0"))
        body = self.rfile.read(length)
        self.server.posted.append(len(body))
        self.answer(200 if self.path == "/form" else 500, str(len(body)).encode("ascii"))

    def log_message(self, *arguments) -> None:
        pass


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", type=Path, default=Path(__file__).resolve().parents[2])
    repo = parser.parse_args().repo_root.resolve()
    checks = Checks("http load")

    try:
        server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    except PermissionError as error:
        print(f"http load self-test skipped: sockets are not permitted here ({error})")
        return 0
    server.posted = []
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    base = f"http://127.0.0.1:{server.server_address[1]}"

    def load(path: str, *extra: str):
        return run(repo, "http_load.py", base + path, "--connections", "2", "--requests", "3", "--json", *extra)

    good = load("/ok")
    checks.expect(good.returncode == 0, f"2xx responses must exit 0: {good.returncode} {good.stderr}")
    if good.returncode == 0:
        summary = json.loads(good.stdout)
        checks.expect(summary["requests"] == 6, f"2 connections of 3 requests are 6: {summary}")
        checks.expect(summary["requests_per_second"] > 0, f"a rate is reported: {summary}")
        checks.expect(summary["p99_ms"] >= summary["p50_ms"], f"p99 is not below p50: {summary}")

    moved = load("/moved")
    checks.expect(moved.returncode == 0, f"3xx responses must exit 0: {moved.returncode} {moved.stderr}")

    form = load("/form", "--form-bytes", "32768")
    checks.expect(form.returncode == 0, f"a posted form must exit 0: {form.returncode} {form.stderr}")
    checks.expect(server.posted == [32768] * 6, f"six bodies of 32768 bytes arrive: {server.posted}")

    broken = load("/broken")
    checks.expect(broken.returncode == 1, f"a 500 response must exit 1: {broken.returncode}")
    checks.expect("not 2xx or 3xx (status 500)" in broken.stderr, f"the 500 is named: {broken.stderr!r}")

    server.shutdown()
    server.server_close()

    # Nothing can listen on port 0, so every connection to it is refused, on any machine and at any time.
    refused = run(repo, "http_load.py", "http://127.0.0.1:0/", "--connections", "2", "--requests", "3")
    checks.expect(refused.returncode == 2, f"refused connections must exit 2: {refused.returncode}")
    checks.expect("2 of 2 clients failed against 127.0.0.1:0: cannot connect: " in refused.stderr,
                  f"the refusal is named: {refused.stderr!r}")

    usage = run(repo, "http_load.py", "ftp://127.0.0.1/")
    checks.expect(usage.returncode == 64, f"a URL that is not http must exit 64: {usage.returncode}")
    return checks.finish()


if __name__ == "__main__":
    sys.exit(main())
