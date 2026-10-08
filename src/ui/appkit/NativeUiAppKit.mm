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
// ps_ui_wait_event then reports a quit request.

#import <AppKit/AppKit.h>
#import <objc/runtime.h>

#include "primec/ui/NativeUi.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace {

struct PendingEvent {
  int32_t kind = PS_UI_EVENT_NONE;
  uint64_t window = 0;
  uint64_t widget = 0;
  int32_t command = 0;
};

struct State {
  bool initialized = false;
  bool quitting = false;
  std::thread::id owner;
  uint64_t nextHandle = 1;
  std::unordered_map<uint64_t, NSWindow *> windows;
  std::unordered_map<uint64_t, NSTextView *> textViews;
  // A text view does not retain its scroll view, so the maps do.
  std::unordered_map<uint64_t, NSScrollView *> scrollViews;
  std::unordered_map<uint64_t, NSMenu *> menus;
  std::unordered_map<uint64_t, bool> modified;
  std::deque<PendingEvent> pending;
  PendingEvent current;
  std::string returnedText;
  std::string snapshotPath;
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

} // namespace

@interface PSController : NSObject <NSApplicationDelegate, NSWindowDelegate, NSTextViewDelegate>
- (void)menuCommand:(id)sender;
- (void)quitRequested:(id)sender;
@end

@implementation PSController
- (void)menuCommand:(id)sender {
  enqueue(PS_UI_EVENT_COMMAND, 0, 0, static_cast<int32_t>([sender tag]));
}

- (void)quitRequested:(id)sender {
  enqueue(PS_UI_EVENT_QUIT_REQUESTED);
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

void captureSnapshot() {
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
  [view cacheDisplayInRect:bounds toBitmapImageRep:rep];
  NSData *png = [rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
  NSString *path = toNSString(s.snapshotPath.c_str());
  NSError *error = nil;
  if (png == nil || ![png writeToFile:path options:NSDataWritingAtomic error:&error]) {
    std::fprintf(stderr, "native ui: could not write snapshot %s: %s\n", s.snapshotPath.c_str(),
                 error != nil ? error.localizedDescription.UTF8String : "no PNG data");
  }
}

} // namespace

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
      if (snapshotting && event == nil && ++s.snapshotIdleSlices >= 12) {
        captureSnapshot();
        enqueue(PS_UI_EVENT_QUIT_REQUESTED);
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
  NSTextView *view = usable() ? textView(widget) : nil;
  if (w == nil || view == nil || (view.window != nil && view.window != w)) {
    return false;
  }
  NSScrollView *scroll = state().scrollViews[widget];
  NSRect contentRect = [w contentRectForFrameRect:w.frame];
  contentRect.origin = NSZeroPoint;
  scroll.frame = contentRect;
  scroll.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
  w.contentView = scroll;
  [w makeFirstResponder:view];
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
  return true;
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
  v.font = monospace ? [NSFont monospacedSystemFontOfSize:13 weight:NSFontWeightRegular]
                     : [NSFont systemFontOfSize:13];
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
  const NSEventModifierFlags cmd = NSEventModifierFlagCommand;
  switch (standardId) {
  case PS_UI_STANDARD_UNDO:
    addMenuItem(menu, @"Undo", @selector(undo:), nil, @"z", cmd, 0);
    return true;
  case PS_UI_STANDARD_REDO:
    addMenuItem(menu, @"Redo", @selector(redo:), nil, @"z", cmd | NSEventModifierFlagShift, 0);
    return true;
  case PS_UI_STANDARD_CUT:
    addMenuItem(menu, @"Cut", @selector(cut:), nil, @"x", cmd, 0);
    return true;
  case PS_UI_STANDARD_COPY:
    addMenuItem(menu, @"Copy", @selector(copy:), nil, @"c", cmd, 0);
    return true;
  case PS_UI_STANDARD_PASTE:
    addMenuItem(menu, @"Paste", @selector(paste:), nil, @"v", cmd, 0);
    return true;
  case PS_UI_STANDARD_SELECT_ALL:
    addMenuItem(menu, @"Select All", @selector(selectAll:), nil, @"a", cmd, 0);
    return true;
  case PS_UI_STANDARD_FIND:
    addMenuItem(menu, @"Find…", @selector(performTextFinderAction:), nil, @"f", cmd, NSTextFinderActionShowFindInterface);
    return true;
  case PS_UI_STANDARD_QUIT:
    addMenuItem(menu, @"Quit", @selector(quitRequested:), controller(), @"q", cmd, 0);
    return true;
  case PS_UI_STANDARD_ABOUT:
    addMenuItem(menu, @"About", @selector(orderFrontStandardAboutPanel:), NSApp, @"", 0, 0);
    return true;
  default:
    return false;
  }
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
  if (!usable()) {
    return s.returnedText.c_str();
  }
  @autoreleasepool {
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    panel.title = toNSString(title);
    panel.canChooseDirectories = NO;
    panel.allowsMultipleSelection = NO;
    if ([panel runModal] == NSModalResponseOK && panel.URL != nil) {
      s.returnedText = panel.URL.path.UTF8String;
    }
  }
  return s.returnedText.c_str();
}

const char *ps_ui_save_panel(const char *title, const char *suggestedName) {
  State &s = state();
  s.returnedText.clear();
  if (!usable()) {
    return s.returnedText.c_str();
  }
  @autoreleasepool {
    NSSavePanel *panel = [NSSavePanel savePanel];
    panel.title = toNSString(title);
    panel.nameFieldStringValue = toNSString(suggestedName);
    if ([panel runModal] == NSModalResponseOK && panel.URL != nil) {
      s.returnedText = panel.URL.path.UTF8String;
    }
  }
  return s.returnedText.c_str();
}

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
