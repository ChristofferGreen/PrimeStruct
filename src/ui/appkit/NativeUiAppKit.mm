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

#import <AppKit/AppKit.h>
#import <objc/runtime.h>

#include "primec/ui/NativeUi.h"
#include "primec/ui/NativeUiStandardItems.h"
#include "primec/ui/SyntaxHighlight.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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
  std::unordered_map<uint64_t, NSFont *> baseFonts;
  // A text view does not retain its scroll view, so the maps do.
  std::unordered_map<uint64_t, NSScrollView *> scrollViews;
  std::unordered_map<uint64_t, NSMenu *> menus;
  std::unordered_map<uint64_t, bool> modified;
  std::deque<PendingEvent> pending;
  PendingEvent current;
  std::string returnedText;
  std::string lastError;
  bool panelChosen = false;
  std::string snapshotPath;
  std::string snapshotTypeText;
  std::string snapshotOpenPath;
  std::string snapshotMenu;
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
  [view cacheDisplayInRect:bounds toBitmapImageRep:rep];
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
    if (const char *open = std::getenv("PRIMESTRUCT_UI_OPEN"); open != nullptr && *open != '\0') {
      s.snapshotOpenPath = open;
    }
    if (const char *menu = std::getenv("PRIMESTRUCT_UI_MENU"); menu != nullptr && *menu != '\0') {
      s.snapshotMenu = menu;
    }
    if (const char *typed = std::getenv("PRIMESTRUCT_UI_TYPE"); typed != nullptr && *typed != '\0') {
      s.snapshotTypeText = typed;
      for (size_t at = s.snapshotTypeText.find("\\n"); at != std::string::npos;
           at = s.snapshotTypeText.find("\\n", at + 1)) {
        s.snapshotTypeText.replace(at, 2, "\n");
      }
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
        if (!s.snapshotOpenPath.empty() && !s.snapshotOpened && s.snapshotIdleSlices >= 4) {
          // Opens a file into the first text view, as the program would on Open.
          s.snapshotOpened = true;
          s.snapshotIdleSlices = 0;
          uint64_t lowest = UINT64_MAX;
          for (const auto &entry : s.textViews) {
            lowest = std::min(lowest, entry.first);
          }
          if (lowest != UINT64_MAX) {
            if (!ps_ui_text_view_load_file(lowest, s.snapshotOpenPath.c_str())) {
              std::fprintf(stderr, "native ui: %s\n", s.lastError.c_str());
            }
            ps_ui_text_view_highlight(lowest, s.snapshotOpenPath.c_str());
            ps_ui_window_set_title(s.snapshotWindow, s.snapshotOpenPath.c_str());
          }
        } else if (!s.snapshotTypeText.empty() && !s.snapshotTyped && s.snapshotIdleSlices >= 6) {
          // Types into the first text view so the picture shows an edited document.
          s.snapshotTyped = true;
          s.snapshotIdleSlices = 0;
          NSTextView *target = nil;
          uint64_t lowest = UINT64_MAX;
          for (const auto &entry : s.textViews) {
            if (entry.first < lowest) {
              lowest = entry.first;
              target = entry.second;
            }
          }
          if (target != nil) {
            [target.window makeFirstResponder:target];
            [target insertText:toNSString(s.snapshotTypeText.c_str()) replacementRange:NSMakeRange(NSNotFound, 0)];
          }
        } else if (s.snapshotIdleSlices >= 12) {
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
  v.font = monospace ? [NSFont monospacedSystemFontOfSize:13 weight:NSFontWeightRegular]
                     : [NSFont systemFontOfSize:13];
  state().baseFonts[view] = v.font;
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

bool ps_ui_text_view_highlight(uint64_t view, const char *path) {
  return usable() && textView(view) != nil && primec::ui::applyHighlight(view, path);
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
