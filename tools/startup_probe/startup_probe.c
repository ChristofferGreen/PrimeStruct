/* Launch-time probe for PrimeStruct native UI programs on macOS (TODO-5603, docs/NativeUiPlan.md section 1).
 *
 * Built by scripts/measure_startup.sh and loaded with DYLD_INSERT_LIBRARIES into a program that uses
 * libps_ui_appkit.dylib. It interposes the ps_ui calls that make up a launch and records, relative to the
 * process start (or PS_PROBE_T0_US, microseconds of the wall clock, when the caller started the launch):
 *   main          the probe's constructor: dyld has loaded and bound every image
 *   app_init      ps_ui_init returned (NSApplication, menu bar, finishLaunching)
 *   loop          the program's first ps_ui_wait_event: everything before the event loop is done
 *   window        the first -[NSWindow displayIfNeeded]
 *   text          the first -[NSTextView drawRect:] (the first drawn text)
 * plus the time spent inside the interposed calls before the loop, the page faults, and (PS_PROBE_SAMPLE_US) a
 * sample of the main thread every that many microseconds, counted per symbol for each phase.
 *
 * Environment:
 *   PS_PROBE_OUT=<file>          append the report there (default stderr)
 *   PS_PROBE_T0_US=<us>          launch start on the CLOCK_REALTIME scale (default: the kernel's process start)
 *   PS_PROBE_SESSION=<a:b:...>   answer ps_ui_pref_list_* for "session.files" with these paths (snapshot runs read
 *                                no preferences, so this is how a session restore is measured)
 *   PS_PROBE_SESSION_PREVIEWS=<0:1:2...>  the matching "session.previews"
 *   PS_PROBE_EXIT_AFTER_TEXT=1   write the report and _exit(0) right after the first text is drawn (a real launch
 *                                through `open`, which has no snapshot to end it)
 *   PS_PROBE_SAMPLE_US=<us>      sample the main thread
 */
#include "primec/ui/NativeUi.h"

#include <dlfcn.h>
#include <mach/mach.h>
#include <objc/message.h>
#include <objc/runtime.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/sysctl.h>
#include <time.h>
#include <unistd.h>

static double now_us(void) {
  struct timespec t;
  clock_gettime(CLOCK_REALTIME, &t);
  return t.tv_sec * 1e6 + t.tv_nsec / 1e3;
}

static double g_start, g_main, g_init_out, g_loop, g_window, g_text;
static long g_majflt_loop, g_minflt_loop;
static int g_reported;

enum { CallInit, CallWindowCreate, CallWindowShow, CallAddTab, CallSetToolbar, CallSetPreview, CallSetPreviewMode,
       CallTextViewCreate, CallLoadFile, CallHighlight, CallCount };
static const char *g_call_names[CallCount] = {"init", "window_create", "window_show", "add_tab", "set_toolbar",
                                              "set_preview", "set_preview_mode", "text_view_create", "load_file",
                                              "highlight"};
static double g_call_ms[CallCount];
static int g_call_n[CallCount];

#define TIMED(index, expr)                                                                                          \
  double t0 = now_us();                                                                                             \
  __typeof__(expr) r = (expr);                                                                                      \
  if (!g_loop) {                                                                                                    \
    g_call_ms[index] += (now_us() - t0) / 1000.0;                                                                   \
    g_call_n[index]++;                                                                                              \
  }                                                                                                                 \
  return r;

/* Main-thread sampler. */
#define MaxSamples 20000
#define MaxDepth 48
static mach_port_t g_main_thread;
static volatile int g_sampling;
static pthread_t g_sampler;
static int g_nsamples;
static double g_sample_time[MaxSamples];
static uintptr_t g_sample_stack[MaxSamples][MaxDepth];
static unsigned char g_sample_depth[MaxSamples];

static void *sampler_main(void *arg) {
  useconds_t interval = (useconds_t)(uintptr_t)arg;
  while (g_sampling && g_nsamples < MaxSamples) {
    usleep(interval);
    if (thread_suspend(g_main_thread) != KERN_SUCCESS) {
      continue;
    }
    arm_thread_state64_t st;
    mach_msg_type_number_t count = ARM_THREAD_STATE64_COUNT;
    if (thread_get_state(g_main_thread, ARM_THREAD_STATE64, (thread_state_t)&st, &count) == KERN_SUCCESS) {
      int n = 0;
      int i = g_nsamples;
      g_sample_stack[i][n++] = (uintptr_t)__darwin_arm_thread_state64_get_pc(st);
      g_sample_stack[i][n++] = (uintptr_t)__darwin_arm_thread_state64_get_lr(st);
      uintptr_t fp = (uintptr_t)__darwin_arm_thread_state64_get_fp(st);
      uintptr_t sp = (uintptr_t)__darwin_arm_thread_state64_get_sp(st);
      while (n < MaxDepth && fp >= sp && fp < sp + (8u << 20) && (fp & 7) == 0) {
        uintptr_t next = ((uintptr_t *)fp)[0];
        uintptr_t ret = ((uintptr_t *)fp)[1] & 0x0000000fffffffffULL;
        if (ret == 0) {
          break;
        }
        g_sample_stack[i][n++] = ret;
        if (next <= fp) {
          break;
        }
        fp = next;
      }
      g_sample_depth[i] = (unsigned char)n;
      g_sample_time[i] = now_us();
      g_nsamples++;
    }
    thread_resume(g_main_thread);
  }
  return NULL;
}

struct sym_count {
  char *name;
  int count;
};

static int by_count(const void *a, const void *b) {
  return ((const struct sym_count *)b)->count - ((const struct sym_count *)a)->count;
}

static void report_samples(FILE *out, const char *label, double from, double to) {
  if (!from || !to || to <= from) {
    return;
  }
  static struct sym_count table[4096];
  int entries = 0;
  int samples = 0;
  for (int i = 0; i < g_nsamples; i++) {
    if (g_sample_time[i] < from || g_sample_time[i] >= to) {
      continue;
    }
    samples++;
    const char *seen[MaxDepth];
    int nseen = 0;
    for (int d = 0; d < g_sample_depth[i]; d++) {
      char name[512];
      Dl_info info;
      if (dladdr((void *)(g_sample_stack[i][d] - 1), &info) && info.dli_fname) {
        const char *image = strrchr(info.dli_fname, '/');
        snprintf(name, sizeof name, "%s!%s", image ? image + 1 : info.dli_fname, info.dli_sname ? info.dli_sname : "?");
      } else {
        snprintf(name, sizeof name, "?");
      }
      int t = 0;
      while (t < entries && strcmp(table[t].name, name) != 0) {
        t++;
      }
      int dup = 0;
      for (int k = 0; k < nseen; k++) {
        dup |= t < entries && seen[k] == table[t].name;
      }
      if (dup) {
        continue;
      }
      if (t == entries) {
        if (entries == 4096) {
          continue;
        }
        table[entries].name = strdup(name);
        table[entries].count = 0;
        entries++;
      }
      table[t].count++;
      seen[nseen++] = table[t].name;
    }
  }
  qsort(table, entries, sizeof table[0], by_count);
  fprintf(out, "samples %s: %d over %.1f ms\n", label, samples, (to - from) / 1000.0);
  for (int t = 0; t < entries && t < 20; t++) {
    fprintf(out, "  %5.1f%% %s\n", samples ? 100.0 * table[t].count / samples : 0.0, table[t].name);
  }
}

static void report(void) {
  if (g_reported) {
    return;
  }
  g_reported = 1;
  double end = now_us();
  if (g_sampling) {
    g_sampling = 0;
    pthread_join(g_sampler, NULL);
  }
  const char *path = getenv("PS_PROBE_OUT");
  FILE *out = path ? fopen(path, "a") : stderr;
  if (out == NULL) {
    return;
  }
#define REL(x) ((x) ? ((x)-g_start) / 1000.0 : -1.0)
  fprintf(out, "phase main_ms=%.1f app_init_ms=%.1f loop_ms=%.1f window_ms=%.1f text_ms=%.1f end_ms=%.1f", REL(g_main),
          REL(g_init_out), REL(g_loop), REL(g_window), REL(g_text), REL(end));
  fprintf(out, " majflt=%ld minflt=%ld", g_majflt_loop, g_minflt_loop);
  for (int i = 0; i < CallCount; i++) {
    fprintf(out, " %s=%d/%.1f", g_call_names[i], g_call_n[i], g_call_ms[i]);
  }
  fprintf(out, "\n");
  if (g_nsamples > 0) {
    report_samples(out, "app_init", g_main, g_init_out);
    report_samples(out, "before_loop", g_init_out, g_loop);
    report_samples(out, "first_frame", g_loop, g_text ? g_text : g_window);
  }
  if (out != stderr) {
    fclose(out);
  }
}

static void after_text(void) {
  if (getenv("PS_PROBE_EXIT_AFTER_TEXT") != NULL) {
    report();
    _exit(0);
  }
}

/* Objective-C hooks for the first display and the first drawn text. */
typedef struct {
  double x, y, w, h;
} ProbeRect;
static IMP g_text_draw_imp, g_display_imp;

static void hook_text_draw(id self, SEL sel, ProbeRect rect) {
  ((void (*)(id, SEL, ProbeRect))g_text_draw_imp)(self, sel, rect);
  if (!g_text) {
    g_text = now_us();
    after_text();
  }
}

static void hook_display(id self, SEL sel) {
  if (!g_window) {
    g_window = now_us();
  }
  ((void (*)(id, SEL))g_display_imp)(self, sel);
}

static void install_objc_hooks(void) {
  Class textView = objc_getClass("NSTextView");
  SEL drawRect = sel_registerName("drawRect:");
  Method draw = textView ? class_getInstanceMethod(textView, drawRect) : NULL;
  if (draw) {
    g_text_draw_imp = method_getImplementation(draw);
    class_replaceMethod(textView, drawRect, (IMP)hook_text_draw, method_getTypeEncoding(draw));
  }
  Class window = objc_getClass("NSWindow");
  Method display = window ? class_getInstanceMethod(window, sel_registerName("displayIfNeeded")) : NULL;
  if (display) {
    g_display_imp = method_getImplementation(display);
    method_setImplementation(display, (IMP)hook_display);
  }
}

/* Interposed calls. */
static bool probe_init(const char *name) {
  install_objc_hooks();
  double enter = now_us();
  bool ok = ps_ui_init(name);
  g_init_out = now_us();
  g_call_ms[CallInit] = (g_init_out - enter) / 1000.0;
  g_call_n[CallInit] = 1;
  return ok;
}
static uint64_t probe_window_create(const char *t, int32_t w, int32_t h) { TIMED(CallWindowCreate, ps_ui_window_create(t, w, h)) }
static bool probe_window_show(uint64_t w) { TIMED(CallWindowShow, ps_ui_window_show(w)) }
static bool probe_add_tab(uint64_t host, uint64_t tab) { TIMED(CallAddTab, ps_ui_window_add_tab(host, tab)) }
static bool probe_set_toolbar(uint64_t w, uint64_t t) { TIMED(CallSetToolbar, ps_ui_window_set_toolbar(w, t)) }
static bool probe_set_preview(uint64_t w, bool on) { TIMED(CallSetPreview, ps_ui_window_set_preview(w, on)) }
static bool probe_set_preview_mode(uint64_t w, int32_t m) { TIMED(CallSetPreviewMode, ps_ui_window_set_preview_mode(w, m)) }
static uint64_t probe_text_view_create(void) { TIMED(CallTextViewCreate, ps_ui_text_view_create()) }
static bool probe_load_file(uint64_t v, const char *p) { TIMED(CallLoadFile, ps_ui_text_view_load_file(v, p)) }
static bool probe_highlight(uint64_t v, const char *p) { TIMED(CallHighlight, ps_ui_text_view_highlight(v, p)) }

static int32_t probe_wait_event(void) {
  if (!g_loop) {
    g_loop = now_us();
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    g_majflt_loop = usage.ru_majflt;
    g_minflt_loop = usage.ru_minflt;
  }
  return ps_ui_wait_event();
}

static const char *session_list(const char *key) {
  if (key == NULL) {
    return NULL;
  }
  if (strcmp(key, "session.files") == 0) {
    return getenv("PS_PROBE_SESSION");
  }
  if (strcmp(key, "session.previews") == 0) {
    return getenv("PS_PROBE_SESSION_PREVIEWS");
  }
  return NULL;
}

static int32_t probe_list_count(const char *key) {
  const char *list = session_list(key);
  if (list == NULL) {
    return ps_ui_pref_list_count(key);
  }
  int32_t n = *list ? 1 : 0;
  for (const char *c = list; *c; c++) {
    n += *c == ':';
  }
  return n;
}

static const char *probe_list_get(const char *key, int32_t index) {
  static char item[4096];
  const char *list = session_list(key);
  if (list == NULL) {
    return ps_ui_pref_list_get(key, index);
  }
  const char *c = list;
  while (index > 0 && *c) {
    index -= *c == ':';
    c++;
  }
  size_t k = 0;
  while (c[k] && c[k] != ':' && k + 1 < sizeof item) {
    item[k] = c[k];
    k++;
  }
  item[k] = '\0';
  return item;
}

__attribute__((constructor)) static void probe_start(void) {
  g_main = now_us();
  g_start = g_main;
  struct kinfo_proc info;
  size_t size = sizeof info;
  int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid()};
  if (sysctl(mib, 4, &info, &size, NULL, 0) == 0) {
    g_start = info.kp_proc.p_starttime.tv_sec * 1e6 + info.kp_proc.p_starttime.tv_usec;
  }
  const char *t0 = getenv("PS_PROBE_T0_US");
  if (t0 != NULL) {
    g_start = atof(t0);
  }
  atexit(report);
  const char *interval = getenv("PS_PROBE_SAMPLE_US");
  if (interval != NULL) {
    g_main_thread = mach_thread_self();
    g_sampling = 1;
    pthread_create(&g_sampler, NULL, sampler_main, (void *)(uintptr_t)atoi(interval));
  }
}

struct interpose {
  const void *replacement;
  const void *original;
};
__attribute__((used)) static const struct interpose g_interposers[] __attribute__((section("__DATA,__interpose"))) = {
    {(const void *)probe_init, (const void *)ps_ui_init},
    {(const void *)probe_window_create, (const void *)ps_ui_window_create},
    {(const void *)probe_window_show, (const void *)ps_ui_window_show},
    {(const void *)probe_add_tab, (const void *)ps_ui_window_add_tab},
    {(const void *)probe_set_toolbar, (const void *)ps_ui_window_set_toolbar},
    {(const void *)probe_set_preview, (const void *)ps_ui_window_set_preview},
    {(const void *)probe_set_preview_mode, (const void *)ps_ui_window_set_preview_mode},
    {(const void *)probe_text_view_create, (const void *)ps_ui_text_view_create},
    {(const void *)probe_load_file, (const void *)ps_ui_text_view_load_file},
    {(const void *)probe_highlight, (const void *)ps_ui_text_view_highlight},
    {(const void *)probe_wait_event, (const void *)ps_ui_wait_event},
    {(const void *)probe_list_count, (const void *)ps_ui_pref_list_count},
    {(const void *)probe_list_get, (const void *)ps_ui_pref_list_get},
};
