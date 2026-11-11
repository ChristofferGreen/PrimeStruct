#!/usr/bin/env python3
"""Load generator for the HTTP servers of /std/http/server (benchmarks/http/README.md).

Opens CONNECTIONS concurrent clients against one URL. Each client sends one request per connection, reads the
response until the server closes (the servers have no keep-alive) and connects again, for a fixed time
(--seconds) or a fixed number of requests (--requests). Prints requests per second and the p50 and p99 latency.

Exit codes: 0 when every response was 2xx or 3xx; 1 when a response had another status or was not HTTP; 2 when a
client could not connect or the connection failed; 64 for a bad command line.

Each client is a process of its own (this script run again with --client), so the generator is not held to one
core by the interpreter lock. Python standard library only.

Examples:
  scripts/http_load.py http://127.0.0.1:8080/ --connections 16 --seconds 10
  scripts/http_load.py http://127.0.0.1:8080/form --form-bytes 32768 --seconds 10
  scripts/http_load.py http://127.0.0.1:8080/todos --body title=milk --connections 1 --requests 200
"""

import argparse
import json
import socket
import subprocess
import sys
import time
from urllib.parse import urlsplit

ExitBadStatus = 1
ExitConnection = 2
ExitUsage = 64


def build_request(method, host, port, path, body):
    lines = ["%s %s HTTP/1.1" % (method, path), "Host: %s:%d" % (host, port), "Connection: close"]
    if body is not None:
        lines.append("Content-Type: application/x-www-form-urlencoded")
        lines.append("Content-Length: %d" % len(body))
    head = ("\r\n".join(lines) + "\r\n\r\n").encode("ascii")
    return head + (body if body is not None else b"")


def status_of(response):
    """The status code of an HTTP response, or 0 when it is not one."""
    line = response.split(b"\r\n", 1)[0].split(b" ")
    if len(line) < 2 or not line[0].startswith(b"HTTP/") or not line[1].isdigit():
        return 0
    return int(line[1])


class ConnectError(OSError):
    """The server did not take the connection (refused, unreachable or timed out)."""


def exchange(address, request, timeout):
    """Sends `request` on a new connection and returns everything the server sends until it closes."""
    try:
        connection = socket.create_connection(address, timeout=timeout)
    except OSError as error:
        raise ConnectError("cannot connect: %s" % error) from error
    with connection:
        connection.sendall(request)
        chunks = []
        while True:
            chunk = connection.recv(65536)
            if not chunk:
                return b"".join(chunks)
            chunks.append(chunk)


def run_client(options, request):
    """One client: requests until the time or the count is used up, or until the first failure."""
    address = (options.host, options.port)
    latencies = []
    result = {"latencies_us": latencies, "elapsed": 0.0, "bad_status": None, "error": None}
    if options.start_at > 0:
        delay = options.start_at - time.time()
        if delay > 0:
            time.sleep(delay)
    started = time.monotonic()
    deadline = started + options.seconds if options.requests == 0 else None
    while True:
        if deadline is None:
            if len(latencies) >= options.requests:
                break
        elif time.monotonic() >= deadline:
            break
        before = time.monotonic()
        try:
            response = exchange(address, request, options.timeout)
        except ConnectError as error:
            result["error"] = str(error)
            break
        except OSError as error:
            result["error"] = "connection failed: %s" % error
            break
        after = time.monotonic()
        status = status_of(response)
        if status < 200 or status > 399:
            result["bad_status"] = status
            break
        latencies.append(int((after - before) * 1000000))
    result["elapsed"] = time.monotonic() - started
    return result


def percentile(ordered, percent):
    """Nearest-rank percentile of a sorted list."""
    if not ordered:
        return 0
    rank = max(1, -(-len(ordered) * percent // 100))
    return ordered[int(rank) - 1]


class UsageParser(argparse.ArgumentParser):
    def error(self, message):
        self.print_usage(sys.stderr)
        self.exit(ExitUsage, "%s: error: %s\n" % (self.prog, message))


def parse_arguments(arguments):
    parser = UsageParser(description="Load generator for /std/http/server programs.")
    parser.add_argument("url", help="http://host:port/path")
    parser.add_argument("--connections", type=int, default=16, help="concurrent clients (default 16)")
    parser.add_argument("--seconds", type=float, default=10.0, help="how long to run (default 10)")
    parser.add_argument("--requests", type=int, default=0,
                        help="requests per client instead of a time; 0 (default) runs for --seconds")
    parser.add_argument("--method", default=None, help="GET, or POST when there is a body")
    parser.add_argument("--body", default=None, help="request body, sent as a form")
    parser.add_argument("--form-bytes", type=int, default=0,
                        help="send a form body of this many bytes (one field, `text`)")
    parser.add_argument("--timeout", type=float, default=30.0, help="seconds a connection may wait (default 30)")
    parser.add_argument("--json", action="store_true", help="print the result as one JSON object")
    parser.add_argument("--client", action="store_true", help=argparse.SUPPRESS)
    parser.add_argument("--start-at", type=float, default=0.0, help=argparse.SUPPRESS)
    options = parser.parse_args(arguments)
    target = urlsplit(options.url)
    if target.scheme != "http" or not target.hostname:
        parser.error("the URL must be http://host:port/path")
    if options.connections < 1 or options.requests < 0 or options.seconds <= 0 or options.form_bytes < 0:
        parser.error("--connections must be at least 1, --seconds above 0, --requests and --form-bytes not negative")
    if options.body is not None and options.form_bytes > 0:
        parser.error("give --body or --form-bytes, not both")
    options.host = target.hostname
    options.port = target.port if target.port is not None else 80
    options.path = (target.path or "/") + ("?" + target.query if target.query else "")
    return options


def request_of(options):
    body = None
    if options.body is not None:
        body = options.body.encode("utf-8")
    elif options.form_bytes > 0:
        body = (b"text=" + b"a" * options.form_bytes)[:max(options.form_bytes, 5)]
    method = options.method if options.method else ("POST" if body is not None else "GET")
    return build_request(method, options.host, options.port, options.path, body)


def main(arguments):
    options = parse_arguments(arguments)
    request = request_of(options)
    if options.client:
        json.dump(run_client(options, request), sys.stdout)
        return 0
    # The clients start together, after every interpreter has had time to start.
    start_at = time.time() + 0.5 + 0.02 * options.connections if options.requests == 0 else 0.0
    command = [sys.executable, __file__] + list(arguments) + ["--client", "--start-at", repr(start_at)]
    clients = [subprocess.Popen(command, stdout=subprocess.PIPE) for _ in range(options.connections)]
    results = []
    for client in clients:
        output = client.communicate()[0]
        try:
            results.append(json.loads(output))
        except ValueError:
            results.append({"latencies_us": [], "elapsed": 0.0, "bad_status": None,
                            "error": "client exited with code %d" % client.returncode})
    latencies = sorted(value for result in results for value in result["latencies_us"])
    rate = sum(len(result["latencies_us"]) / result["elapsed"] for result in results if result["elapsed"] > 0)
    summary = {
        "url": options.url,
        "connections": options.connections,
        "requests": len(latencies),
        "requests_per_second": round(rate, 1),
        "p50_ms": round(percentile(latencies, 50) / 1000.0, 3),
        "p99_ms": round(percentile(latencies, 99) / 1000.0, 3),
    }
    if options.json:
        print(json.dumps(summary))
    else:
        print("%d requests on %d connections: %.1f requests/s, p50 %.3f ms, p99 %.3f ms" %
              (summary["requests"], summary["connections"], summary["requests_per_second"], summary["p50_ms"],
               summary["p99_ms"]))
    errors = [result["error"] for result in results if result["error"]]
    statuses = [result["bad_status"] for result in results if result["bad_status"] is not None]
    if errors:
        print("http_load: %d of %d clients failed against %s:%d: %s" %
              (len(errors), len(results), options.host, options.port, errors[0]), file=sys.stderr)
        return ExitConnection
    if statuses:
        what = "status %d" % statuses[0] if statuses[0] else "not an HTTP response"
        print("http_load: %d of %d clients got a response that is not 2xx or 3xx (%s) from %s" %
              (len(statuses), len(results), what, options.url), file=sys.stderr)
        return ExitBadStatus
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
