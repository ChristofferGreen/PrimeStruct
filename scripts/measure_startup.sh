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
#                        person at the machine, or an agent the person allowed to show windows for a while; never
#                        for tests.
#   --cold               every measured launch runs a fresh copy of the bundle at a new path (`ditto`, removed and
#                        unregistered from Launch Services afterwards), so it pays what a first launch of new code
#                        pays: the Gatekeeper assessment, the code-signature check and a cold file cache of the
#                        copy, and nothing is warm but the system's own caches (a launch after `purge` or a reboot
#                        needs root or a person; a reboot is the colder of the two). The warm-up launch uses the
#                        bundle itself, so it absorbs the probe's own first load. Needs a bundle.
#   --marks MODE         who writes the phase marks: `probe` (default: the probe is injected into a PrimeStruct app),
#                        `self` (the program writes them itself: the reference editor) or `none` (a program without
#                        marks, such as TextEdit: only --poll can time it)
#   --poll NAME          with --open: also time the first on-screen window of the process named NAME by polling the
#                        window list every millisecond (tools/launch_bench/window_poll.c); with --marks none the
#                        process is ended (SIGTERM) once its window is listed. The table's `listed` row
#   --app-args ARGS      arguments passed through `open --args` to a program that is not a PrimeStruct app (words,
#                        e.g. "-ApplePersistenceIgnoreState YES" so TextEdit does not bring documents back)
#   --work-dir DIR       where the probe and the reports go (default: a new temporary directory)
#
# A target ending in .mm (tools/launch_bench/reference_editor.mm) is built with clang++ into a bundle in the work
# directory and measured with --marks self: the reference Objective-C++ editor with the same window, menu, toolbar and
# text view setup as PrimeScroll, for the budget "within 5 ms of a pure Objective-C++ editor".
#
# Examples: scripts/measure_startup.sh --runs 15 ~/Applications/PrimeScroll.app
#           scripts/measure_startup.sh --runs 15 tools/launch_bench/reference_editor.mm
#           scripts/measure_startup.sh --open --runs 10 --marks none --poll TextEdit /System/Applications/TextEdit.app
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
COLD=0
MARKS=""
POLL=""
APP_ARGS=""
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
    --cold) COLD=1; shift ;;
    --marks) MARKS="$2"; shift 2 ;;
    --poll) POLL="$2"; shift 2 ;;
    --app-args) APP_ARGS="$2"; shift 2 ;;
    --work-dir) WORK="$2"; shift 2 ;;
    -h|--help) usage ;;
    -*) echo "unknown option: $1" >&2; usage ;;
    *) TARGET="$1"; shift ;;
  esac
done
[[ -n "$TARGET" ]] || usage
[[ "$(uname -s)" == "Darwin" ]] || { echo "measure_startup.sh: macOS only" >&2; exit 2; }

if [[ -z "$WORK" ]]; then
  WORK="$(mktemp -d -t primestruct_startup)"
fi
mkdir -p "$WORK"

# The reference editor: built here, bundled (Launch Services needs a bundle for --open), ad-hoc signed.
if [[ "$TARGET" == *.mm ]]; then
  REF_APP="$WORK/ReferenceEditor.app"
  rm -rf "$REF_APP"
  mkdir -p "$REF_APP/Contents/MacOS"
  clang++ -std=c++20 -O2 -fobjc-arc -framework AppKit -framework Foundation -o "$REF_APP/Contents/MacOS/ReferenceEditor" \
    "$TARGET"
  cat >"$REF_APP/Contents/Info.plist" <<'PLIST'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>CFBundleName</key><string>ReferenceEditor</string>
  <key>CFBundleIdentifier</key><string>com.primestruct.launch-bench.reference-editor</string>
  <key>CFBundleExecutable</key><string>ReferenceEditor</string>
  <key>CFBundlePackageType</key><string>APPL</string>
  <key>CFBundleVersion</key><string>1</string>
  <key>LSMinimumSystemVersion</key><string>13.0</string>
  <key>NSHighResolutionCapable</key><true/>
  <key>NSPrincipalClass</key><string>NSApplication</string>
</dict>
</plist>
PLIST
  codesign --force --sign - "$REF_APP" >/dev/null 2>&1
  TARGET="$REF_APP"
  [[ -n "$MARKS" ]] || MARKS="self"
fi
MARKS="${MARKS:-probe}"
case "$MARKS" in probe | self | none) ;; *) echo "--marks takes probe, self or none" >&2; exit 64 ;; esac

BUNDLE=""
if [[ -d "$TARGET" && "$TARGET" == *.app ]]; then
  BUNDLE="$(cd "$TARGET" && pwd)"
  EXE_NAME="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleExecutable' "$BUNDLE/Contents/Info.plist")"
  EXE="$BUNDLE/Contents/MacOS/$EXE_NAME"
else
  EXE="$TARGET"
fi
[[ -x "$EXE" ]] || { echo "not an executable: $EXE" >&2; exit 2; }
if [[ ( $OPEN -eq 1 || $COLD -eq 1 ) && -z "$BUNDLE" ]]; then
  echo "--open and --cold need an .app bundle" >&2
  exit 64
fi
if [[ -n "$POLL" && $OPEN -eq 0 ]]; then
  echo "--poll needs --open" >&2
  exit 64
fi
if [[ "$MARKS" == "none" && ( -z "$POLL" || $OPEN -eq 0 ) ]]; then
  echo "--marks none needs --open --poll NAME: nothing else times the program" >&2
  exit 64
fi

PROBE="$WORK/startup_probe.dylib"
# Rebuilt only when the source changed: new code pays the Gatekeeper assessment on its first load, which the
# warm-up launch absorbs either way.
if [[ "$MARKS" == "probe" ]] && [[ ! -f "$PROBE" || "$ROOT_DIR/tools/startup_probe/startup_probe.c" -nt "$PROBE" ]]; then
  clang -O1 -dynamiclib -o "$PROBE" "$ROOT_DIR/tools/startup_probe/startup_probe.c" -I"$ROOT_DIR/include" -lobjc \
    -Wl,-undefined,dynamic_lookup
fi
POLLER="$WORK/window_poll"
if [[ -n "$POLL" && ( ! -x "$POLLER" || "$ROOT_DIR/tools/launch_bench/window_poll.c" -nt "$POLLER" ) ]]; then
  clang -O1 -o "$POLLER" "$ROOT_DIR/tools/launch_bench/window_poll.c" -framework CoreGraphics -framework CoreFoundation
fi
REPORT="$WORK/startup_report.txt"
SNAPSHOT="$WORK/startup_snapshot.png"
rm -f "$REPORT"

now_us() { python3 -c 'import time; print(int(time.time() * 1e6))'; }

LSREGISTER=/System/Library/Frameworks/CoreServices.framework/Versions/A/Frameworks/LaunchServices.framework/Versions/A/Support/lsregister

# launch <bundle> <executable>: one launch; appends a "phase" line (and, with --poll, a "poll" line) to $REPORT.
launch() {
  local bundle="$1" exe="$2"
  local envs=(PS_PROBE_OUT="$REPORT")
  [[ "$MARKS" == "probe" ]] && envs+=(DYLD_INSERT_LIBRARIES="$PROBE")
  [[ -n "$SESSION" ]] && envs+=(PS_PROBE_SESSION="$SESSION" PS_PROBE_SESSION_PREVIEWS="${PREVIEWS:-0}")
  [[ $SAMPLE -eq 1 ]] && envs+=(PS_PROBE_SAMPLE_US=1000)
  if [[ $OPEN -eq 1 ]]; then
    local args=(-n -W -a "$bundle")
    local item t0
    t0="$(now_us)"
    if [[ "$MARKS" != "none" ]]; then
      local delay=()
      [[ -n "$POLL" ]] && delay=(PS_PROBE_EXIT_DELAY_MS=400) # the window stays up long enough to be listed
      for item in "${envs[@]}" "PS_PROBE_T0_US=$t0" PS_PROBE_EXIT_AFTER_TEXT=1 "${delay[@]+"${delay[@]}"}"; do
        args+=(--env "$item")
      done
    fi
    [[ -n "$FILE" ]] && args+=("$FILE")
    # shellcheck disable=SC2206
    [[ -n "$APP_ARGS" ]] && args+=(--args $APP_ARGS)
    if [[ -n "$POLL" ]]; then
      local kill_flag=()
      [[ "$MARKS" == "none" ]] && kill_flag=(--kill)
      "$POLLER" --owner "$POLL" --t0-us "$t0" --timeout-ms 30000 --out "$REPORT" "${kill_flag[@]+"${kill_flag[@]}"}" &
      local poller=$!
      open "${args[@]}"
      wait "$poller" || true
    else
      open "${args[@]}"
    fi
  else
    local args=()
    [[ -n "$FILE" ]] && args+=("$FILE")
    env "${envs[@]}" PRIMESTRUCT_UI_SNAPSHOT="$SNAPSHOT" "$exe" "${args[@]+"${args[@]}"}" >/dev/null 2>"$WORK/stderr.txt"
  fi
}

# One measured launch: the bundle itself, or with --cold a fresh copy of it at a new path.
measured_launch() { # measured_launch <index>
  if [[ $COLD -eq 0 ]]; then
    launch "$BUNDLE" "$EXE"
    return
  fi
  local dir="$WORK/cold_$1"
  rm -rf "$dir"
  mkdir -p "$dir"
  ditto "$BUNDLE" "$dir/$(basename "$BUNDLE")"
  launch "$dir/$(basename "$BUNDLE")" "$dir/$(basename "$BUNDLE")/Contents/MacOS/$EXE_NAME"
  "$LSREGISTER" -u "$dir/$(basename "$BUNDLE")" >/dev/null 2>&1 || true
  rm -rf "$dir"
}

launch "$BUNDLE" "$EXE"
WARMUP_MAIN="$(grep '^phase' "$REPORT" | tail -1 | sed -n 's/.*main_ms=\([0-9.-]*\).*/\1/p' || true)"
rm -f "$REPORT"
for ((i = 0; i < RUNS; i++)); do
  measured_launch "$i"
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
listed = []
calls = {}
lines = open(report).read().splitlines()
for line in lines:
    if line.startswith("poll"):
        m = re.search(r"\bwindow_listed_ms=(-?[0-9.]+)", line)
        if m and float(m.group(1)) >= 0:
            listed.append(float(m.group(1)))
    if not line.startswith("phase"):
        continue
    for p in phases:
        m = re.search(r"\b%s_ms=(-?[0-9.]+)" % p, line)
        if m and float(m.group(1)) >= 0:
            values[p].append(float(m.group(1)))
    for name, n, ms in re.findall(r"\b([a-z_]+)=(\d+)/([0-9.]+)", line):
        calls.setdefault(name, []).append((int(n), float(ms)))
print("launches: %d measured (after one warm-up), load average %s"
      % (max(len(values["main"]), len(listed)), load.strip("{} ")))
print("%-9s %8s %8s %8s" % ("phase", "median", "min", "max"))
for p in phases:
    v = values[p]
    if v:
        print("%-9s %8.0f %8.0f %8.0f  ms" % (p, statistics.median(v), min(v), max(v)))
    else:
        print("%-9s %8s" % (p, "-"))
if listed:
    print("%-9s %8.0f %8.0f %8.0f  ms (first on-screen window in the window list, polled every ms)"
          % ("listed", statistics.median(listed), min(listed), max(listed)))
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
