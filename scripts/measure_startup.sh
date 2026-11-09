#!/usr/bin/env bash
# Launch time of a PrimeStruct native UI app on macOS (TODO-5603, docs/NativeUiPlan.md section 1).
#
# Builds tools/startup_probe/startup_probe.c (clang only, a few hundred milliseconds), loads it into the app with
# DYLD_INSERT_LIBRARIES and launches the app N times in the AppKit backend's snapshot mode: no Dock icon, no focus,
# and no saved preferences or session (the probe can inject a session with --session). It prints the median and
# range of each phase, measured from the process start:
#   main      dyld has loaded and bound every image (the probe's constructor)
#   app_init  ps_ui_init returned (NSApplication, the menu bar, finishLaunching)
#   loop      the program reached its event loop: everything it does before the first frame is done
#   window    the first window display;  text  the first drawn text
#   frame     the program got PS_UI_EVENT_FIRST_FRAME;  settled  its next wait: the work after the first frame is done
# then the time inside the ps_ui calls that dominate a launch (before the event loop, and after it until `settled`),
# the peak resident set, and the number of images dyld loads (distinct paths; with --reference, also those of a plain
# AppKit program and the images only the app loads).
# The snapshot's own idle wait (about 600 ms before the PNG is written) is not part of any phase.
#
# Usage: scripts/measure_startup.sh [options] <App.app | executable>
#   --runs N             launches to measure (default 10), after one discarded warm-up launch
#   --file PATH          pass PATH as the program's first argument (a document opened at launch)
#   --session A:B:...    restore these files as the last session (PS_PROBE_SESSION); --previews 0:1:2 per file
#   --max-libraries N    exit 1 when the launch loads more than N images
#   --reference EXE      also count the images of EXE (a plain AppKit program, tools/startup_probe/reference_app.m,
#                        which exits by itself), and list the images only the app loads
#   --max-extra-images N exit 1 when the app loads more than N images the reference does not (the guard of TODO-5603:
#                        a new framework link or an eager system lookup adds images)
#   --sample             sample the main thread every millisecond and print the top symbols per phase
#   --open               launch through Launch Services (`open -n -W`) with the real preferences and session; the
#                        app shows its window and takes focus, and quits right after its first drawn text. For a
#                        person at the machine, never for tests or agents.
#   --work-dir DIR       where the probe and the reports go (default: a new temporary directory)
#
# Example: scripts/measure_startup.sh --runs 15 ~/Applications/PrimeScroll.app
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
RUNS=10
FILE=""
SESSION=""
PREVIEWS=""
MAX_LIBRARIES=""
REFERENCE=""
MAX_EXTRA=""
SAMPLE=0
OPEN=0
WORK=""
TARGET=""

usage() {
  sed -n '2,/^set -euo/p' "$0" | sed '$d' | sed 's/^# \{0,1\}//' >&2
  exit 64
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --runs) RUNS="$2"; shift 2 ;;
    --file) FILE="$2"; shift 2 ;;
    --session) SESSION="$2"; shift 2 ;;
    --previews) PREVIEWS="$2"; shift 2 ;;
    --max-libraries) MAX_LIBRARIES="$2"; shift 2 ;;
    --reference) REFERENCE="$2"; shift 2 ;;
    --max-extra-images) MAX_EXTRA="$2"; shift 2 ;;
    --sample) SAMPLE=1; shift ;;
    --open) OPEN=1; shift ;;
    --work-dir) WORK="$2"; shift 2 ;;
    -h|--help) usage ;;
    -*) echo "unknown option: $1" >&2; usage ;;
    *) TARGET="$1"; shift ;;
  esac
done
[[ -n "$TARGET" ]] || usage
[[ "$(uname -s)" == "Darwin" ]] || { echo "measure_startup.sh: macOS only" >&2; exit 2; }

BUNDLE=""
if [[ -d "$TARGET" && "$TARGET" == *.app ]]; then
  BUNDLE="$(cd "$TARGET" && pwd)"
  EXE_NAME="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleExecutable' "$BUNDLE/Contents/Info.plist")"
  EXE="$BUNDLE/Contents/MacOS/$EXE_NAME"
else
  EXE="$TARGET"
fi
[[ -x "$EXE" ]] || { echo "not an executable: $EXE" >&2; exit 2; }
if [[ $OPEN -eq 1 && -z "$BUNDLE" ]]; then
  echo "--open needs an .app bundle" >&2
  exit 64
fi

if [[ -z "$WORK" ]]; then
  WORK="$(mktemp -d -t primestruct_startup)"
fi
mkdir -p "$WORK"
PROBE="$WORK/startup_probe.dylib"
# Rebuilt only when the source changed: new code pays the Gatekeeper assessment on its first load, which the
# warm-up launch absorbs either way.
if [[ ! -f "$PROBE" || "$ROOT_DIR/tools/startup_probe/startup_probe.c" -nt "$PROBE" ]]; then
  clang -O1 -dynamiclib -o "$PROBE" "$ROOT_DIR/tools/startup_probe/startup_probe.c" -I"$ROOT_DIR/include" -lobjc \
    -Wl,-undefined,dynamic_lookup
fi
REPORT="$WORK/startup_report.txt"
SNAPSHOT="$WORK/startup_snapshot.png"
rm -f "$REPORT"

now_us() { python3 -c 'import time; print(int(time.time() * 1e6))'; }

launch() { # one launch; appends a "phase" line to $REPORT
  local envs=(PS_PROBE_OUT="$REPORT" DYLD_INSERT_LIBRARIES="$PROBE")
  [[ -n "$SESSION" ]] && envs+=(PS_PROBE_SESSION="$SESSION" PS_PROBE_SESSION_PREVIEWS="${PREVIEWS:-0}")
  [[ $SAMPLE -eq 1 ]] && envs+=(PS_PROBE_SAMPLE_US=1000)
  if [[ $OPEN -eq 1 ]]; then
    local args=(-n -W -a "$BUNDLE")
    local item
    for item in "${envs[@]}" "PS_PROBE_T0_US=$(now_us)" PS_PROBE_EXIT_AFTER_TEXT=1; do
      args+=(--env "$item")
    done
    [[ -n "$FILE" ]] && args+=("$FILE")
    open "${args[@]}"
  else
    local args=()
    [[ -n "$FILE" ]] && args+=("$FILE")
    env "${envs[@]}" PRIMESTRUCT_UI_SNAPSHOT="$SNAPSHOT" "$EXE" "${args[@]+"${args[@]}"}" >/dev/null 2>"$WORK/stderr.txt"
  fi
}

launch
WARMUP_MAIN="$(grep '^phase' "$REPORT" | tail -1 | sed -n 's/.*main_ms=\([0-9.-]*\).*/\1/p')"
rm -f "$REPORT"
for ((i = 0; i < RUNS; i++)); do
  launch
done

# The distinct images of a launch: dyld prints a line per image it loads, "dyld[pid]: <uuid> path", and lines for the
# images whose initialization it defers and later runs ("move loaded to delayed: name", "move delayed to loaded:
# name"), which are not images of their own (the earlier count of 1266 to 1274 included the second kind).
images_of() { # images_of <out-file> <exe> [args...]
  local out="$1"
  shift
  env DYLD_PRINT_LIBRARIES=1 PRIMESTRUCT_UI_SNAPSHOT="$SNAPSHOT" "$@" >/dev/null 2>"$out.raw" || true
  grep '^dyld\[[0-9]*\]: <' "$out.raw" | sed 's/^dyld\[[0-9]*\]: <[^>]*> //' | sort -u >"$out"
}
LIBRARIES=""
EXTRA=""
if [[ $OPEN -eq 0 ]]; then
  images_of "$WORK/libraries.txt" "$EXE"
  LIBRARIES="$(wc -l <"$WORK/libraries.txt" | tr -d ' ')"
  if [[ -n "$REFERENCE" ]]; then
    images_of "$WORK/reference_libraries.txt" "$REFERENCE"
    # The executables themselves differ by name; every other image the app loads and the reference does not counts.
    comm -13 "$WORK/reference_libraries.txt" "$WORK/libraries.txt" | grep -vxF "$EXE" >"$WORK/extra_libraries.txt" || true
    EXTRA="$(wc -l <"$WORK/extra_libraries.txt" | tr -d ' ')"
  fi
fi

python3 - "$REPORT" "$RUNS" "${WARMUP_MAIN:-}" "${LIBRARIES:-}" "$(sysctl -n vm.loadavg)" "${EXTRA:-}" "$WORK" <<'EOF'
import re, statistics, sys
report, runs, warmup_main, libraries, load, extra, work = sys.argv[1:8]
phases = ["main", "app_init", "loop", "window", "text", "frame", "settled"]
values = {p: [] for p in phases}
calls = {}
lines = open(report).read().splitlines()
for line in lines:
    if not line.startswith("phase"):
        continue
    for p in phases:
        m = re.search(r"\b%s_ms=(-?[0-9.]+)" % p, line)
        if m and float(m.group(1)) >= 0:
            values[p].append(float(m.group(1)))
    for name, n, ms in re.findall(r"\b([a-z_]+)=(\d+)/([0-9.]+)", line):
        calls.setdefault(name, []).append((int(n), float(ms)))
print("launches: %d measured (after one warm-up), load average %s" % (len(values["main"]), load.strip("{} ")))
print("%-9s %8s %8s %8s" % ("phase", "median", "min", "max"))
for p in phases:
    v = values[p]
    if v:
        print("%-9s %8.0f %8.0f %8.0f  ms" % (p, statistics.median(v), min(v), max(v)))
    else:
        print("%-9s %8s" % (p, "-"))
busy = [(name, statistics.median([c[1] for c in v]), v[0][0]) for name, v in calls.items()]
busy = [b for b in busy if b[2] > 0]
for title, late in (("before the event loop", False), ("after it, until the program settled", True)):
    part = [b for b in busy if b[0].startswith("late_") == late]
    if part:
        print("time in ps_ui calls %s (median ms, calls): %.1f in all" % (title, sum(b[1] for b in part)))
        for name, ms, n in sorted(part, key=lambda b: -b[1]):
            print("  %-18s %7.1f  x%d" % (name[5:] if late else name, ms, n))
if warmup_main:
    note = (" (new code, the app's or the probe's, was assessed by Gatekeeper on this first load)"
            if float(warmup_main) > 150 else "")
    print("warm-up launch: main at %s ms%s" % (warmup_main, note))
rss = [float(m.group(1)) for line in lines for m in [re.search(r"\bmaxrss_kb=([0-9]+)", line)] if m]
if rss:
    print("peak resident set: median %.1f MB, max %.1f MB" % (statistics.median(rss) / 1024, max(rss) / 1024))
if libraries:
    print("images loaded: %s" % libraries)
if extra:
    names = [l.strip() for l in open(work + "/extra_libraries.txt") if l.strip()]
    print("images the reference does not load: %s%s" % (extra, "".join("\n  " + n for n in names)))
for line in lines:
    if line.startswith("samples") or line.startswith("  "):
        print(line)
EOF

if [[ -n "$MAX_LIBRARIES" && -n "$LIBRARIES" && "$LIBRARIES" -gt "$MAX_LIBRARIES" ]]; then
  echo "measure_startup.sh: the launch loads $LIBRARIES images, more than the limit of $MAX_LIBRARIES" >&2
  exit 1
fi
if [[ -n "$MAX_EXTRA" ]]; then
  if [[ -z "$EXTRA" ]]; then
    echo "measure_startup.sh: --max-extra-images needs --reference" >&2
    exit 64
  fi
  if [[ "$EXTRA" -gt "$MAX_EXTRA" ]]; then
    echo "measure_startup.sh: the launch loads $EXTRA images the reference does not, more than $MAX_EXTRA" >&2
    exit 1
  fi
fi
