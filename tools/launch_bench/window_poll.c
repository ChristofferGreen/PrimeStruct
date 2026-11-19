/* Times the first on-screen window of an application that has no probe (docs/NativeUiPlan.md section 1,
 * scripts/measure_startup.sh --poll). Polls the window list every millisecond for a window of a layer-0, on-screen
 * window owned by a process of the given name, and prints one report line:
 *
 *   poll window_listed_ms=<ms since --t0-us> pid=<pid>
 *
 * The window list needs no screen recording permission for the owner's name, pid, layer and bounds (only window
 * titles are withheld). With --kill the process is sent SIGTERM once its window is listed, which ends an app that
 * has no way to quit by itself (TextEdit); a timeout prints window_listed_ms=-1 and also ends it.
 *
 * Usage: window_poll --owner NAME --t0-us <wall clock microseconds> [--timeout-ms N] [--kill] [--out FILE]
 * Build: clang -O1 -o window_poll window_poll.c -framework CoreGraphics -framework CoreFoundation
 */
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static double now_us(void) {
  struct timespec t;
  clock_gettime(CLOCK_REALTIME, &t);
  return t.tv_sec * 1e6 + t.tv_nsec / 1e3;
}

/* The pid of a listed window of `owner` (at least 100 x 100 points, layer 0), or 0. */
static int find_window(const char *owner, int *lastSeenPid) {
  CFArrayRef list = CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly | kCGWindowListExcludeDesktopElements,
                                               kCGNullWindowID);
  if (list == NULL) {
    return 0;
  }
  int found = 0;
  for (CFIndex i = 0; i < CFArrayGetCount(list) && !found; i++) {
    CFDictionaryRef info = (CFDictionaryRef)CFArrayGetValueAtIndex(list, i);
    char name[256] = "";
    CFStringRef ownerName = (CFStringRef)CFDictionaryGetValue(info, kCGWindowOwnerName);
    if (ownerName == NULL || !CFStringGetCString(ownerName, name, sizeof name, kCFStringEncodingUTF8) ||
        strcmp(name, owner) != 0) {
      continue;
    }
    int layer = -1, pid = 0;
    CFNumberRef number = (CFNumberRef)CFDictionaryGetValue(info, kCGWindowLayer);
    if (number != NULL) {
      CFNumberGetValue(number, kCFNumberIntType, &layer);
    }
    number = (CFNumberRef)CFDictionaryGetValue(info, kCGWindowOwnerPID);
    if (number != NULL) {
      CFNumberGetValue(number, kCFNumberIntType, &pid);
    }
    *lastSeenPid = pid;
    CGRect bounds = CGRectZero;
    CFDictionaryRef boundsInfo = (CFDictionaryRef)CFDictionaryGetValue(info, kCGWindowBounds);
    if (boundsInfo != NULL) {
      CGRectMakeWithDictionaryRepresentation(boundsInfo, &bounds);
    }
    if (layer == 0 && bounds.size.width >= 100 && bounds.size.height >= 100) {
      found = pid;
    }
  }
  CFRelease(list);
  return found;
}

int main(int argc, char **argv) {
  const char *owner = NULL, *outPath = NULL;
  double t0 = 0;
  int timeoutMs = 15000, kill_it = 0;
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--owner") == 0 && i + 1 < argc) {
      owner = argv[++i];
    } else if (strcmp(argv[i], "--t0-us") == 0 && i + 1 < argc) {
      t0 = atof(argv[++i]);
    } else if (strcmp(argv[i], "--timeout-ms") == 0 && i + 1 < argc) {
      timeoutMs = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
      outPath = argv[++i];
    } else if (strcmp(argv[i], "--kill") == 0) {
      kill_it = 1;
    } else {
      fprintf(stderr, "usage: window_poll --owner NAME --t0-us US [--timeout-ms N] [--kill] [--out FILE]\n");
      return 64;
    }
  }
  if (owner == NULL || t0 == 0) {
    fprintf(stderr, "window_poll: --owner and --t0-us are required\n");
    return 64;
  }
  int pid = 0, seen = 0;
  double at = 0;
  const double deadline = now_us() + timeoutMs * 1000.0;
  while (now_us() < deadline) {
    pid = find_window(owner, &seen);
    if (pid != 0) {
      at = now_us();
      break;
    }
    usleep(1000);
  }
  FILE *out = outPath != NULL ? fopen(outPath, "a") : stdout;
  if (out != NULL) {
    fprintf(out, "poll window_listed_ms=%.1f pid=%d\n", pid != 0 ? (at - t0) / 1000.0 : -1.0, pid != 0 ? pid : seen);
    if (out != stdout) {
      fclose(out);
    }
  }
  if (kill_it && (pid != 0 || seen != 0)) {
    kill(pid != 0 ? pid : seen, SIGTERM);
  }
  return 0;
}
