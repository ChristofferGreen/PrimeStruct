// macOS (AppKit) backend of the native UI C ABI (include/primec/ui/NativeUi.h,
// docs/NativeUiPlan.md). The program owns the loop: ps_ui_wait_event pumps the
// AppKit event queue until something the program should handle happens.
//
// Startup rules (docs/NativeUiPlan.md section 1): a plain NSWindow (no
// NSDocument, no state restoration), TextKit 2 through NSTextView's default
// stack, only AppKit and Foundation.
//
// Snapshot mode: when PRIMESTRUCT_UI_SNAPSHOT is set to a path, the first shown
// window is rendered into a PNG at that path once its layout has settled (no
// Screen Recording permission needed: the view hierarchy draws itself), and
// ps_ui_wait_event then reports a quit request. PRIMESTRUCT_UI_TYPE=text
// additionally types the text (with "\n" as a newline) into the first text view
// before the picture is taken, and then exits right after it, since the program
// would otherwise ask what to do with the unsaved text. PRIMESTRUCT_UI_OPEN=file
// loads a file into the first text view and highlights it (the window title
// becomes the path), and PRIMESTRUCT_UI_MENU=File pops that menu-bar menu open
// and draws its window into the picture (no Screen Recording permission needed).
// PRIMESTRUCT_UI_COMMANDS=1,1 delivers those menu command ids to the program
// first (for the editor, two New commands: two more tabs).

#import <AppKit/AppKit.h>
#import <objc/message.h>
#import <objc/runtime.h>

#include "primec/ui/Markdown.h"
#include "primec/ui/NativeUi.h"
#include "primec/ui/NativeUiRecentFiles.h"
#include "primec/ui/NativeUiStandardItems.h"
#include "primec/ui/SyntaxHighlight.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <dlfcn.h>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#import <os/log.h>

// Unified-log trace of the document and styling calls, off unless PRIMESTRUCT_UI_TRACE
// is set (it names the files being opened). Read it with
// `log show --predicate 'subsystem == "com.primestruct.ui"' --info`.
#define PS_TRACE(...)                                                                         \
  do {                                                                                        \
    static const bool traceOn = std::getenv("PRIMESTRUCT_UI_TRACE") != nullptr;               \
    if (traceOn) {                                                                            \
      static os_log_t traceLog = os_log_create("com.primestruct.ui", "trace");                \
      os_log(traceLog, "%{public}s", [[NSString stringWithFormat:__VA_ARGS__] UTF8String]);   \
    }                                                                                         \
  } while (0)

namespace {

struct PendingEvent {
  int32_t kind = PS_UI_EVENT_NONE;
  uint64_t window = 0;
  uint64_t widget = 0;
  int32_t command = 0;
};

// A control of a settings window (ps_ui_label_create and the others): its view, and the form or tabs that holds it.
struct ControlInfo {
  enum class Kind { Label, CheckBox, Popup, Form, Tabs } kind = Kind::Label;
  NSView *view = nil;
  uint64_t parent = 0;
};

struct State {
  bool initialized = false;
  bool quitting = false;
  std::thread::id owner;
  uint64_t nextHandle = 1;
  std::unordered_map<uint64_t, NSWindow *> windows;
  std::unordered_map<uint64_t, uint64_t> windowContent;
  std::unordered_map<uint64_t, std::string> windowPaths;
  // The markdown preview (ps_ui_window_set_preview): a split view of the text view and a web view per
  // window. WebKit is loaded when the first preview turns on and is never linked.
  std::unordered_map<uint64_t, bool> previews;
  std::unordered_map<uint64_t, NSSplitView *> previewSplits;
  std::unordered_map<uint64_t, NSView *> previewWebs;
  std::unordered_map<uint64_t, bool> previewLoading;
  std::unordered_map<uint64_t, int> previewScriptsPending; // scripts sent to the page that have not answered
  std::unordered_map<uint64_t, double> previewFraction;    // where both views were last put (0 top, 1 bottom)
  std::unordered_map<uint64_t, bool> previewSyncing;       // the page is moving the text view right now
  std::unordered_map<uint64_t, id> previewObservers;       // the observer of the text view's scrolling
  id previewEventMonitor = nil;                            // sees the events that scroll a page
  bool previewScrollLock = true;                           // the source and the page scroll together
  std::unordered_map<uint64_t, bool> previewRefreshPending;
  std::unordered_set<int32_t> previewMenuItems; // command ids whose check mark follows the active preview
  std::unordered_map<uint64_t, ControlInfo> controls;
  std::unordered_map<uint64_t, double> fontPoints; // the size set with ps_ui_text_view_set_font_size
  std::unordered_map<uint64_t, NSTextView *> textViews;
  std::unordered_map<uint64_t, NSFont *> baseFonts;
  // A text view does not retain its scroll view, so the maps do.
  std::unordered_map<uint64_t, NSScrollView *> scrollViews;
  std::unordered_map<uint64_t, NSMenu *> menus;
  std::unordered_map<uint64_t, bool> modified;
  // PRIMESTRUCT_UI_PREFS=key=1,other=0 (and every snapshot run) answers preferences from this map
  // and never reads or writes the user's defaults.
  bool prefOverridesActive = false;
  std::unordered_map<std::string, int32_t> intOverrides;
  std::unordered_map<std::string, std::vector<std::string>> listOverrides;
  std::vector<std::string> recentOverride; // the recent files while the overrides are active
  std::unordered_map<std::string, bool> prefOverrides;
  std::deque<PendingEvent> pending;
  PendingEvent current;
  std::string returnedText;
  std::string lastError;
  bool panelChosen = false;
  std::string snapshotPath;
  std::string snapshotTypeText;
  std::string snapshotOpenPath;
  std::string snapshotMenu;
  std::string snapshotScroll; // PRIMESTRUCT_UI_SCROLL=text:1 or preview:1: scroll one view, the other follows
  bool snapshotScrolled = false;
  std::string snapshotClicks; // PRIMESTRUCT_UI_CLICK=check:6;popup:8:3 clicks controls through their actions
  bool snapshotClicked = false;
  std::string snapshotCommands;
  bool snapshotCommandsSent = false;
  bool snapshotOpened = false;
  bool snapshotTyped = false;
  bool snapshotTaken = false;
  uint64_t snapshotWindow = 0;
  int snapshotIdleSlices = 0;
};

State &state() {
  static State instance;
  return instance;
}

const void *HandleKey = &HandleKey;

uint64_t handleOf(id object) {
  NSNumber *number = objc_getAssociatedObject(object, HandleKey);
  return number != nil ? number.unsignedLongLongValue : 0;
}

void tag(id object, uint64_t handle) {
  objc_setAssociatedObject(object, HandleKey, @(handle), OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}

NSString *toNSString(const char *text) {
  NSString *string = text != nullptr ? [NSString stringWithUTF8String:text] : @"";
  return string != nil ? string : @"";
}

bool usable() {
  const State &s = state();
  return s.initialized && s.owner == std::this_thread::get_id();
}

void enqueue(int32_t kind, uint64_t window = 0, uint64_t widget = 0, int32_t command = 0) {
  PendingEvent event;
  event.kind = kind;
  event.window = window;
  event.widget = widget;
  event.command = command;
  state().pending.push_back(event);
}

NSWindow *liveWindow(uint64_t handle) {
  auto it = state().windows.find(handle);
  return it != state().windows.end() ? it->second : nil;
}

NSTextView *textView(uint64_t handle) {
  auto it = state().textViews.find(handle);
  return it != state().textViews.end() ? it->second : nil;
}

NSMenu *menuFor(uint64_t handle) {
  auto it = state().menus.find(handle);
  return it != state().menus.end() ? it->second : nil;
}

// ---- controls of a settings window -----------------------------------------------------------------
// Labels, check boxes and pop-ups are the platform's own controls; a form is an NSGridView (labels in one
// column, controls in the next), tabs an NSTabView whose pages are forms. A control is held by one form or
// tabs, or is the content of a window.

constexpr CGFloat PageMargin = 20;
constexpr CGFloat TabsMargin = 16;

ControlInfo *controlInfo(uint64_t handle) {
  auto it = state().controls.find(handle);
  return it != state().controls.end() ? &it->second : nullptr;
}

ControlInfo *controlOfKind(uint64_t handle, ControlInfo::Kind kind) {
  ControlInfo *info = usable() ? controlInfo(handle) : nullptr;
  return info != nullptr && info->kind == kind ? info : nullptr;
}

// The window whose content holds the control (directly or through forms and tabs); 0 when none.
uint64_t windowOfControl(uint64_t handle) {
  State &s = state();
  uint64_t root = handle;
  for (int depth = 0; depth < 64; ++depth) {
    const ControlInfo *info = controlInfo(root);
    if (info == nullptr || info->parent == 0) {
      break;
    }
    root = info->parent;
  }
  for (const auto &entry : s.windowContent) {
    if (entry.second == root && s.windows.count(entry.first) != 0) {
      return entry.first;
    }
  }
  return 0;
}

// A control that may be put into a container: it exists and nothing holds it yet.
ControlInfo *freeControl(uint64_t handle) {
  ControlInfo *info = controlInfo(handle);
  return info != nullptr && info->parent == 0 && windowOfControl(handle) == 0 ? info : nullptr;
}

uint64_t createControl(ControlInfo::Kind kind, NSView *view) {
  State &s = state();
  const uint64_t handle = s.nextHandle++;
  tag(view, handle);
  ControlInfo info;
  info.kind = kind;
  info.view = view;
  s.controls[handle] = info;
  return handle;
}

// The content at the top left of a view, a margin away from its edges (the view takes its own size).
NSView *paddedView(NSView *content, CGFloat margin) {
  NSView *container = [[NSView alloc] initWithFrame:NSZeroRect];
  content.translatesAutoresizingMaskIntoConstraints = NO;
  [container addSubview:content];
  [NSLayoutConstraint activateConstraints:@[
    [content.topAnchor constraintEqualToAnchor:container.topAnchor constant:margin],
    [content.leadingAnchor constraintEqualToAnchor:container.leadingAnchor constant:margin],
  ]];
  return container;
}

// The content filling a view with a margin on all four sides.
NSView *insetView(NSView *content, CGFloat margin) {
  NSView *container = [[NSView alloc] initWithFrame:NSZeroRect];
  content.translatesAutoresizingMaskIntoConstraints = NO;
  [container addSubview:content];
  [NSLayoutConstraint activateConstraints:@[
    [content.topAnchor constraintEqualToAnchor:container.topAnchor constant:margin],
    [content.leadingAnchor constraintEqualToAnchor:container.leadingAnchor constant:margin],
    [container.trailingAnchor constraintEqualToAnchor:content.trailingAnchor constant:margin],
    [container.bottomAnchor constraintEqualToAnchor:content.bottomAnchor constant:margin],
  ]];
  return container;
}

// The room a control needs: its fitting size; tabs need their largest page plus the tab view's own chrome.
NSSize controlFittingSize(uint64_t handle) {
  const ControlInfo *info = controlInfo(handle);
  if (info == nullptr) {
    return NSZeroSize;
  }
  if (info->kind != ControlInfo::Kind::Tabs) {
    return info->view.fittingSize;
  }
  NSTabView *tabs = (NSTabView *)info->view;
  NSSize best = NSZeroSize;
  for (NSTabViewItem *item in tabs.tabViewItems) {
    const NSSize page = controlFittingSize(static_cast<uint64_t>([item.identifier unsignedLongLongValue]));    best.width = std::max(best.width, page.width + 2 * PageMargin);
    best.height = std::max(best.height, page.height + 2 * PageMargin);
  }
  const NSRect probe = NSMakeRect(0, 0, 400, 300);
  tabs.frame = probe;
  const NSRect inner = tabs.contentRect;
  return NSMakeSize(best.width + (probe.size.width - inner.size.width), best.height + (probe.size.height - inner.size.height));
}

// The view a window shows for a control that is its content.
NSView *windowContentFor(uint64_t handle) {
  ControlInfo *info = controlInfo(handle);
  if (info->kind == ControlInfo::Kind::Tabs) {
    return insetView(info->view, TabsMargin);
  }
  return paddedView(info->view, PageMargin);
}

// A settings window is as large as its content needs, and stays centered.
void fitSettingsWindow(NSWindow *w, uint64_t window) {
  State &s = state();
  const auto content = s.windowContent.find(window);
  const ControlInfo *root = content != s.windowContent.end() ? controlInfo(content->second) : nullptr;
  if (root == nullptr) {
    return;
  }
  NSSize size = controlFittingSize(content->second);
  const CGFloat margin = root->kind == ControlInfo::Kind::Tabs ? TabsMargin : PageMargin;
  size.width += 2 * margin;
  size.height += 2 * margin;
  [w setContentSize:size];
  [w center];
}

// Snapshot runs: "check:6" clicks check box 6, "popup:8:3" chooses item 3 of pop-up 8, through the controls'
// own actions (the program hears about each as a control event).
void applySnapshotClicks(const std::string &spec) {
  for (size_t start = 0; start < spec.size();) {
    size_t end = spec.find(';', start);
    end = end == std::string::npos ? spec.size() : end;
    const std::string entry = spec.substr(start, end - start);
    start = end + 1;
    const size_t first = entry.find(':');
    const std::string kind = entry.substr(0, first);
    const uint64_t handle = first == std::string::npos ? 0 : std::strtoull(entry.c_str() + first + 1, nullptr, 10);
    const size_t second = first == std::string::npos ? std::string::npos : entry.find(':', first + 1);
    const ControlInfo *info = controlInfo(handle);
    if (info == nullptr) {
      continue;
    }
    if (kind == "check" && info->kind == ControlInfo::Kind::CheckBox) {
      [(NSButton *)info->view performClick:nil];
    } else if (kind == "popup" && info->kind == ControlInfo::Kind::Popup && second != std::string::npos) {
      NSPopUpButton *popup = (NSPopUpButton *)info->view;
      [popup selectItemAtIndex:std::atoi(entry.c_str() + second + 1)];
      [NSApp sendAction:popup.action to:popup.target from:popup];
    }
  }
}

// ---- the markdown preview ------------------------------------------------------------------------
// WebKit is not linked: the first preview loads the framework and its classes are used by name, so a
// program that never previews neither loads WebKit nor pays for it at launch.

Class webKitClass(NSString *name) {
  static const bool loaded = dlopen("/System/Library/Frameworks/WebKit.framework/WebKit", RTLD_LAZY) != nullptr;
  return loaded ? NSClassFromString(name) : Nil;
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Warc-performSelector-leaks"

void loadPage(NSView *web, NSString *page) {
  [web performSelector:NSSelectorFromString(@"loadHTMLString:baseURL:") withObject:page withObject:nil];
}

void evaluateScript(NSView *web, NSString *script, void (^done)(id)) {
  [web performSelector:NSSelectorFromString(@"evaluateJavaScript:completionHandler:")
            withObject:script
            withObject:^(id result, NSError *error) {
              if (done != nil) {
                done(result);
              }
            }];
}

bool previewOn(uint64_t window) {
  const auto it = state().previews.find(window);
  return it != state().previews.end() && it->second;
}

// Scrolling is locked between the source and the page: both sit at the same fraction of the height they can
// scroll. The text view's scrolling is observed; the page's is noticed through the events that scroll it
// (the wheel, keys, the scroll bar), after which its position is read back.

NSScrollView *previewTextScroll(uint64_t window) {
  State &s = state();
  const auto content = s.windowContent.find(window);
  if (content == s.windowContent.end()) {
    return nil;
  }
  const auto scroll = s.scrollViews.find(content->second);
  return scroll != s.scrollViews.end() ? scroll->second : nil;
}

CGFloat textScrollRange(NSScrollView *scroll) {
  return scroll.documentView.frame.size.height - scroll.contentView.bounds.size.height;
}

double textScrollFraction(NSScrollView *scroll) {
  const CGFloat range = textScrollRange(scroll);
  return range > 0 ? std::clamp<double>(scroll.contentView.bounds.origin.y / range, 0, 1) : 0;
}

void setTextScrollFraction(NSScrollView *scroll, double fraction) {
  const CGFloat range = textScrollRange(scroll);
  if (range <= 0) {
    return;
  }
  NSPoint origin = scroll.contentView.bounds.origin;
  origin.y = std::clamp<double>(fraction, 0, 1) * range;
  [scroll.contentView scrollToPoint:origin];
  [scroll reflectScrolledClipView:scroll.contentView];
}

NSString *const WebFractionScript =
    @"(function(){var h=document.documentElement.scrollHeight-window.innerHeight;return h>0?window.scrollY/h:0;})()";

NSString *webScrollScript(double fraction) {
  return [NSString stringWithFormat:@"(function(){var h=document.documentElement.scrollHeight-window.innerHeight;"
                                    @"window.scrollTo(0,h>0?%f*h:0);})()",
                                    fraction];
}

// The page follows the text view.
void syncWebFromText(uint64_t window, bool force) {
  State &s = state();
  if (!s.previewScrollLock) {
    return;
  }
  NSScrollView *scroll = previewTextScroll(window);
  const auto web = s.previewWebs.find(window);
  if (scroll == nil || web == s.previewWebs.end()) {
    return;
  }
  const double fraction = textScrollFraction(scroll);
  const auto last = s.previewFraction.find(window);
  if (!force && last != s.previewFraction.end() && std::fabs(last->second - fraction) < 0.0005) {
    return;
  }
  s.previewFraction[window] = fraction;
  s.previewScriptsPending[window] += 1;
  evaluateScript(web->second, webScrollScript(fraction), ^(id) {
    state().previewScriptsPending[window] -= 1;
  });
}

// The text view follows the page: read where the page is, then move the text view there.
void syncTextFromWeb(uint64_t window) {
  State &s = state();
  if (!s.previewScrollLock) {
    return;
  }
  const auto web = s.previewWebs.find(window);
  if (web == s.previewWebs.end()) {
    return;
  }
  s.previewScriptsPending[window] += 1;
  evaluateScript(web->second, WebFractionScript, ^(id result) {
    State &now = state();
    now.previewScriptsPending[window] -= 1;
    NSScrollView *scroll = previewTextScroll(window);
    if (scroll == nil || ![result respondsToSelector:@selector(doubleValue)]) {
      return;
    }
    const double fraction = std::clamp<double>([result doubleValue], 0, 1);
    now.previewFraction[window] = fraction;
    now.previewSyncing[window] = true;
    setTextScrollFraction(scroll, fraction);
    now.previewSyncing[window] = false;
  });
}

// Navigation is the program's to refuse: the page the preview loads is allowed, a click on a link opens
// the address in the default browser, and nothing else may navigate the preview.
Class previewDelegateClass() {
  static Class created = [] {
    Class made = objc_allocateClassPair([NSObject class], "PSPreviewDelegate", 0);
    class_addMethod(made, NSSelectorFromString(@"webView:decidePolicyForNavigationAction:decisionHandler:"),
                    imp_implementationWithBlock(^(id, id, id action, void (^decide)(NSInteger)) {
                      const NSInteger type = [[action valueForKey:@"navigationType"] integerValue];
                      NSURL *url = [[action valueForKey:@"request"] URL];
                      if (type == -1 || [url.absoluteString hasPrefix:@"about:blank"]) {
                        decide(1);
                        return;
                      }
                      decide(0);
                      NSString *scheme = url.scheme.lowercaseString;
                      if ([scheme isEqualToString:@"http"] || [scheme isEqualToString:@"https"] ||
                          [scheme isEqualToString:@"mailto"]) {
                        [[NSWorkspace sharedWorkspace] openURL:url];
                      }
                    }),
                    "v@:@@@?");
    // A page that has loaded goes to where the text view is.
    class_addMethod(made, NSSelectorFromString(@"webView:didFinishNavigation:"),
                    imp_implementationWithBlock(^(id, id web, id) {
                      const uint64_t window = handleOf(web);
                      state().previewLoading[window] = false;
                      syncWebFromText(window, true);
                    }),
                    "v@:@@");
    for (NSString *failed in @[ @"webView:didFailNavigation:withError:",
                                @"webView:didFailProvisionalNavigation:withError:" ]) {
      class_addMethod(made, NSSelectorFromString(failed),
                      imp_implementationWithBlock(^(id, id web, id, id) { state().previewLoading[handleOf(web)] = false; }),
                      "v@:@@@");
    }
    objc_registerClassPair(made);
    return made;
  }();
  return created;
}

bool anyPreviewLoading() {
  State &s = state();
  for (const auto &entry : s.previewLoading) {
    if (entry.second) {
      return true;
    }
  }
  for (const auto &entry : s.previewScriptsPending) {
    if (entry.second > 0) {
      return true;
    }
  }
  return false;
}

// Builds the page of the window's text and loads it into its web view.
void renderPreview(uint64_t window) {
  State &s = state();
  const auto web = s.previewWebs.find(window);
  const auto content = s.windowContent.find(window);
  NSTextView *view = content != s.windowContent.end() ? textView(content->second) : nil;
  if (web == s.previewWebs.end() || view == nil) {
    return;
  }
  const char *text = view.string.UTF8String;
  s.previewLoading[window] = true;
  loadPage(web->second, toNSString(primec::ui::markdownPreviewPage(text != nullptr ? text : "").c_str()));
}

// One monitor for every preview: a wheel turn, a key or a drag over a page may have scrolled it, so the text
// view is brought to where the page is (again after the page's own scrolling animation, for keys).
void ensurePreviewEventMonitor() {
  State &s = state();
  if (s.previewEventMonitor != nil) {
    return;
  }
  const NSEventMask mask =
      NSEventMaskScrollWheel | NSEventMaskKeyDown | NSEventMaskLeftMouseDragged | NSEventMaskLeftMouseUp;
  s.previewEventMonitor = [NSEvent addLocalMonitorForEventsMatchingMask:mask
                                                                handler:^NSEvent *(NSEvent *event) {
                                                                  State &now = state();
                                                                  const uint64_t window = handleOf(event.window);
                                                                  const auto web = now.previewWebs.find(window);
                                                                  if (web == now.previewWebs.end()) {
                                                                    return event;
                                                                  }
                                                                  NSView *view = web->second;
                                                                  bool relevant = false;
                                                                  if (event.type == NSEventTypeKeyDown) {
                                                                    NSResponder *responder = event.window.firstResponder;
                                                                    relevant = [responder isKindOfClass:[NSView class]] &&
                                                                               [(NSView *)responder isDescendantOf:view];
                                                                  } else {
                                                                    const NSPoint point =
                                                                        [view.superview convertPoint:event.locationInWindow
                                                                                            fromView:nil];
                                                                    relevant = [view hitTest:point] != nil;
                                                                  }
                                                                  if (relevant) {
                                                                    for (const int64_t delay : {30, 250}) {
                                                                      dispatch_after(
                                                                          dispatch_time(DISPATCH_TIME_NOW, delay * NSEC_PER_MSEC),
                                                                          dispatch_get_main_queue(), ^{
                                                                            if (previewOn(window)) {
                                                                              syncTextFromWeb(window);
                                                                            }
                                                                          });
                                                                    }
                                                                  }
                                                                  return event;
                                                                }];
}

void dropPreviewState(uint64_t window) {
  State &s = state();
  const auto web = s.previewWebs.find(window);
  if (web != s.previewWebs.end()) {
    [web->second setValue:nil forKey:@"navigationDelegate"];
  }
  const auto observer = s.previewObservers.find(window);
  if (observer != s.previewObservers.end()) {
    [NSNotificationCenter.defaultCenter removeObserver:observer->second];
  }
  s.previews.erase(window);
  s.previewSplits.erase(window);
  s.previewWebs.erase(window);
  s.previewLoading.erase(window);
  s.previewScriptsPending.erase(window);
  s.previewFraction.erase(window);
  s.previewSyncing.erase(window);
  s.previewObservers.erase(window);
  s.previewRefreshPending.erase(window);
  if (s.previews.empty() && s.previewEventMonitor != nil) {
    [NSEvent removeMonitor:s.previewEventMonitor];
    s.previewEventMonitor = nil;
  }
}

// Scrolls one of the two views to a fraction, "text:0.5" or "preview:1", the way the user would; the other
// view follows (snapshot runs use this to show the lock).
void applySnapshotScroll(uint64_t window, const std::string &spec) {
  State &s = state();
  const size_t colon = spec.find(':');
  const std::string side = spec.substr(0, colon);
  const double fraction = colon == std::string::npos ? 1.0 : std::atof(spec.c_str() + colon + 1);
  if (side == "text") {
    if (NSScrollView *scroll = previewTextScroll(window)) {
      setTextScrollFraction(scroll, fraction);
    }
  } else if (side == "preview") {
    const auto web = s.previewWebs.find(window);
    if (web != s.previewWebs.end()) {
      s.previewScriptsPending[window] += 1;
      evaluateScript(web->second, webScrollScript(fraction), ^(id) {
        state().previewScriptsPending[window] -= 1;
        syncTextFromWeb(window);
      });
    }
  }
}

// A web view does not draw itself into an offscreen bitmap: ask it for a snapshot of its own and put that
// where the web view sits in the window's picture.
void drawPreviewWeb(uint64_t window, NSView *view, NSBitmapImageRep *rep) {
  State &s = state();
  const auto found = s.previewWebs.find(window);
  Class configurationClass = webKitClass(@"WKSnapshotConfiguration");
  if (found == s.previewWebs.end() || configurationClass == Nil) {
    return;
  }
  NSView *web = found->second;
  __block NSImage *image = nil;
  __block BOOL done = NO;
  id configuration = [[configurationClass alloc] init];
  [web performSelector:NSSelectorFromString(@"takeSnapshotWithConfiguration:completionHandler:")
            withObject:configuration
            withObject:^(NSImage *taken, NSError *error) {
              image = taken;
              done = YES;
            }];
  NSDate *deadline = [NSDate dateWithTimeIntervalSinceNow:5];
  while (!done && deadline.timeIntervalSinceNow > 0) {
    [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.05]];
  }
  if (image == nil) {
    std::fprintf(stderr, "native ui: the preview could not be drawn into the snapshot\n");
    return;
  }
  const NSRect place = [web convertRect:web.bounds toView:view];
  NSGraphicsContext *context = [NSGraphicsContext graphicsContextWithBitmapImageRep:rep];
  [NSGraphicsContext saveGraphicsState];
  NSGraphicsContext.currentContext = context;
  [image drawInRect:place];
  [NSGraphicsContext restoreGraphicsState];
}

#pragma clang diagnostic pop

} // namespace

@interface PSController : NSObject <NSApplicationDelegate, NSWindowDelegate, NSTextViewDelegate>
- (void)menuCommand:(id)sender;
- (void)controlChanged:(id)sender;
- (void)quitRequested:(id)sender;
@end

@implementation PSController
- (void)menuCommand:(id)sender {
  enqueue(PS_UI_EVENT_COMMAND, 0, 0, static_cast<int32_t>([sender tag]));
}

- (void)quitRequested:(id)sender {
  enqueue(PS_UI_EVENT_QUIT_REQUESTED);
}

// The user clicked a check box or chose an item of a pop-up.
- (void)controlChanged:(id)sender {
  const uint64_t control = handleOf(sender);
  enqueue(PS_UI_EVENT_CONTROL_CHANGED, windowOfControl(control), control);
}

// A menu item registered with ps_ui_menu_follow_preview shows whether the active window has a preview.
- (BOOL)validateMenuItem:(NSMenuItem *)item {
  if (state().previewMenuItems.count(static_cast<int32_t>(item.tag)) != 0) {
    item.state = ps_ui_window_has_preview(ps_ui_active_window()) ? NSControlStateValueOn : NSControlStateValueOff;
  }
  return YES;
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender {
  enqueue(PS_UI_EVENT_QUIT_REQUESTED);
  return NSTerminateCancel;
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
  return NO;
}

- (BOOL)windowShouldClose:(NSWindow *)sender {
  enqueue(PS_UI_EVENT_WINDOW_CLOSE_REQUESTED, handleOf(sender));
  return NO;
}

- (void)textDidChange:(NSNotification *)notification {
  NSTextView *view = notification.object;
  const uint64_t widget = handleOf(view);
  PS_TRACE(@"textDidChange view=%llu window=%llu", widget, handleOf(view.window));
  state().modified[widget] = true;
  enqueue(PS_UI_EVENT_TEXT_CHANGED, handleOf(view.window), widget);
}
@end

namespace {

PSController *controller() {
  static PSController *instance = [[PSController alloc] init];
  return instance;
}

// Splits "cmd+shift+s" into a key equivalent and modifier mask; "cmd" is
// Command on macOS.
void parseShortcut(const char *shortcut, NSString *__strong *key, NSEventModifierFlags *mask) {
  *key = @"";
  *mask = 0;
  std::string text = shortcut != nullptr ? shortcut : "";
  size_t start = 0;
  while (start <= text.size()) {
    size_t plus = text.find('+', start);
    if (plus == std::string::npos) {
      plus = text.size();
    }
    std::string part = text.substr(start, plus - start);
    for (char &c : part) {
      c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    const bool last = plus >= text.size();
    if (part == "cmd" && !last) {
      *mask |= NSEventModifierFlagCommand;
    } else if (part == "shift" && !last) {
      *mask |= NSEventModifierFlagShift;
    } else if ((part == "alt" || part == "opt") && !last) {
      *mask |= NSEventModifierFlagOption;
    } else if (part == "ctrl" && !last) {
      *mask |= NSEventModifierFlagControl;
    } else if (!part.empty()) {
      *key = toNSString(part.c_str());
    }
    start = plus + 1;
  }
}

NSMenuItem *addMenuItem(NSMenu *menu, NSString *title, SEL action, id target, NSString *key,
                        NSEventModifierFlags mask, NSInteger itemTag) {
  NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:title action:action keyEquivalent:key];
  item.keyEquivalentModifierMask = mask;
  item.target = target;
  item.tag = itemTag;
  [menu addItem:item];
  return item;
}

void buildApplicationMenu(NSString *appName) {
  NSMenu *mainMenu = [[NSMenu alloc] initWithTitle:@""];
  NSMenuItem *appItem = [[NSMenuItem alloc] initWithTitle:appName action:nil keyEquivalent:@""];
  NSMenu *appMenu = [[NSMenu alloc] initWithTitle:appName];
  addMenuItem(appMenu, [@"About " stringByAppendingString:appName], @selector(orderFrontStandardAboutPanel:), NSApp,
              @"", 0, 0);
  [appMenu addItem:[NSMenuItem separatorItem]];
  addMenuItem(appMenu, [@"Quit " stringByAppendingString:appName], @selector(quitRequested:), controller(), @"q",
              NSEventModifierFlagCommand, 0);
  appItem.submenu = appMenu;
  [mainMenu addItem:appItem];
  NSApp.mainMenu = mainMenu;
}

NSView *findViewOfClass(NSView *root, NSString *className) {
  if (root == nil) {
    return nil;
  }
  if ([NSStringFromClass([root class]) isEqualToString:className]) {
    return root;
  }
  for (NSView *child in root.subviews) {
    if (NSView *found = findViewOfClass(child, className)) {
      return found;
    }
  }
  return nil;
}

void collectViewsOfClass(NSView *root, NSString *className, std::vector<NSView *> &out) {
  if (root == nil) {
    return;
  }
  if ([NSStringFromClass([root class]) isEqualToString:className]) {
    out.push_back(root);
  }
  for (NSView *child in root.subviews) {
    collectViewsOfClass(child, className, out);
  }
}

// The visible window of the pop-up menu being tracked, if any.
NSWindow *openMenuWindow(NSWindow *except) {
  for (NSWindow *candidate in NSApp.windows) {
    if (candidate != except && candidate.isVisible &&
        [NSStringFromClass([candidate class]) containsString:@"Menu"]) {
      return candidate;
    }
  }
  return nil;
}

void dumpViews(NSView *view, int depth);

void captureSnapshot(NSWindow *menuWindow = nil) {
  State &s = state();
  NSWindow *window = liveWindow(s.snapshotWindow);
  s.snapshotTaken = true;
  if (window == nil) {
    return;
  }
  NSView *view = window.contentView.superview != nil ? window.contentView.superview : window.contentView;
  NSRect bounds = view.bounds;
  if (bounds.size.width <= 0 || bounds.size.height <= 0) {
    std::fprintf(stderr, "native ui: frame view is %gx%g; window frame %gx%g, content %s %gx%g\n",
                 bounds.size.width, bounds.size.height, window.frame.size.width, window.frame.size.height,
                 window.contentView != nil ? NSStringFromClass([window.contentView class]).UTF8String : "nil",
                 window.contentView.bounds.size.width, window.contentView.bounds.size.height);
    view = window.contentView;
    bounds = view.bounds;
  }
  NSBitmapImageRep *rep = [view bitmapImageRepForCachingDisplayInRect:bounds];
  if (rep == nil) {
    std::fprintf(stderr, "native ui: no bitmap for a %gx%g view\n", bounds.size.width, bounds.size.height);
    return;
  }
  if (std::getenv("PRIMESTRUCT_UI_DUMP_WINDOW") != nullptr) {
    dumpViews(view, 0);
  }
  [view cacheDisplayInRect:bounds toBitmapImageRep:rep];
  drawPreviewWeb(s.snapshotWindow, view, rep);
  if (menuWindow != nil) {
    // An open menu is a window of its own: draw it over the picture at its place.
    // The glass backdrop of a pop-up menu does not draw offscreen, but its item
    // table does; the backdrop is filled in below.
    NSView *menuView = findViewOfClass(menuWindow.contentView.superview, @"NSMenuScrollView");
    if (menuView == nil) {
      menuView = menuWindow.contentView;
    }
    NSBitmapImageRep *menuRep = [menuView bitmapImageRepForCachingDisplayInRect:menuView.bounds];
    if (menuRep != nil) {
      [menuView cacheDisplayInRect:menuView.bounds toBitmapImageRep:menuRep];
      NSBitmapImageRep *combined = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL
                                                                           pixelsWide:rep.pixelsWide
                                                                           pixelsHigh:rep.pixelsHigh
                                                                        bitsPerSample:8
                                                                      samplesPerPixel:4
                                                                             hasAlpha:YES
                                                                             isPlanar:NO
                                                                       colorSpaceName:NSCalibratedRGBColorSpace
                                                                          bytesPerRow:0
                                                                         bitsPerPixel:0];
      combined.size = rep.size;
      NSGraphicsContext *context = [NSGraphicsContext graphicsContextWithBitmapImageRep:combined];
      [NSGraphicsContext saveGraphicsState];
      NSGraphicsContext.currentContext = context;
      [rep drawInRect:NSMakeRect(0, 0, rep.size.width, rep.size.height)];
      const NSRect frame = menuWindow.frame;
      const NSRect place = NSMakeRect(frame.origin.x - window.frame.origin.x, frame.origin.y - window.frame.origin.y,
                                      frame.size.width, frame.size.height);
      NSBezierPath *backdrop = [NSBezierPath bezierPathWithRoundedRect:place xRadius:10 yRadius:10];
      [[NSColor colorWithWhite:0.24 alpha:1.0] setFill];
      [backdrop fill];
      [[NSColor colorWithWhite:0.4 alpha:1.0] setStroke];
      backdrop.lineWidth = 1;
      [backdrop stroke];
      // Each row draws itself (text, shortcut, separator) on the backdrop.
      std::vector<NSView *> rows;
      collectViewsOfClass(menuWindow.contentView.superview, @"NSContextMenuItemView", rows);
      for (NSView *row : rows) {
        NSBitmapImageRep *rowRep = [row bitmapImageRepForCachingDisplayInRect:row.bounds];
        if (rowRep == nil) {
          continue;
        }
        [row cacheDisplayInRect:row.bounds toBitmapImageRep:rowRep];
        const NSRect inWindow = [row convertRect:row.bounds toView:nil];
        [rowRep drawInRect:NSMakeRect(place.origin.x + inWindow.origin.x, place.origin.y + inWindow.origin.y,
                                      inWindow.size.width, inWindow.size.height)
                  fromRect:NSZeroRect
                 operation:NSCompositingOperationSourceOver
                  fraction:1.0
            respectFlipped:YES
                     hints:nil];
      }
      [NSGraphicsContext restoreGraphicsState];
      rep = combined;
    }
  }
  NSData *png = [rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
  NSString *path = toNSString(s.snapshotPath.c_str());
  NSError *error = nil;
  if (png == nil || ![png writeToFile:path options:NSDataWritingAtomic error:&error]) {
    std::fprintf(stderr, "native ui: could not write snapshot %s: %s\n", s.snapshotPath.c_str(),
                 error != nil ? error.localizedDescription.UTF8String : "no PNG data");
  }
}

void dumpViews(NSView *view, int depth) {
  if (view == nil || depth > 8) {
    return;
  }
  std::fprintf(stderr, "%*s%s %g,%g %gx%g\n", depth * 2, "", NSStringFromClass([view class]).UTF8String,
               view.frame.origin.x, view.frame.origin.y, view.frame.size.width, view.frame.size.height);
  for (NSView *child in view.subviews) {
    dumpViews(child, depth + 1);
  }
}

// Snapshot of the window with the named menu-bar menu open.
void captureWithOpenMenu(const std::string &name) {
  State &s = state();
  NSWindow *window = liveWindow(s.snapshotWindow);
  NSMenuItem *item = [NSApp.mainMenu itemWithTitle:toNSString(name.c_str())];
  NSMenu *menu = item.submenu;
  if (window == nil || menu == nil) {
    std::fprintf(stderr, "native ui: no menu named %s; taking the plain snapshot\n", name.c_str());
    captureSnapshot();
    return;
  }
  NSTimer *timer = [NSTimer timerWithTimeInterval:1.0
                                          repeats:NO
                                            block:^(NSTimer *) {
                                              NSWindow *open = openMenuWindow(window);
                                              if (open == nil) {
                                                std::fprintf(stderr, "native ui: the menu window was not found\n");
                                              }
                                              if (std::getenv("PRIMESTRUCT_UI_DUMP_VIEWS") != nullptr && open != nil) {
                                                std::fprintf(stderr, "menu window %s frame %g,%g %gx%g\n",
                                                             NSStringFromClass([open class]).UTF8String,
                                                             open.frame.origin.x, open.frame.origin.y,
                                                             open.frame.size.width, open.frame.size.height);
                                                dumpViews(open.contentView.superview, 0);
                                              }
                                              captureSnapshot(open);
                                              [menu cancelTracking];
                                            }];
  [[NSRunLoop currentRunLoop] addTimer:timer forMode:NSRunLoopCommonModes];
  NSView *content = window.contentView;
  // Just below the top edge of the content, whichever way its y axis points.
  const CGFloat top = content.isFlipped ? 2 : content.bounds.size.height - 2;
  [menu popUpMenuPositioningItem:nil atLocation:NSMakePoint(8, top) inView:content];
  if (!s.snapshotTaken) {
    // Tracking ended before the timer fired (the window server closed the menu, for
    // instance because the user switched apps): still leave a picture of the window.
    std::fprintf(stderr, "native ui: the menu closed early; taking the plain snapshot\n");
    captureSnapshot();
  }
}

} // namespace

static std::vector<std::string> recentFilePaths() {
  State &s = state();
  if (s.prefOverridesActive) {
    return s.recentOverride;
  }
  // The list is kept in the app's own defaults: the system recent documents are tied to the app's
  // code signature and bundle, so they are lost whenever the app is replaced by an update.
  std::vector<std::string> paths;
  for (NSString *path in [NSUserDefaults.standardUserDefaults stringArrayForKey:@"ps.recent.files"]) {
    paths.push_back(path.UTF8String);
  }
  return paths;
}

static void storeRecentFilePaths(const std::vector<std::string> &paths) {
  NSMutableArray<NSString *> *list = [NSMutableArray array];
  for (const std::string &path : paths) {
    [list addObject:toNSString(path.c_str())];
  }
  [NSUserDefaults.standardUserDefaults setObject:list forKey:@"ps.recent.files"];
}

// The bar of line numbers to the left of a text view: one number per paragraph (the layout fragments
// of TextKit 2, so wrapped lines carry none), drawn for the visible part only. Counting the
// newlines before the first visible paragraph is cached between redraws and dropped on every edit
// of the characters.
@interface PSLineNumberRuler : NSRulerView
- (instancetype)initForTextView:(NSTextView *)textView scrollView:(NSScrollView *)scrollView;
@end

@implementation PSLineNumberRuler {
  __weak NSTextView *_textView;
  NSUInteger _cachedOffset;
  NSUInteger _cachedNewlines;
  NSUInteger _totalLines;
}

static NSUInteger countNewlines(NSString *text, NSUInteger from, NSUInteger to) {
  unichar buffer[2048];
  NSUInteger count = 0;
  while (from < to) {
    const NSUInteger chunk = std::min<NSUInteger>(2048, to - from);
    [text getCharacters:buffer range:NSMakeRange(from, chunk)];
    for (NSUInteger i = 0; i < chunk; ++i) {
      count += buffer[i] == '\n' ? 1 : 0;
    }
    from += chunk;
  }
  return count;
}

- (instancetype)initForTextView:(NSTextView *)textView scrollView:(NSScrollView *)scrollView {
  self = [super initWithScrollView:scrollView orientation:NSVerticalRuler];
  if (self != nil) {
    _textView = textView;
    self.clientView = textView;
    NSNotificationCenter *center = NSNotificationCenter.defaultCenter;
    [center addObserver:self
               selector:@selector(storageEdited:)
                   name:NSTextStorageDidProcessEditingNotification
                 object:textView.textStorage];
    scrollView.contentView.postsBoundsChangedNotifications = YES;
    [center addObserver:self
               selector:@selector(scrolled:)
                   name:NSViewBoundsDidChangeNotification
                 object:scrollView.contentView];
    [self recount];
  }
  return self;
}

- (void)dealloc {
  [NSNotificationCenter.defaultCenter removeObserver:self];
}

- (void)recount {
  NSString *text = _textView.string;
  _cachedOffset = 0;
  _cachedNewlines = 0;
  _totalLines = 1 + countNewlines(text, 0, text.length);
  NSUInteger digits = 1;
  for (NSUInteger lines = _totalLines; lines >= 10; lines /= 10) {
    ++digits;
  }
  NSFont *font = [NSFont monospacedDigitSystemFontOfSize:11 weight:NSFontWeightRegular];
  const CGFloat digitWidth = [@"0" sizeWithAttributes:@{NSFontAttributeName : font}].width;
  const CGFloat thickness = std::max<CGFloat>(32, std::ceil(digitWidth * std::max<NSUInteger>(digits, 2) + 18));
  if (thickness != self.ruleThickness) {
    self.ruleThickness = thickness;
  }
  self.needsDisplay = YES;
}

- (void)storageEdited:(NSNotification *)note {
  NSTextStorage *storage = note.object;
  if ((storage.editedMask & NSTextStorageEditedCharacters) != 0) {
    [self recount];
  }
}

- (void)scrolled:(NSNotification *)note {
  self.needsDisplay = YES;
}

- (NSUInteger)newlinesBefore:(NSUInteger)offset text:(NSString *)text {
  if (offset < _cachedOffset) {
    _cachedOffset = 0;
    _cachedNewlines = 0;
  }
  _cachedNewlines += countNewlines(text, _cachedOffset, offset);
  _cachedOffset = offset;
  return _cachedNewlines;
}

- (void)drawHashMarksAndLabelsInRect:(NSRect)rect {
  NSTextView *textView = _textView;
  NSTextLayoutManager *layout = textView.textLayoutManager;
  // Only the bar's own strip: the dirty rect can be larger than the ruler (a snapshot of an ancestor).
  const NSRect strip = NSIntersectionRect(rect, self.bounds);
  [NSColor.controlBackgroundColor setFill];
  NSRectFill(strip);
  [NSColor.separatorColor setFill];
  NSRectFill(NSMakeRect(NSMaxX(self.bounds) - 1, strip.origin.y, 1, strip.size.height));
  if (textView == nil || layout == nil) {
    return;
  }
  NSFont *font = [NSFont monospacedDigitSystemFontOfSize:11 weight:NSFontWeightRegular];
  NSDictionary *attributes = @{NSFontAttributeName : font, NSForegroundColorAttributeName : NSColor.secondaryLabelColor};
  NSString *text = textView.string;
  const NSPoint origin = textView.textContainerOrigin;
  const CGFloat visibleBottom = NSMaxY(textView.visibleRect);
  NSTextRange *viewport = layout.textViewportLayoutController.viewportRange;
  id<NSTextLocation> start = viewport != nil ? viewport.location : layout.documentRange.location;
  const NSInteger startOffset = [layout offsetFromLocation:layout.documentRange.location toLocation:start];
  __block NSUInteger line = 1 + [self newlinesBefore:(NSUInteger)std::max<NSInteger>(startOffset, 0) text:text];
  __block CGFloat lastBottom = origin.y;
  __block CGFloat lastTop = origin.y;
  __block NSTextLineFragment *lastLine = nil;
  __block BOOL reachedEnd = YES;
  void (^drawNumber)(NSUInteger, CGFloat, CGFloat) = ^(NSUInteger number, CGFloat top, CGFloat height) {
    NSString *label = [NSString stringWithFormat:@"%lu", (unsigned long)number];
    const NSSize size = [label sizeWithAttributes:attributes];
    const CGFloat y = [self convertPoint:NSMakePoint(0, top) fromView:textView].y;
    if (y + height < rect.origin.y || y > NSMaxY(rect)) {
      return;
    }
    [label drawAtPoint:NSMakePoint(NSMaxX(self.bounds) - 7 - size.width, y + (height - size.height) / 2)
        withAttributes:attributes];
  };
  // The viewport controller has laid out what is visible; forcing layout from a draw would disturb it.
  const NSTextLayoutFragmentEnumerationOptions options =
      viewport != nil ? NSTextLayoutFragmentEnumerationOptionsNone : NSTextLayoutFragmentEnumerationOptionsEnsuresLayout;
  [layout enumerateTextLayoutFragmentsFromLocation:start
                                           options:options
                                        usingBlock:^BOOL(NSTextLayoutFragment *fragment) {
                                          const CGRect frame = fragment.layoutFragmentFrame;
                                          const CGFloat top = frame.origin.y + origin.y;
                                          if (top > visibleBottom) {
                                            reachedEnd = NO;
                                            return NO;
                                          }
                                          NSTextLineFragment *first = fragment.textLineFragments.firstObject;
                                          const CGFloat height =
                                              first != nil ? first.typographicBounds.size.height : frame.size.height;
                                          drawNumber(line, top, height);
                                          ++line;
                                          lastBottom = top + frame.size.height;
                                          lastTop = top;
                                          lastLine = fragment.textLineFragments.lastObject;
                                          return YES;
                                        }];
  // The empty last paragraph after a final newline is the empty last line fragment of the paragraph
  // before it (or, in an empty text, the first line).
  if (reachedEnd && line <= _totalLines) {
    CGFloat top = lastBottom;
    CGFloat height = font.pointSize + 4;
    if (lastLine != nil && lastLine.characterRange.length == 0) {
      top = lastTop + lastLine.typographicBounds.origin.y;
      height = lastLine.typographicBounds.size.height;
    }
    if (top <= visibleBottom) {
      drawNumber(line, top, height);
    }
  }
}

@end

extern "C" {

int32_t ps_ui_abi_version(void) { return PS_UI_ABI_VERSION; }

bool ps_ui_init(const char *appName) {
  State &s = state();
  if (s.initialized) {
    return false;
  }
  @autoreleasepool {
    if (const char *path = std::getenv("PRIMESTRUCT_UI_SNAPSHOT"); path != nullptr && *path != '\0') {
      s.snapshotPath = path;
    }
    if (const char *open = std::getenv("PRIMESTRUCT_UI_OPEN"); open != nullptr && *open != '\0') {
      s.snapshotOpenPath = open;
    }
    if (const char *commands = std::getenv("PRIMESTRUCT_UI_COMMANDS"); commands != nullptr && *commands != '\0') {
      s.snapshotCommands = commands;
    }
    if (const char *menu = std::getenv("PRIMESTRUCT_UI_MENU"); menu != nullptr && *menu != '\0') {
      s.snapshotMenu = menu;
    }
    if (const char *scroll = std::getenv("PRIMESTRUCT_UI_SCROLL"); scroll != nullptr && *scroll != '\0') {
      s.snapshotScroll = scroll;
    }
    if (const char *click = std::getenv("PRIMESTRUCT_UI_CLICK"); click != nullptr && *click != '\0') {
      s.snapshotClicks = click;
    }
    if (const char *typed = std::getenv("PRIMESTRUCT_UI_TYPE"); typed != nullptr && *typed != '\0') {
      s.snapshotTypeText = typed;
      for (size_t at = s.snapshotTypeText.find("\\n"); at != std::string::npos;
           at = s.snapshotTypeText.find("\\n", at + 1)) {
        s.snapshotTypeText.replace(at, 2, "\n");
      }
    }
    if (const char *prefs = std::getenv("PRIMESTRUCT_UI_PREFS"); prefs != nullptr) {
      s.prefOverridesActive = true;
      const std::string list = prefs;
      for (size_t start = 0; start < list.size();) {
        size_t comma = list.find(',', start);
        comma = comma == std::string::npos ? list.size() : comma;
        const std::string item = list.substr(start, comma - start);
        const size_t equals = item.find('=');
        if (equals != std::string::npos) {
          s.prefOverrides[item.substr(0, equals)] = item.substr(equals + 1) == "1";
          s.intOverrides[item.substr(0, equals)] = std::atoi(item.substr(equals + 1).c_str());
        }
        start = comma + 1;
      }
    }
    if (const char *recent = std::getenv("PRIMESTRUCT_UI_RECENT"); recent != nullptr) {
      s.prefOverridesActive = true;
      const std::string list = recent;
      for (size_t start = 0; start < list.size();) {
        size_t colon = list.find(':', start);
        colon = colon == std::string::npos ? list.size() : colon;
        if (colon > start) {
          s.recentOverride.push_back(list.substr(start, colon - start));
        }
        start = colon + 1;
      }
    }
    if (!s.snapshotPath.empty()) {
      s.prefOverridesActive = true;
    }
    [NSApplication sharedApplication];
    // Snapshot runs stay out of the user's way: no Dock icon, no activation.
    [NSApp setActivationPolicy:s.snapshotPath.empty() ? NSApplicationActivationPolicyRegular
                                                      : NSApplicationActivationPolicyAccessory];
    NSApp.delegate = controller();
    buildApplicationMenu(toNSString(appName));
    [NSApp finishLaunching];
  }
  s.initialized = true;
  s.owner = std::this_thread::get_id();
  return true;
}

void ps_ui_quit(void) {
  if (usable()) {
    state().quitting = true;
  }
}

int32_t ps_ui_platform(void) { return PS_UI_PLATFORM_MACOS; }

int32_t ps_ui_wait_event(void) {
  if (!usable()) {
    return PS_UI_EVENT_NONE;
  }
  State &s = state();
  s.current = PendingEvent{};
  for (;;) {
    if (!s.pending.empty()) {
      s.current = s.pending.front();
      s.pending.pop_front();
      return s.current.kind;
    }
    if (s.quitting) {
      s.current.kind = PS_UI_EVENT_QUIT_REQUESTED;
      return s.current.kind;
    }
    @autoreleasepool {
      const bool snapshotting = !s.snapshotPath.empty() && !s.snapshotTaken && s.snapshotWindow != 0;
      NSDate *until = snapshotting ? [NSDate dateWithTimeIntervalSinceNow:0.05] : [NSDate distantFuture];
      NSEvent *event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                          untilDate:until
                                             inMode:NSDefaultRunLoopMode
                                            dequeue:YES];
      if (event != nil) {
        [NSApp sendEvent:event];
      }
      [NSApp updateWindows];
      if (snapshotting && event == nil) {
        ++s.snapshotIdleSlices;
        if (!s.snapshotCommands.empty() && !s.snapshotCommandsSent && s.snapshotIdleSlices >= 2) {
          // Menu commands as if chosen by the user (the program's own handling runs).
          s.snapshotCommandsSent = true;
          s.snapshotIdleSlices = 0;
          size_t start = 0;
          while (start < s.snapshotCommands.size()) {
            size_t comma = s.snapshotCommands.find(',', start);
            comma = comma == std::string::npos ? s.snapshotCommands.size() : comma;
            enqueue(PS_UI_EVENT_COMMAND, 0, 0, std::atoi(s.snapshotCommands.substr(start, comma - start).c_str()));
            start = comma + 1;
          }
        } else if (!s.snapshotOpenPath.empty() && !s.snapshotOpened && s.snapshotIdleSlices >= 4) {
          // Opens a file into the first text view, as the program would on Open.
          s.snapshotOpened = true;
          s.snapshotIdleSlices = 0;
          uint64_t chosen = UINT64_MAX;
          const bool lastTab = std::getenv("PRIMESTRUCT_UI_TARGET_LAST") != nullptr;
          for (const auto &entry : s.textViews) {
            if (chosen == UINT64_MAX || (lastTab ? entry.first > chosen : entry.first < chosen)) {
              chosen = entry.first;
            }
          }
          if (chosen != UINT64_MAX) {
            if (!ps_ui_text_view_load_file(chosen, s.snapshotOpenPath.c_str())) {
              std::fprintf(stderr, "native ui: %s\n", s.lastError.c_str());
            }
            ps_ui_text_view_highlight(chosen, s.snapshotOpenPath.c_str());
            const uint64_t window = handleOf(textView(chosen).window);
            ps_ui_window_set_title(window, s.snapshotOpenPath.c_str());
            ps_ui_window_refresh_preview(window);
            if (lastTab) {
              s.snapshotWindow = window; // picture of that tab's own window
            }
          }
        } else if (!s.snapshotTypeText.empty() && !s.snapshotTyped && s.snapshotIdleSlices >= 6) {
          // Types into the first text view so the picture shows an edited document.
          s.snapshotTyped = true;
          s.snapshotIdleSlices = 0;
          NSTextView *target = nil;
          const bool lastTab = std::getenv("PRIMESTRUCT_UI_TARGET_LAST") != nullptr;
          uint64_t picked = lastTab ? 0 : UINT64_MAX;
          for (const auto &entry : s.textViews) {
            if (lastTab ? entry.first > picked : entry.first < picked) {
              picked = entry.first;
              target = entry.second;
            }
          }
          if (target != nil) {
            [target.window makeFirstResponder:target];
            [target insertText:toNSString(s.snapshotTypeText.c_str()) replacementRange:NSMakeRange(NSNotFound, 0)];
          }
        } else if (!s.snapshotClicks.empty() && !s.snapshotClicked && s.snapshotIdleSlices >= 6) {
          // Clicks controls through their actions, as the user would; the program hears about each.
          s.snapshotClicked = true;
          s.snapshotIdleSlices = 0;
          applySnapshotClicks(s.snapshotClicks);
        } else if (!s.snapshotScroll.empty() && !s.snapshotScrolled && s.snapshotIdleSlices >= 8 &&
                   !anyPreviewLoading()) {
          // Scrolls one of the two views, as the user would; the other one follows.
          s.snapshotScrolled = true;
          s.snapshotIdleSlices = 0;
          applySnapshotScroll(s.snapshotWindow, s.snapshotScroll);
        } else if (s.snapshotIdleSlices >= 12 && (!anyPreviewLoading() || s.snapshotIdleSlices >= 400)) {
          // (A preview still loading its page is waited for, up to twenty seconds.)
          if (std::getenv("PRIMESTRUCT_UI_TARGET_LAST") != nullptr) {
            for (const auto &entry : s.windows) {
              s.snapshotWindow = std::max(s.snapshotWindow, entry.first);
            }
          }
          if (s.snapshotMenu.empty()) {
            captureSnapshot();
          } else {
            captureWithOpenMenu(s.snapshotMenu);
          }
          if (!s.snapshotTypeText.empty() || !s.snapshotMenu.empty()) {
            // The typed text is unsaved, so a graceful quit would ask about it.
            std::fflush(nullptr);
            std::_Exit(0);
          }
          enqueue(PS_UI_EVENT_QUIT_REQUESTED);
        }
      }
    }
  }
}

uint64_t ps_ui_event_window(void) { return usable() ? state().current.window : 0; }
uint64_t ps_ui_event_widget(void) { return usable() ? state().current.widget : 0; }
int32_t ps_ui_event_command(void) { return usable() ? state().current.command : 0; }

uint64_t ps_ui_window_create(const char *title, int32_t width, int32_t height) {
  if (!usable() || width <= 0 || height <= 0) {
    return 0;
  }
  State &s = state();
  @autoreleasepool {
    const NSWindowStyleMask style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                                    NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable;
    NSWindow *window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, width, height)
                                                   styleMask:style
                                                     backing:NSBackingStoreBuffered
                                                       defer:NO];
    window.releasedWhenClosed = NO;
    window.restorable = NO;
    window.delegate = controller();
    window.title = toNSString(title);
    window.tabbingMode = NSWindowTabbingModePreferred;
    window.tabbingIdentifier = @"PrimeStructDocument";
    [window center];
    const uint64_t handle = s.nextHandle++;
    tag(window, handle);
    s.windows[handle] = window;
    return handle;
  }
}

bool ps_ui_window_set_title(uint64_t window, const char *title) {
  NSWindow *w = usable() ? liveWindow(window) : nil;
  if (w == nil) {
    return false;
  }
  w.title = toNSString(title);
  return true;
}

bool ps_ui_window_set_content(uint64_t window, uint64_t widget) {
  NSWindow *w = usable() ? liveWindow(window) : nil;
  State &s = state();
  if (w != nil && usable() && controlInfo(widget) != nullptr) {
    // A control (a form, tabs, ...) as the content: its holder must be this window or none.
    const ControlInfo *root = controlInfo(widget);
    const uint64_t holder = windowOfControl(widget);
    if (root->parent != 0 || (holder != 0 && holder != window)) {
      return false;
    }
    if (holder == window) {
      return true;
    }
    w.contentView = windowContentFor(widget);
    s.windowContent[window] = widget;
    if (w.tabbingMode == NSWindowTabbingModeDisallowed) {
      fitSettingsWindow(w, window);
    }
    return true;
  }
  NSTextView *view = usable() ? textView(widget) : nil;
  if (w == nil || view == nil || (view.window != nil && view.window != w)) {
    return false;
  }
  NSScrollView *scroll = s.scrollViews[widget];
  NSRect contentRect = [w contentRectForFrameRect:w.frame];
  contentRect.origin = NSZeroPoint;
  scroll.frame = contentRect;
  scroll.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
  w.contentView = scroll;
  [w makeFirstResponder:view];
  s.windowContent[window] = widget;
  return true;
}

bool ps_ui_window_set_edited(uint64_t window, bool edited) {
  NSWindow *w = usable() ? liveWindow(window) : nil;
  if (w == nil) {
    return false;
  }
  w.documentEdited = edited;
  return true;
}

bool ps_ui_window_show(uint64_t window) {
  NSWindow *w = usable() ? liveWindow(window) : nil;
  if (w == nil) {
    return false;
  }
  State &s = state();
  [w makeKeyAndOrderFront:nil];
  if (s.snapshotPath.empty()) {
    [NSApp activateIgnoringOtherApps:YES];
  } else if (s.snapshotWindow == 0) {
    s.snapshotWindow = window;
  }
  return true;
}

bool ps_ui_window_close(uint64_t window) {
  NSWindow *w = usable() ? liveWindow(window) : nil;
  if (w == nil) {
    return false;
  }
  w.delegate = nil;
  [w close];
  state().windows.erase(window);
  state().windowContent.erase(window);
  state().windowPaths.erase(window);
  dropPreviewState(window);
  return true;
}

bool ps_ui_window_add_tab(uint64_t host, uint64_t tab) {
  NSWindow *h = usable() ? liveWindow(host) : nil;
  NSWindow *t = usable() ? liveWindow(tab) : nil;
  if (h == nil || t == nil || h == t || t.tabbedWindows.count > 1 ||
      h.tabbingMode == NSWindowTabbingModeDisallowed || t.tabbingMode == NSWindowTabbingModeDisallowed) {
    return false; // a settings window is never a tab
  }
  [h addTabbedWindow:t ordered:NSWindowAbove];
  [t makeKeyAndOrderFront:nil];
  return true;
}

uint64_t ps_ui_active_window(void) {
  if (!usable()) {
    return 0;
  }
  State &s = state();
  PS_TRACE(@"activeWindow key=%llu main=%llu windows=%lu", handleOf(NSApp.keyWindow), handleOf(NSApp.mainWindow),
           static_cast<unsigned long>(s.windows.size()));
  for (const uint64_t handle : {handleOf(NSApp.keyWindow), handleOf(NSApp.mainWindow)}) {
    if (handle != 0 && s.windows.count(handle) != 0) {
      return handle;
    }
  }
  if (std::getenv("PRIMESTRUCT_UI_TARGET_LAST") != nullptr) {
    // Snapshot runs have no key window: act on the newest tab, as the user would.
    uint64_t newest = 0;
    for (const auto &entry : s.windows) {
      newest = std::max(newest, entry.first);
    }
    return newest;
  }
  if (s.snapshotWindow != 0 && s.windows.count(s.snapshotWindow) != 0) {
    return s.snapshotWindow;
  }
  uint64_t lowest = 0;
  for (const auto &entry : s.windows) {
    lowest = lowest == 0 ? entry.first : std::min(lowest, entry.first);
  }
  return lowest;
}

int32_t ps_ui_window_tab_count(uint64_t window) {
  NSWindow *w = usable() ? liveWindow(window) : nil;
  if (w == nil) {
    return 0;
  }
  return w.tabbedWindows.count > 0 ? static_cast<int32_t>(w.tabbedWindows.count) : 1;
}

uint64_t ps_ui_window_tab_at(uint64_t window, int32_t index) {
  NSWindow *w = usable() ? liveWindow(window) : nil;
  if (w == nil || index < 0) {
    return 0;
  }
  NSArray<NSWindow *> *tabs = w.tabbedWindows.count > 0 ? w.tabbedWindows : @[ w ];
  return static_cast<NSUInteger>(index) < tabs.count ? handleOf(tabs[static_cast<NSUInteger>(index)]) : 0;
}

bool ps_ui_window_select(uint64_t window) {
  NSWindow *w = usable() ? liveWindow(window) : nil;
  if (w == nil) {
    return false;
  }
  if (w.tabGroup != nil) {
    w.tabGroup.selectedWindow = w;
  }
  [w makeKeyAndOrderFront:nil];
  return true;
}

uint64_t ps_ui_window_content(uint64_t window) {
  State &s = state();
  auto it = usable() && liveWindow(window) != nil ? s.windowContent.find(window) : s.windowContent.end();
  return it != s.windowContent.end() ? it->second : 0;
}

int32_t ps_ui_window_count(void) { return usable() ? static_cast<int32_t>(state().windows.size()) : 0; }

uint64_t ps_ui_window_at(int32_t index) {
  if (!usable() || index < 0) {
    return 0;
  }
  std::vector<uint64_t> handles;
  for (const auto &entry : state().windows) {
    handles.push_back(entry.first);
  }
  std::sort(handles.begin(), handles.end());
  return static_cast<size_t>(index) < handles.size() ? handles[static_cast<size_t>(index)] : 0;
}

bool ps_ui_window_set_path(uint64_t window, const char *path) {
  if (!usable() || liveWindow(window) == nil) {
    return false;
  }
  state().windowPaths[window] = path != nullptr ? path : "";
  PS_TRACE(@"setPath window=%llu path=%s", window, path != nullptr ? path : "(null)");
  return true;
}

const char *ps_ui_window_path(uint64_t window) {
  State &s = state();
  auto it = usable() && liveWindow(window) != nil ? s.windowPaths.find(window) : s.windowPaths.end();
  s.returnedText = it != s.windowPaths.end() ? it->second : std::string();
  return s.returnedText.c_str();
}

bool ps_ui_window_has_path(uint64_t window) {
  State &s = state();
  auto it = usable() && liveWindow(window) != nil ? s.windowPaths.find(window) : s.windowPaths.end();
  return it != s.windowPaths.end() && !it->second.empty();
}

uint64_t ps_ui_text_view_create(void) {
  if (!usable()) {
    return 0;
  }
  State &s = state();
  @autoreleasepool {
    NSScrollView *scroll = [NSTextView scrollableTextView];
    NSTextView *view = scroll.documentView;
    view.richText = NO;
    view.allowsUndo = YES;
    view.automaticQuoteSubstitutionEnabled = NO;
    view.automaticDashSubstitutionEnabled = NO;
    view.automaticTextReplacementEnabled = NO;
    view.automaticSpellingCorrectionEnabled = NO;
    view.textContainerInset = NSMakeSize(8, 8);
    view.delegate = controller();
    const uint64_t handle = s.nextHandle++;
    tag(view, handle);
    s.textViews[handle] = view;
    s.baseFonts[handle] = view.font;
    s.scrollViews[handle] = scroll;
    s.modified[handle] = false;
    return handle;
  }
}

const char *ps_ui_text_view_get_text(uint64_t view) {
  State &s = state();
  NSTextView *v = usable() ? textView(view) : nil;
  s.returnedText = v != nil ? std::string(v.string.UTF8String) : std::string();
  return s.returnedText.c_str();
}

bool ps_ui_text_view_set_text(uint64_t view, const char *text) {
  NSTextView *v = usable() ? textView(view) : nil;
  if (v == nil) {
    return false;
  }
  v.string = toNSString(text);
  state().modified[view] = false;
  [v.undoManager removeAllActions];
  return true;
}

bool ps_ui_text_view_set_monospace(uint64_t view, bool monospace) {
  NSTextView *v = usable() ? textView(view) : nil;
  if (v == nil) {
    return false;
  }
  const auto points = state().fontPoints.find(view);
  const CGFloat size = points != state().fontPoints.end() ? points->second : 13;
  v.font = monospace ? [NSFont monospacedSystemFontOfSize:size weight:NSFontWeightRegular]
                     : [NSFont systemFontOfSize:size];
  state().baseFonts[view] = v.font;
  return true;
}

bool ps_ui_text_view_set_line_numbers(uint64_t view, bool visible) {
  NSTextView *v = usable() ? textView(view) : nil;
  if (v == nil) {
    return false;
  }
  @autoreleasepool {
    NSScrollView *scroll = state().scrollViews[view];
    if (visible && ![scroll.verticalRulerView isKindOfClass:PSLineNumberRuler.class]) {
      scroll.verticalRulerView = [[PSLineNumberRuler alloc] initForTextView:v scrollView:scroll];
      scroll.hasVerticalRuler = YES;
    }
    scroll.rulersVisible = visible;
  }
  return true;
}

bool ps_ui_text_view_set_wrapping(uint64_t view, bool wrap) {
  NSTextView *v = usable() ? textView(view) : nil;
  if (v == nil) {
    return false;
  }
  NSScrollView *scroll = state().scrollViews[view];
  NSTextContainer *container = v.textContainer;
  if (wrap) {
    container.widthTracksTextView = YES;
    v.horizontallyResizable = NO;
    v.autoresizingMask = NSViewWidthSizable;
    scroll.hasHorizontalScroller = NO;
    container.containerSize = NSMakeSize(scroll.contentSize.width, CGFLOAT_MAX);
  } else {
    container.widthTracksTextView = NO;
    container.containerSize = NSMakeSize(CGFLOAT_MAX, CGFLOAT_MAX);
    v.horizontallyResizable = YES;
    v.autoresizingMask = NSViewNotSizable;
    v.maxSize = NSMakeSize(CGFLOAT_MAX, CGFLOAT_MAX);
    scroll.hasHorizontalScroller = YES;
  }
  [v sizeToFit];
  return true;
}

bool ps_ui_text_view_clear_styles(uint64_t view) {
  NSTextView *v = usable() ? textView(view) : nil;
  if (v == nil) {
    return false;
  }
  NSFont *base = state().baseFonts[view];
  NSTextStorage *storage = v.textStorage;
  const NSRange all = NSMakeRange(0, storage.length);
  [v.undoManager disableUndoRegistration];
  [storage beginEditing];
  [storage addAttribute:NSForegroundColorAttributeName value:[NSColor textColor] range:all];
  if (base != nil) {
    [storage addAttribute:NSFontAttributeName value:base range:all];
  }
  [storage endEditing];
  [v.undoManager enableUndoRegistration];
  return true;
}

// Restyles the whole text in one edit: the lexer's runs are in order and do not
// overlap, so byte offsets become UTF-16 offsets in a single pass and the cost is
// linear in the text, however many runs there are.
bool ps_ui_text_view_highlight(uint64_t view, const char *path) {
  NSTextView *v = usable() ? textView(view) : nil;
  if (v == nil) {
    return false;
  }
  const char *utf8 = v.string.UTF8String;
  const std::string text = utf8 != nullptr ? utf8 : "";
  const primec::ui::Language language = primec::ui::languageForPath(path != nullptr ? path : "");
  const bool styled = language != primec::ui::Language::None && text.size() <= primec::ui::MaxHighlightBytes;
  NSFont *base = state().baseFonts[view];
  NSTextStorage *storage = v.textStorage;
  size_t runCount = 0;
  [v.undoManager disableUndoRegistration];
  [storage beginEditing];
  const NSRange all = NSMakeRange(0, storage.length);
  [storage addAttribute:NSForegroundColorAttributeName value:[NSColor textColor] range:all];
  if (base != nil) {
    [storage addAttribute:NSFontAttributeName value:base range:all];
  }
  if (styled) {
    NSFontManager *manager = [NSFontManager sharedFontManager];
    NSFont *fonts[4] = {base, nil, nil, nil}; // by flags: bold = 1, italic = 2
    auto fontFor = [&](int32_t flags) -> NSFont * {
      const int index = flags & 3;
      if (fonts[index] == nil && base != nil) {
        NSFont *font = base;
        if ((index & PS_UI_STYLE_BOLD) != 0) {
          font = [manager convertFont:font toHaveTrait:NSBoldFontMask];
        }
        if ((index & PS_UI_STYLE_ITALIC) != 0) {
          font = [manager convertFont:font toHaveTrait:NSItalicFontMask];
        }
        fonts[index] = font;
      }
      return fonts[index];
    };
    size_t bytePos = 0;
    NSUInteger utf16Pos = 0;
    auto advanceTo = [&](size_t target) {
      while (bytePos < target && bytePos < text.size()) {
        const unsigned char c = static_cast<unsigned char>(text[bytePos]);
        const size_t length = c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4;
        utf16Pos += length == 4 ? 2 : 1;
        bytePos += length;
      }
    };
    for (const primec::ui::HighlightRun &run : primec::ui::highlight(language, text)) {
      advanceTo(static_cast<size_t>(run.start));
      const NSUInteger begin = utf16Pos;
      advanceTo(static_cast<size_t>(run.end));
      if (utf16Pos <= begin || utf16Pos > storage.length) {
        continue;
      }
      const NSRange range = NSMakeRange(begin, utf16Pos - begin);
      [storage addAttribute:NSForegroundColorAttributeName
                      value:[NSColor colorWithSRGBRed:((run.rgb >> 16) & 0xFF) / 255.0
                                                green:((run.rgb >> 8) & 0xFF) / 255.0
                                                 blue:(run.rgb & 0xFF) / 255.0
                                                alpha:1.0]
                      range:range];
      if (NSFont *font = fontFor(run.flags); font != nil && (run.flags & 3) != 0) {
        [storage addAttribute:NSFontAttributeName value:font range:range];
      }
      ++runCount;
    }
  }
  [storage endEditing];
  [v.undoManager enableUndoRegistration];
  PS_TRACE(@"highlight view=%llu window=%llu path=%s textBytes=%lu runs=%lu", view, handleOf(v.window),
           path != nullptr ? path : "(null)", static_cast<unsigned long>(text.size()),
           static_cast<unsigned long>(runCount));
  return true;
}

bool ps_ui_text_view_add_style(uint64_t view, int32_t startByte, int32_t endByte, int32_t rgb, int32_t flags) {
  NSTextView *v = usable() ? textView(view) : nil;
  if (v == nil || startByte < 0 || endByte <= startByte) {
    return false;
  }
  const char *utf8 = v.string.UTF8String;
  const size_t size = std::strlen(utf8);
  auto boundary = [&](int32_t at) {
    return static_cast<size_t>(at) == size || (static_cast<unsigned char>(utf8[at]) & 0xC0) != 0x80;
  };
  if (static_cast<size_t>(endByte) > size || !boundary(startByte) || !boundary(endByte)) {
    return false;
  }
  // Byte offsets of the UTF-8 text become UTF-16 offsets of the NSString.
  NSString *head = [[NSString alloc] initWithBytes:utf8 length:static_cast<NSUInteger>(startByte)
                                          encoding:NSUTF8StringEncoding];
  NSString *body = [[NSString alloc] initWithBytes:utf8 + startByte length:static_cast<NSUInteger>(endByte - startByte)
                                          encoding:NSUTF8StringEncoding];
  if (head == nil || body == nil) {
    return false;
  }
  const NSRange range = NSMakeRange(head.length, body.length);
  NSColor *color = [NSColor colorWithSRGBRed:((rgb >> 16) & 0xFF) / 255.0
                                       green:((rgb >> 8) & 0xFF) / 255.0
                                        blue:(rgb & 0xFF) / 255.0
                                       alpha:1.0];
  NSFont *font = state().baseFonts[view];
  if (font != nil && (flags & (PS_UI_STYLE_BOLD | PS_UI_STYLE_ITALIC)) != 0) {
    NSFontManager *manager = [NSFontManager sharedFontManager];
    if ((flags & PS_UI_STYLE_BOLD) != 0) {
      font = [manager convertFont:font toHaveTrait:NSBoldFontMask];
    }
    if ((flags & PS_UI_STYLE_ITALIC) != 0) {
      font = [manager convertFont:font toHaveTrait:NSItalicFontMask];
    }
  }
  NSTextStorage *storage = v.textStorage;
  // Presentation only: no undo entry, and the program-visible modified flag is
  // tracked from user edits, which attribute changes are not.
  [v.undoManager disableUndoRegistration];
  [storage beginEditing];
  [storage addAttribute:NSForegroundColorAttributeName value:color range:range];
  if (font != nil) {
    [storage addAttribute:NSFontAttributeName value:font range:range];
  }
  [storage endEditing];
  [v.undoManager enableUndoRegistration];
  return true;
}

bool ps_ui_text_view_is_modified(uint64_t view) {
  return usable() && textView(view) != nil && state().modified[view];
}

bool ps_ui_text_view_clear_modified(uint64_t view) {
  if (!usable() || textView(view) == nil) {
    return false;
  }
  state().modified[view] = false;
  return true;
}

bool ps_ui_text_view_load_file(uint64_t view, const char *path) {
  State &s = state();
  NSTextView *v = usable() ? textView(view) : nil;
  if (v == nil) {
    s.lastError = "invalid text view";
    return false;
  }
  @autoreleasepool {
    NSError *error = nil;
    NSString *contents = [NSString stringWithContentsOfFile:toNSString(path) encoding:NSUTF8StringEncoding error:&error];
    if (contents == nil) {
      s.lastError = std::string("cannot read ") + (path != nullptr ? path : "") + ": " +
                    (error != nil ? error.localizedDescription.UTF8String : "not valid UTF-8 text");
      return false;
    }
    v.string = contents;
    s.modified[view] = false;
    [v.undoManager removeAllActions];
    PS_TRACE(@"loadFile view=%llu window=%llu path=%s", view, handleOf(v.window), path != nullptr ? path : "(null)");
    return true;
  }
}

bool ps_ui_text_view_save_file(uint64_t view, const char *path) {
  State &s = state();
  NSTextView *v = usable() ? textView(view) : nil;
  if (v == nil) {
    s.lastError = "invalid text view";
    return false;
  }
  @autoreleasepool {
    NSError *error = nil;
    if (![v.string writeToFile:toNSString(path) atomically:YES encoding:NSUTF8StringEncoding error:&error]) {
      s.lastError = std::string("cannot write ") + (path != nullptr ? path : "") + ": " +
                    (error != nil ? error.localizedDescription.UTF8String : "unknown error");
      return false;
    }
    return true;
  }
}

const char *ps_ui_last_error(void) { return state().lastError.c_str(); }

uint64_t ps_ui_menu_create(const char *title) {
  if (!usable()) {
    return 0;
  }
  State &s = state();
  NSMenu *menu = [[NSMenu alloc] initWithTitle:toNSString(title)];
  menu.autoenablesItems = YES;
  const uint64_t handle = s.nextHandle++;
  tag(menu, handle);
  s.menus[handle] = menu;
  return handle;
}

bool ps_ui_menu_add_item(uint64_t menuHandle, const char *title, const char *shortcut, int32_t commandId) {
  NSMenu *menu = usable() ? menuFor(menuHandle) : nil;
  if (menu == nil) {
    return false;
  }
  NSString *key = nil;
  NSEventModifierFlags mask = 0;
  parseShortcut(shortcut, &key, &mask);
  addMenuItem(menu, toNSString(title), @selector(menuCommand:), controller(), key, mask, commandId);
  return true;
}

bool ps_ui_menu_set_checked(uint64_t menuHandle, int32_t commandId, bool checked) {
  NSMenu *menu = usable() ? menuFor(menuHandle) : nil;
  if (menu == nil) {
    return false;
  }
  bool found = false;
  for (NSMenuItem *item in menu.itemArray) {
    if (item.action == @selector(menuCommand:) && item.tag == commandId) {
      item.state = checked ? NSControlStateValueOn : NSControlStateValueOff;
      found = true;
    }
  }
  return found;
}

bool ps_ui_window_set_autosave_name(uint64_t window, const char *name) {
  NSWindow *w = usable() ? liveWindow(window) : nil;
  if (w == nil || name == nullptr || *name == '\0') {
    return false;
  }
  if (state().prefOverridesActive) {
    return true; // test and snapshot runs neither read nor write the user's saved frames
  }
  return [w setFrameAutosaveName:toNSString(name)];
}

bool ps_ui_window_set_preview(uint64_t window, bool on) {
  NSWindow *w = usable() ? liveWindow(window) : nil;
  if (w == nil) {
    return false;
  }
  State &s = state();
  const auto content = s.windowContent.find(window);
  NSTextView *view = content != s.windowContent.end() ? textView(content->second) : nil;
  NSScrollView *scroll = previewTextScroll(window);
  if (on == previewOn(window)) {
    return true; // already as asked
  }
  NSRect contentRect = [w contentRectForFrameRect:w.frame];
  contentRect.origin = NSZeroPoint;
  if (!on) {
    // The text view alone again.
    [s.previewWebs[window] removeFromSuperview];
    if (scroll != nil) {
      [scroll removeFromSuperview];
      scroll.frame = contentRect;
      scroll.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
      w.contentView = scroll;
      [w makeFirstResponder:view];
    }
    dropPreviewState(window);
    return true;
  }
  if (view == nil || scroll == nil) {
    return false;
  }
  Class configurationClass = webKitClass(@"WKWebViewConfiguration");
  Class webClass = webKitClass(@"WKWebView");
  if (configurationClass == Nil || webClass == Nil) {
    return false;
  }
  @autoreleasepool {
    id configuration = [[configurationClass alloc] init];
    // The page is fixed text: nothing in it runs.
    [[configuration valueForKey:@"defaultWebpagePreferences"] setValue:@NO forKey:@"allowsContentJavaScript"];
    NSView *web = ((id(*)(id, SEL, NSRect, id))objc_msgSend)([webClass alloc],
                                                             NSSelectorFromString(@"initWithFrame:configuration:"),
                                                             contentRect, configuration);
    static id delegate = [[previewDelegateClass() alloc] init];
    [web setValue:delegate forKey:@"navigationDelegate"];
    tag(web, window);
    NSSplitView *split = [[NSSplitView alloc] initWithFrame:contentRect];
    split.vertical = YES;
    split.dividerStyle = NSSplitViewDividerStyleThin;
    split.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    w.contentView = split;
    [split addSubview:scroll];
    [split addSubview:web];
    [split setPosition:contentRect.size.width / 2 ofDividerAtIndex:0];
    [w makeFirstResponder:view];
    s.previews[window] = true;
    s.previewSplits[window] = split;
    s.previewWebs[window] = web;
    // The page follows the text view when it scrolls (the text view follows the page through the monitor).
    scroll.contentView.postsBoundsChangedNotifications = YES;
    s.previewObservers[window] =
        [NSNotificationCenter.defaultCenter addObserverForName:NSViewBoundsDidChangeNotification
                                                        object:scroll.contentView
                                                         queue:nil
                                                    usingBlock:^(NSNotification *) {
                                                      if (!state().previewSyncing[window]) {
                                                        syncWebFromText(window, false);
                                                      }
                                                    }];
    ensurePreviewEventMonitor();
    renderPreview(window);
  }
  return true;
}

bool ps_ui_window_has_preview(uint64_t window) {
  return usable() && liveWindow(window) != nil && previewOn(window);
}

// Refreshes that follow each other closely (every keystroke) become one page load per run-loop turn.
bool ps_ui_window_refresh_preview(uint64_t window) {
  if (!ps_ui_window_has_preview(window)) {
    return false;
  }
  State &s = state();
  if (!s.previewRefreshPending[window]) {
    s.previewRefreshPending[window] = true;
    s.previewLoading[window] = true; // a snapshot waits for the page
    dispatch_async(dispatch_get_main_queue(), ^{
      state().previewRefreshPending[window] = false;
      if (previewOn(window)) {
        renderPreview(window);
      }
    });
  }
  return true;
}

bool ps_ui_menu_follow_preview(uint64_t menuHandle, int32_t commandId) {
  NSMenu *menu = usable() ? menuFor(menuHandle) : nil;
  if (menu == nil) {
    return false;
  }
  bool found = false;
  for (NSMenuItem *item in menu.itemArray) {
    if (item.action == @selector(menuCommand:) && item.tag == commandId) {
      found = true;
    }
  }
  if (found) {
    state().previewMenuItems.insert(commandId);
  }
  return found;
}

uint64_t ps_ui_label_create(const char *text) {
  return usable() ? createControl(ControlInfo::Kind::Label, [NSTextField labelWithString:toNSString(text)]) : 0;
}

bool ps_ui_label_set_text(uint64_t label, const char *text) {
  ControlInfo *info = controlOfKind(label, ControlInfo::Kind::Label);
  if (info == nullptr || text == nullptr) {
    return false;
  }
  [(NSTextField *)info->view setStringValue:toNSString(text)];
  return true;
}

uint64_t ps_ui_checkbox_create(const char *title, bool checked) {
  if (!usable()) {
    return 0;
  }
  NSButton *box = [NSButton checkboxWithTitle:toNSString(title) target:controller() action:@selector(controlChanged:)];
  box.state = checked ? NSControlStateValueOn : NSControlStateValueOff;
  // A title stays on one line: wrapping would make the control's fitting width the width of a word.
  box.lineBreakMode = NSLineBreakByClipping;
  [box setContentCompressionResistancePriority:NSLayoutPriorityRequired forOrientation:NSLayoutConstraintOrientationHorizontal];
  return createControl(ControlInfo::Kind::CheckBox, box);
}

bool ps_ui_checkbox_set_checked(uint64_t checkbox, bool checked) {
  ControlInfo *info = controlOfKind(checkbox, ControlInfo::Kind::CheckBox);
  if (info == nullptr) {
    return false;
  }
  ((NSButton *)info->view).state = checked ? NSControlStateValueOn : NSControlStateValueOff;
  return true;
}

bool ps_ui_checkbox_checked(uint64_t checkbox) {
  ControlInfo *info = controlOfKind(checkbox, ControlInfo::Kind::CheckBox);
  return info != nullptr && ((NSButton *)info->view).state == NSControlStateValueOn;
}

uint64_t ps_ui_popup_create(void) {
  if (!usable()) {
    return 0;
  }
  NSPopUpButton *popup = [[NSPopUpButton alloc] initWithFrame:NSZeroRect pullsDown:NO];
  popup.menu.autoenablesItems = NO;
  popup.target = controller();
  popup.action = @selector(controlChanged:);
  return createControl(ControlInfo::Kind::Popup, popup);
}

bool ps_ui_popup_add_item(uint64_t popup, const char *title) {
  ControlInfo *info = controlOfKind(popup, ControlInfo::Kind::Popup);
  if (info == nullptr || title == nullptr) {
    return false;
  }
  NSPopUpButton *button = (NSPopUpButton *)info->view;
  // The menu is used directly: addItemWithTitle: on the button drops an item whose title is already there.
  NSMenuItem *item = [button.menu addItemWithTitle:toNSString(title) action:nil keyEquivalent:@""];
  item.enabled = YES;
  if (button.numberOfItems == 1) {
    [button selectItemAtIndex:0];
  }
  return true;
}

bool ps_ui_popup_select(uint64_t popup, int32_t index) {
  ControlInfo *info = controlOfKind(popup, ControlInfo::Kind::Popup);
  NSPopUpButton *button = info != nullptr ? (NSPopUpButton *)info->view : nil;
  if (button == nil || index < 0 || index >= button.numberOfItems) {
    return false;
  }
  [button selectItemAtIndex:index];
  return true;
}

int32_t ps_ui_popup_selected(uint64_t popup) {
  ControlInfo *info = controlOfKind(popup, ControlInfo::Kind::Popup);
  return info != nullptr ? static_cast<int32_t>(((NSPopUpButton *)info->view).indexOfSelectedItem) : -1;
}

uint64_t ps_ui_form_create(void) {
  if (!usable()) {
    return 0;
  }
  NSGridView *grid = [NSGridView gridViewWithNumberOfColumns:2 rows:0];
  grid.columnSpacing = 12;
  grid.rowSpacing = 10;
  grid.rowAlignment = NSGridRowAlignmentFirstBaseline;
  [grid columnAtIndex:0].xPlacement = NSGridCellPlacementTrailing;
  [grid columnAtIndex:1].xPlacement = NSGridCellPlacementLeading;
  return createControl(ControlInfo::Kind::Form, grid);
}

bool ps_ui_form_add_heading(uint64_t form, const char *title) {
  ControlInfo *info = controlOfKind(form, ControlInfo::Kind::Form);
  if (info == nullptr || title == nullptr) {
    return false;
  }
  NSGridView *grid = (NSGridView *)info->view;
  NSTextField *heading = [NSTextField labelWithString:toNSString(title)];
  heading.font = [NSFont boldSystemFontOfSize:NSFont.systemFontSize];
  NSGridRow *row = [grid addRowWithViews:@[ heading, [NSGridCell emptyContentView] ]];
  [row mergeCellsInRange:NSMakeRange(0, 2)];
  [row cellAtIndex:0].xPlacement = NSGridCellPlacementLeading;
  if ([grid indexOfRow:row] > 0) {
    row.topPadding = 12;
  }
  return true;
}

bool ps_ui_form_add_row(uint64_t form, const char *label, uint64_t control) {
  ControlInfo *info = controlOfKind(form, ControlInfo::Kind::Form);
  ControlInfo *child = info != nullptr && control != form ? freeControl(control) : nullptr;
  if (child == nullptr || label == nullptr) {
    return false;
  }
  [(NSGridView *)info->view addRowWithViews:@[ [NSTextField labelWithString:toNSString(label)], child->view ]];
  child->parent = form;
  return true;
}

bool ps_ui_form_add_control(uint64_t form, uint64_t control) {
  ControlInfo *info = controlOfKind(form, ControlInfo::Kind::Form);
  ControlInfo *child = info != nullptr && control != form ? freeControl(control) : nullptr;
  if (child == nullptr) {
    return false;
  }
  // The blank first cell is an empty label, not the grid's empty-content marker: a column of only markers and
  // merged headings takes no width, and the controls beside it would not count in the form's size.
  [(NSGridView *)info->view addRowWithViews:@[ [NSTextField labelWithString:@""], child->view ]];
  child->parent = form;
  return true;
}

uint64_t ps_ui_tabs_create(void) {
  if (!usable()) {
    return 0;
  }
  return createControl(ControlInfo::Kind::Tabs, [[NSTabView alloc] initWithFrame:NSZeroRect]);
}

bool ps_ui_tabs_add(uint64_t tabs, const char *title, uint64_t content) {
  ControlInfo *info = controlOfKind(tabs, ControlInfo::Kind::Tabs);
  ControlInfo *page = info != nullptr && content != tabs ? freeControl(content) : nullptr;
  if (page == nullptr || title == nullptr) {
    return false;
  }
  NSTabViewItem *item = [[NSTabViewItem alloc] initWithIdentifier:@(content)];
  item.label = toNSString(title);
  item.view = paddedView(page->view, PageMargin);
  [(NSTabView *)info->view addTabViewItem:item];
  page->parent = tabs;
  return true;
}

bool ps_ui_tabs_select(uint64_t tabs, int32_t index) {
  ControlInfo *info = controlOfKind(tabs, ControlInfo::Kind::Tabs);
  NSTabView *view = info != nullptr ? (NSTabView *)info->view : nil;
  if (view == nil || index < 0 || index >= view.numberOfTabViewItems) {
    return false;
  }
  [view selectTabViewItemAtIndex:index];
  return true;
}

// A settings window is never a tab, is as large as its content needs, and has no minimize or zoom; a
// document window is the default.
bool ps_ui_window_set_style(uint64_t window, int32_t style) {
  NSWindow *w = usable() ? liveWindow(window) : nil;
  if (w == nil || (style != PS_UI_WINDOW_DOCUMENT && style != PS_UI_WINDOW_SETTINGS)) {
    return false;
  }
  if (style == PS_UI_WINDOW_SETTINGS) {
    if (w.tabbedWindows.count > 1) {
      return false;
    }
    w.tabbingMode = NSWindowTabbingModeDisallowed;
    w.styleMask = w.styleMask & ~(NSWindowStyleMaskResizable | NSWindowStyleMaskMiniaturizable);
    [w standardWindowButton:NSWindowZoomButton].enabled = NO;
    [w standardWindowButton:NSWindowMiniaturizeButton].enabled = NO;
    fitSettingsWindow(w, window);
  } else {
    w.tabbingMode = NSWindowTabbingModePreferred;
    w.styleMask = w.styleMask | NSWindowStyleMaskResizable | NSWindowStyleMaskMiniaturizable;
    [w standardWindowButton:NSWindowZoomButton].enabled = YES;
    [w standardWindowButton:NSWindowMiniaturizeButton].enabled = YES;
  }
  return true;
}

// The text's size in points: the base font and every styled run keep their weight and slant at the new size,
// so a highlighted document changes size at once.
bool ps_ui_text_view_set_font_size(uint64_t view, int32_t points) {
  NSTextView *v = usable() ? textView(view) : nil;
  if (v == nil || points < 6 || points > 96) {
    return false;
  }
  State &s = state();
  s.fontPoints[view] = points;
  NSFont *base = s.baseFonts[view];
  if (base != nil) {
    s.baseFonts[view] = [NSFont fontWithDescriptor:base.fontDescriptor size:points];
  }
  NSTextStorage *storage = v.textStorage;
  [v.undoManager disableUndoRegistration];
  [storage beginEditing];
  [storage enumerateAttribute:NSFontAttributeName
                      inRange:NSMakeRange(0, storage.length)
                      options:0
                   usingBlock:^(id value, NSRange range, BOOL *stop) {
                     if ([value isKindOfClass:[NSFont class]]) {
                       NSFont *font = value;
                       [storage addAttribute:NSFontAttributeName
                                       value:[NSFont fontWithDescriptor:font.fontDescriptor size:points]
                                       range:range];
                     }
                   }];
  [storage endEditing];
  [v.undoManager enableUndoRegistration];
  NSMutableDictionary *typing = [v.typingAttributes mutableCopy];
  typing[NSFontAttributeName] = s.baseFonts[view] != nil ? s.baseFonts[view] : [NSFont systemFontOfSize:points];
  v.typingAttributes = typing;
  return true;
}

bool ps_ui_preview_set_scroll_lock(bool on) {
  if (!usable()) {
    return false;
  }
  state().previewScrollLock = on;
  return true;
}

int32_t ps_ui_pref_get_int(const char *key, int32_t fallback) {
  if (!usable() || key == nullptr || *key == '\0') {
    return fallback;
  }
  State &s = state();
  if (const auto it = s.intOverrides.find(key); it != s.intOverrides.end()) {
    return it->second;
  }
  if (s.prefOverridesActive) {
    return fallback;
  }
  NSString *name = [@"ps.pref." stringByAppendingString:toNSString(key)];
  NSUserDefaults *defaults = NSUserDefaults.standardUserDefaults;
  return [defaults objectForKey:name] != nil ? static_cast<int32_t>([defaults integerForKey:name]) : fallback;
}

bool ps_ui_pref_set_int(const char *key, int32_t value) {
  if (!usable() || key == nullptr || *key == '\0') {
    return false;
  }
  State &s = state();
  if (s.prefOverridesActive) {
    s.intOverrides[key] = value;
    return true;
  }
  [NSUserDefaults.standardUserDefaults setInteger:value forKey:[@"ps.pref." stringByAppendingString:toNSString(key)]];
  return true;
}

static NSString *listName(const char *key) { return [@"ps.list." stringByAppendingString:toNSString(key)]; }

bool ps_ui_pref_list_clear(const char *key) {
  if (!usable() || key == nullptr || *key == '\0') {
    return false;
  }
  State &s = state();
  if (s.prefOverridesActive) {
    s.listOverrides[key].clear();
  } else {
    [NSUserDefaults.standardUserDefaults setObject:@[] forKey:listName(key)];
  }
  return true;
}

bool ps_ui_pref_list_add(const char *key, const char *text) {
  if (!usable() || key == nullptr || *key == '\0' || text == nullptr) {
    return false;
  }
  State &s = state();
  if (s.prefOverridesActive) {
    s.listOverrides[key].push_back(text);
  } else {
    NSUserDefaults *defaults = NSUserDefaults.standardUserDefaults;
    NSMutableArray *items = [([defaults arrayForKey:listName(key)] ?: @[]) mutableCopy];
    [items addObject:toNSString(text)];
    [defaults setObject:items forKey:listName(key)];
  }
  return true;
}

int32_t ps_ui_pref_list_count(const char *key) {
  if (!usable() || key == nullptr || *key == '\0') {
    return 0;
  }
  State &s = state();
  if (s.prefOverridesActive) {
    const auto it = s.listOverrides.find(key);
    return it != s.listOverrides.end() ? static_cast<int32_t>(it->second.size()) : 0;
  }
  return static_cast<int32_t>([NSUserDefaults.standardUserDefaults arrayForKey:listName(key)].count);
}

const char *ps_ui_pref_list_get(const char *key, int32_t index) {
  State &s = state();
  s.returnedText.clear();
  if (usable() && key != nullptr && *key != '\0' && index >= 0) {
    if (s.prefOverridesActive) {
      const auto it = s.listOverrides.find(key);
      if (it != s.listOverrides.end() && static_cast<size_t>(index) < it->second.size()) {
        s.returnedText = it->second[static_cast<size_t>(index)];
      }
    } else {
      NSArray *items = [NSUserDefaults.standardUserDefaults arrayForKey:listName(key)];
      if (static_cast<NSUInteger>(index) < items.count && [items[static_cast<NSUInteger>(index)] isKindOfClass:NSString.class]) {
        s.returnedText = [items[static_cast<NSUInteger>(index)] UTF8String];
      }
    }
  }
  return s.returnedText.c_str();
}

bool ps_ui_menu_add_submenu(uint64_t menuHandle, const char *title, uint64_t submenuHandle) {
  NSMenu *menu = usable() ? menuFor(menuHandle) : nil;
  NSMenu *child = usable() ? menuFor(submenuHandle) : nil;
  if (menu == nil || child == nil || menu == child || child.supermenu != nil) {
    return false;
  }
  NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:toNSString(title) action:nil keyEquivalent:@""];
  child.title = toNSString(title);
  item.submenu = child;
  [menu addItem:item];
  return true;
}

bool ps_ui_menu_clear(uint64_t menuHandle) {
  NSMenu *menu = usable() ? menuFor(menuHandle) : nil;
  if (menu == nil) {
    return false;
  }
  [menu removeAllItems];
  return true;
}

int32_t ps_ui_menu_add_recent_files(uint64_t menuHandle, int32_t firstCommandId, int32_t maxItems) {
  NSMenu *menu = usable() ? menuFor(menuHandle) : nil;
  if (menu == nil || maxItems <= 0) {
    return 0;
  }
  const std::vector<std::string> paths = recentFilePaths();
  int32_t added = 0;
  for (size_t i = 0; i < paths.size() && added < maxItems; ++i) {
    addMenuItem(menu, toNSString(primec::ui::recentFileTitle(paths, i).c_str()), @selector(menuCommand:),
                controller(), @"", 0, firstCommandId + static_cast<int32_t>(i));
    ++added;
  }
  return added;
}

bool ps_ui_recent_add(const char *path) {
  if (!usable() || path == nullptr || *path == '\0') {
    return false;
  }
  State &s = state();
  if (s.prefOverridesActive) {
    s.recentOverride.erase(std::remove(s.recentOverride.begin(), s.recentOverride.end(), std::string(path)),
                           s.recentOverride.end());
    s.recentOverride.insert(s.recentOverride.begin(), path);
    if (s.recentOverride.size() > 10) {
      s.recentOverride.resize(10);
    }
    return true;
  }
  std::vector<std::string> paths = recentFilePaths();
  paths.erase(std::remove(paths.begin(), paths.end(), std::string(path)), paths.end());
  paths.insert(paths.begin(), path);
  if (paths.size() > 10) {
    paths.resize(10);
  }
  storeRecentFilePaths(paths);
  [NSDocumentController.sharedDocumentController noteNewRecentDocumentURL:[NSURL fileURLWithPath:toNSString(path)]];
  return true;
}

const char *ps_ui_recent_path(int32_t index) {
  State &s = state();
  const std::vector<std::string> paths = usable() ? recentFilePaths() : std::vector<std::string>();
  s.returnedText = index >= 0 && static_cast<size_t>(index) < paths.size() ? paths[static_cast<size_t>(index)]
                                                                           : std::string();
  return s.returnedText.c_str();
}

bool ps_ui_recent_clear(void) {
  if (!usable()) {
    return false;
  }
  State &s = state();
  if (s.prefOverridesActive) {
    s.recentOverride.clear();
  } else {
    storeRecentFilePaths({});
    [NSDocumentController.sharedDocumentController clearRecentDocuments:nil];
  }
  return true;
}

bool ps_ui_pref_get_bool(const char *key, bool fallback) {
  if (!usable() || key == nullptr || *key == '\0') {
    return fallback;
  }
  State &s = state();
  if (const auto it = s.prefOverrides.find(key); it != s.prefOverrides.end()) {
    return it->second;
  }
  if (s.prefOverridesActive) {
    return fallback;
  }
  NSString *name = [@"ps.pref." stringByAppendingString:toNSString(key)];
  NSUserDefaults *defaults = NSUserDefaults.standardUserDefaults;
  return [defaults objectForKey:name] != nil ? [defaults boolForKey:name] : fallback;
}

bool ps_ui_pref_set_bool(const char *key, bool value) {
  if (!usable() || key == nullptr || *key == '\0') {
    return false;
  }
  State &s = state();
  if (s.prefOverridesActive) {
    s.prefOverrides[key] = value;
    return true;
  }
  [NSUserDefaults.standardUserDefaults setBool:value forKey:[@"ps.pref." stringByAppendingString:toNSString(key)]];
  return true;
}

bool ps_ui_menu_add_separator(uint64_t menuHandle) {
  NSMenu *menu = usable() ? menuFor(menuHandle) : nil;
  if (menu == nil) {
    return false;
  }
  [menu addItem:[NSMenuItem separatorItem]];
  return true;
}

bool ps_ui_menu_add_standard(uint64_t menuHandle, int32_t standardId) {
  NSMenu *menu = usable() ? menuFor(menuHandle) : nil;
  if (menu == nil) {
    return false;
  }
  // Titles and shortcuts come from the table the headless backend reports too.
  const primec::ui::StandardItemInfo info = primec::ui::standardItemInfo(standardId);
  if (info.title == nullptr) {
    return false;
  }
  SEL action = nil;
  id target = nil; // nil: the first responder, so the focused text view handles it
  NSInteger itemTag = 0;
  switch (standardId) {
  case PS_UI_STANDARD_UNDO: action = @selector(undo:); break;
  case PS_UI_STANDARD_REDO: action = @selector(redo:); break;
  case PS_UI_STANDARD_CUT: action = @selector(cut:); break;
  case PS_UI_STANDARD_COPY: action = @selector(copy:); break;
  case PS_UI_STANDARD_PASTE: action = @selector(paste:); break;
  case PS_UI_STANDARD_SELECT_ALL: action = @selector(selectAll:); break;
  case PS_UI_STANDARD_FIND:
    action = @selector(performTextFinderAction:);
    itemTag = NSTextFinderActionShowFindInterface;
    break;
  case PS_UI_STANDARD_QUIT:
    action = @selector(quitRequested:);
    target = controller();
    break;
  case PS_UI_STANDARD_ABOUT:
    action = @selector(orderFrontStandardAboutPanel:);
    target = NSApp;
    break;
  case PS_UI_STANDARD_HIDE:
    action = @selector(hide:);
    target = NSApp;
    break;
  case PS_UI_STANDARD_HIDE_OTHERS:
    action = @selector(hideOtherApplications:);
    target = NSApp;
    break;
  case PS_UI_STANDARD_MINIMIZE: action = @selector(performMiniaturize:); break;
  case PS_UI_STANDARD_ZOOM: action = @selector(performZoom:); break;
  case PS_UI_STANDARD_BRING_ALL_TO_FRONT:
    action = @selector(arrangeInFront:);
    target = NSApp;
    break;
  case PS_UI_STANDARD_FULL_SCREEN: action = @selector(toggleFullScreen:); break;
  case PS_UI_STANDARD_HELP:
    action = @selector(showHelp:);
    target = NSApp;
    break;
  case PS_UI_STANDARD_SHOW_TAB_BAR: action = @selector(toggleTabBar:); break;
  case PS_UI_STANDARD_MERGE_ALL_WINDOWS: action = @selector(mergeAllWindows:); break;
  case PS_UI_STANDARD_MOVE_TAB_TO_NEW_WINDOW: action = @selector(moveTabToNewWindow:); break;
  case PS_UI_STANDARD_NEXT_TAB: action = @selector(selectNextTab:); break;
  case PS_UI_STANDARD_PREVIOUS_TAB: action = @selector(selectPreviousTab:); break;
  default:
    return false;
  }
  NSString *key = nil;
  NSEventModifierFlags mask = 0;
  parseShortcut(info.shortcut, &key, &mask);
  addMenuItem(menu, toNSString(info.title), action, target, key, mask, itemTag);
  return true;
}

bool ps_ui_menu_bar_add_role(uint64_t menuHandle, int32_t role) {
  NSMenu *menu = usable() ? menuFor(menuHandle) : nil;
  if (menu == nil || menu.supermenu != nil || role < PS_UI_MENU_ROLE_APP || role > PS_UI_MENU_ROLE_HELP) {
    return false;
  }
  NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:menu.title action:nil keyEquivalent:@""];
  item.submenu = menu;
  if (role == PS_UI_MENU_ROLE_APP) {
    if (NSApp.mainMenu.numberOfItems > 0) {
      [NSApp.mainMenu removeItemAtIndex:0]; // the default application menu
    }
    [NSApp.mainMenu insertItem:item atIndex:0];
  } else {
    [NSApp.mainMenu addItem:item];
    if (role == PS_UI_MENU_ROLE_WINDOW) {
      NSApp.windowsMenu = menu;
    } else {
      NSApp.helpMenu = menu;
    }
  }
  return true;
}

bool ps_ui_menu_bar_add(uint64_t menuHandle) {
  NSMenu *menu = usable() ? menuFor(menuHandle) : nil;
  if (menu == nil || menu.supermenu != nil) {
    return false;
  }
  NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:menu.title action:nil keyEquivalent:@""];
  item.submenu = menu;
  [NSApp.mainMenu addItem:item];
  return true;
}

const char *ps_ui_open_panel(const char *title) {
  State &s = state();
  s.returnedText.clear();
  s.panelChosen = false;
  if (!usable()) {
    return s.returnedText.c_str();
  }
  if (const char *answer = std::getenv("PRIMESTRUCT_UI_OPEN_PANEL"); answer != nullptr && *answer != '\0') {
    // Snapshot runs have nobody to click: the Open panel answers with this path.
    s.returnedText = answer;
    s.panelChosen = true;
    return s.returnedText.c_str();
  }
  @autoreleasepool {
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    panel.title = toNSString(title);
    panel.canChooseDirectories = NO;
    panel.allowsMultipleSelection = NO;
    if ([panel runModal] == NSModalResponseOK && panel.URL != nil) {
      s.returnedText = panel.URL.path.UTF8String;
      s.panelChosen = !s.returnedText.empty();
    }
  }
  return s.returnedText.c_str();
}

const char *ps_ui_save_panel(const char *title, const char *suggestedName) {
  State &s = state();
  s.returnedText.clear();
  s.panelChosen = false;
  if (!usable()) {
    return s.returnedText.c_str();
  }
  @autoreleasepool {
    NSSavePanel *panel = [NSSavePanel savePanel];
    panel.title = toNSString(title);
    panel.nameFieldStringValue = toNSString(suggestedName);
    if ([panel runModal] == NSModalResponseOK && panel.URL != nil) {
      s.returnedText = panel.URL.path.UTF8String;
      s.panelChosen = !s.returnedText.empty();
    }
  }
  return s.returnedText.c_str();
}

bool ps_ui_panel_chosen(void) { return state().panelChosen; }

int32_t ps_ui_alert(const char *message, const char *detail, const char *buttons) {
  if (!usable()) {
    return -1;
  }
  @autoreleasepool {
    NSAlert *alert = [[NSAlert alloc] init];
    alert.messageText = toNSString(message);
    alert.informativeText = toNSString(detail);
    NSArray<NSString *> *titles = [toNSString(buttons) componentsSeparatedByString:@"\n"];
    for (NSString *title in titles) {
      [alert addButtonWithTitle:title.length > 0 ? title : @"OK"];
    }
    const NSModalResponse response = [alert runModal];
    return static_cast<int32_t>(response - NSAlertFirstButtonReturn);
  }
}

} // extern "C"
