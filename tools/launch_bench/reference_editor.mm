/* The reference editor of the launch-time harness (docs/NativeUiPlan.md section 1, scripts/measure_startup.sh).
 *
 * A text editor written directly in Objective-C++ with the window, tab, menu, toolbar and text view setup of the
 * PrimeStruct editor (src/ui/appkit/NativeUiAppKit.mm, examples/apps/text_editor/main.prime) and the startup rules of
 * section 1: AppKit and Foundation only, a plain NSWindow, no window restoration, no AutoFill, completion or Writing
 * Tools, the window shown before anything else is loaded, the file loaded in one batch, no child processes. It is
 * the yardstick for the budget "within 5 ms of a pure Objective-C++ editor": what the PrimeStruct editor costs beyond
 * this program is PrimeStruct's own share of a launch.
 *
 * It writes its own phase marks in the format of tools/startup_probe/startup_probe.c (one `phase ...` line), so
 * scripts/measure_startup.sh --marks self reads it like a probed PrimeStruct app:
 *   main      the constructor: dyld has loaded and bound every image
 *   app_init  NSApplication, the default registrations, the menu bar and finishLaunching are done
 *   loop      the first wait for an event: everything before the event loop is done
 *   window    the first -[NSWindow displayIfNeeded];  text  the first -[NSTextView drawRect:]
 * and the peak resident set. Environment, as for the probe: PS_PROBE_OUT (append the report there, default stderr),
 * PS_PROBE_T0_US (the launch start, microseconds of the wall clock; default the kernel's process start),
 * PS_PROBE_EXIT_AFTER_TEXT=1 (write the report and exit when the first text is drawn, for a launch through `open`),
 * PS_PROBE_EXIT_DELAY_MS (keep running that long after the report before exiting, so a window-list poller still sees the window).
 * PRIMESTRUCT_UI_SNAPSHOT=<any value> runs it like the PrimeStruct snapshot mode: no Dock icon, no activation, a
 * transparent window parked off screen, and an exit once the first text was drawn and the program waited once more.
 *
 * Usage: ReferenceEditor [file]   (also an open-file event; a .prime, .md or .markdown file gets the toolbar of a
 * script or markdown tab)
 * Build (scripts/measure_startup.sh does this): clang++ -std=c++20 -fobjc-arc -framework AppKit
 *   -framework Foundation -o ReferenceEditor reference_editor.mm
 */
#import <AppKit/AppKit.h>
#import <objc/message.h>
#import <objc/runtime.h>

#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <sys/sysctl.h>
#include <time.h>
#include <unistd.h>

static double nowUs() {
  struct timespec t;
  clock_gettime(CLOCK_REALTIME, &t);
  return t.tv_sec * 1e6 + t.tv_nsec / 1e3;
}

static double MarkStart, MarkMain, MarkInit, MarkLoop, MarkWindow, MarkText;
static bool Reported = false;
static bool SnapshotMode = false;

static void writeReport() {
  if (Reported) {
    return;
  }
  Reported = true;
  const double end = nowUs();
  const char *path = getenv("PS_PROBE_OUT");
  FILE *out = path != nullptr ? fopen(path, "a") : stderr;
  if (out == nullptr) {
    return;
  }
  auto rel = [](double mark) { return mark != 0 ? (mark - MarkStart) / 1000.0 : -1.0; };
  struct rusage usage;
  getrusage(RUSAGE_SELF, &usage);
  fprintf(out,
          "phase main_ms=%.1f app_init_ms=%.1f loop_ms=%.1f window_ms=%.1f text_ms=%.1f frame_ms=-1.0 settled_ms=-1.0"
          " end_ms=%.1f majflt=0 minflt=0 maxrss_kb=%ld\n",
          rel(MarkMain), rel(MarkInit), rel(MarkLoop), rel(MarkWindow), rel(MarkText), rel(end),
          (long)(usage.ru_maxrss / 1024));
  if (out != stderr) {
    fclose(out);
  }
}

__attribute__((constructor)) static void markMain() {
  MarkMain = nowUs();
  MarkStart = MarkMain;
  struct kinfo_proc info;
  size_t size = sizeof info;
  int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid()};
  if (sysctl(mib, 4, &info, &size, nullptr, 0) == 0) {
    MarkStart = info.kp_proc.p_starttime.tv_sec * 1e6 + info.kp_proc.p_starttime.tv_usec;
  }
  if (const char *t0 = getenv("PS_PROBE_T0_US")) {
    MarkStart = atof(t0);
  }
  atexit(writeReport);
}

// The first display and the first drawn text, hooked as the probe does.
static IMP TextDrawImp, DisplayImp;

static void hookedTextDraw(id self, SEL sel, NSRect rect) {
  ((void (*)(id, SEL, NSRect))TextDrawImp)(self, sel, rect);
  if (MarkText == 0) {
    MarkText = nowUs();
    if (getenv("PS_PROBE_EXIT_AFTER_TEXT") != nullptr) {
      writeReport();
      if (const char *delay = getenv("PS_PROBE_EXIT_DELAY_MS")) {
        // The event loop goes on, so the window is committed and listed for a window-list poller.
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(atof(delay) * 1e6)), dispatch_get_main_queue(), ^{
          _exit(0);
        });
        return;
      }
      _exit(0);
    }
  }
}

static void hookedDisplay(id self, SEL sel) {
  if (MarkWindow == 0) {
    MarkWindow = nowUs();
  }
  ((void (*)(id, SEL))DisplayImp)(self, sel);
}

static void installHooks() {
  Method draw = class_getInstanceMethod([NSTextView class], @selector(drawRect:));
  TextDrawImp = method_getImplementation(draw);
  method_setImplementation(draw, (IMP)hookedTextDraw);
  Method display = class_getInstanceMethod([NSWindow class], @selector(displayIfNeeded));
  DisplayImp = method_getImplementation(display);
  method_setImplementation(display, (IMP)hookedDisplay);
}

// ---- the menu bar: the application, File, Edit, Format, View, Run, Window and Help menus of the editor ----------------

@interface RefTarget : NSObject
- (void)command:(id)sender;
@end
@implementation RefTarget
- (void)command:(id)sender {
}
@end

static RefTarget *Target;

static void addItem(NSMenu *menu, NSString *title, NSString *key, NSEventModifierFlags flags = NSEventModifierFlagCommand) {
  NSMenuItem *item = [menu addItemWithTitle:title action:@selector(command:) keyEquivalent:key];
  item.target = Target;
  item.keyEquivalentModifierMask = flags;
}

static void addStandard(NSMenu *menu, NSString *title, SEL action, NSString *key,
                        NSEventModifierFlags flags = NSEventModifierFlagCommand) {
  NSMenuItem *item = [menu addItemWithTitle:title action:action keyEquivalent:key];
  item.keyEquivalentModifierMask = flags;
}

static NSMenu *addMenu(NSMenu *bar, NSString *title) {
  NSMenuItem *holder = [bar addItemWithTitle:title action:nil keyEquivalent:@""];
  NSMenu *menu = [[NSMenu alloc] initWithTitle:title];
  holder.submenu = menu;
  return menu;
}

static void addSubmenu(NSMenu *menu, NSString *title, NSArray<NSString *> *items) {
  NSMenu *sub = addMenu(menu, title);
  for (NSString *name in items) {
    addItem(sub, name, @"");
  }
}

static void buildMenuBar(NSString *appName) {
  Target = [[RefTarget alloc] init];
  NSMenu *bar = [[NSMenu alloc] initWithTitle:@""];
  NSMenu *app = addMenu(bar, appName);
  addStandard(app, [@"About " stringByAppendingString:appName], @selector(orderFrontStandardAboutPanel:), @"");
  [app addItem:NSMenuItem.separatorItem];
  addItem(app, @"Settings...", @",");
  [app addItem:NSMenuItem.separatorItem];
  addStandard(app, [@"Hide " stringByAppendingString:appName], @selector(hide:), @"h");
  addStandard(app, @"Hide Others", @selector(hideOtherApplications:), @"h",
              NSEventModifierFlagCommand | NSEventModifierFlagOption);
  addStandard(app, @"Show All", @selector(unhideAllApplications:), @"");
  [app addItem:NSMenuItem.separatorItem];
  addStandard(app, [@"Quit " stringByAppendingString:appName], @selector(terminate:), @"q");

  NSMenu *file = addMenu(bar, @"File");
  addItem(file, @"New", @"n");
  addItem(file, @"New Tab", @"t");
  addItem(file, @"Open...", @"o");
  addSubmenu(file, @"Open Recent", @[ @"Clear Menu" ]);
  [file addItem:NSMenuItem.separatorItem];
  addStandard(file, @"Close", @selector(performClose:), @"w");
  addItem(file, @"Save", @"s");
  addItem(file, @"Save As...", @"S", NSEventModifierFlagCommand | NSEventModifierFlagShift);
  addItem(file, @"Revert to Saved", @"");
  [file addItem:NSMenuItem.separatorItem];
  addItem(file, @"Export as HTML...", @"");
  addItem(file, @"Print...", @"p");

  NSMenu *edit = addMenu(bar, @"Edit");
  addStandard(edit, @"Undo", @selector(undo:), @"z");
  addStandard(edit, @"Redo", @selector(redo:), @"Z", NSEventModifierFlagCommand | NSEventModifierFlagShift);
  [edit addItem:NSMenuItem.separatorItem];
  addStandard(edit, @"Cut", @selector(cut:), @"x");
  addStandard(edit, @"Copy", @selector(copy:), @"c");
  addStandard(edit, @"Paste", @selector(paste:), @"v");
  addStandard(edit, @"Delete", @selector(delete:), @"");
  addStandard(edit, @"Select All", @selector(selectAll:), @"a");
  [edit addItem:NSMenuItem.separatorItem];
  NSMenu *find = addMenu(edit, @"Find");
  for (NSString *name in @[ @"Find...", @"Find Next", @"Find Previous", @"Use Selection for Find", @"Replace...",
                            @"Go to Line..." ]) {
    addItem(find, name, @"");
  }
  addSubmenu(edit, @"Spelling and Grammar", @[ @"Show Spelling", @"Check Now", @"Check While Typing" ]);

  NSMenu *format = addMenu(bar, @"Format");
  for (NSString *name in @[ @"Bold", @"Italic", @"Strikethrough", @"Inline Code", @"Link", @"Image" ]) {
    addItem(format, name, @"");
  }
  [format addItem:NSMenuItem.separatorItem];
  addSubmenu(format, @"Heading", @[ @"Heading 1", @"Heading 2", @"Heading 3", @"Paragraph" ]);
  addSubmenu(format, @"Blocks", @[ @"Bulleted List", @"Numbered List", @"Task List", @"Quote", @"Code Block",
                                   @"Horizontal Rule" ]);
  addSubmenu(format, @"Table", @[ @"Insert 2 by 2", @"Insert 3 by 3", @"Insert 4 by 3", @"Insert 5 by 4",
                                  @"Format Table", @"Add Row", @"Add Column", @"Delete Row", @"Delete Column",
                                  @"Align Left", @"Align Center", @"Align Right" ]);
  addSubmenu(format, @"Indent", @[ @"Indent", @"Outdent", @"Comment Selection", @"Sort Lines" ]);

  NSMenu *view = addMenu(bar, @"View");
  for (NSString *name in @[ @"Show Line Numbers", @"Word Wrap", @"Editor Only", @"Preview Only",
                            @"Editor and Preview", @"Show Output" ]) {
    addItem(view, name, @"");
  }
  [view addItem:NSMenuItem.separatorItem];
  addItem(view, @"Make Text Bigger", @"+");
  addItem(view, @"Make Text Smaller", @"-");
  addItem(view, @"Actual Size", @"0");
  addSubmenu(view, @"Font", @[ @"Monospaced", @"Proportional", @"Choose..." ]);
  addSubmenu(view, @"Theme", @[ @"Automatic", @"Light", @"Dark" ]);
  addStandard(view, @"Enter Full Screen", @selector(toggleFullScreen:), @"f",
              NSEventModifierFlagCommand | NSEventModifierFlagControl);

  NSMenu *run = addMenu(bar, @"Run");
  for (NSString *name in @[ @"Run Script", @"Stop Script", @"Clear Output", @"Run Selection", @"Show Diagnostics" ]) {
    addItem(run, name, @"");
  }

  NSMenu *windowMenu = addMenu(bar, @"Window");
  addStandard(windowMenu, @"Minimize", @selector(performMiniaturize:), @"m");
  addStandard(windowMenu, @"Zoom", @selector(performZoom:), @"");
  [windowMenu addItem:NSMenuItem.separatorItem];
  addStandard(windowMenu, @"Show Next Tab", @selector(selectNextTab:), @"");
  addStandard(windowMenu, @"Show Previous Tab", @selector(selectPreviousTab:), @"");
  addStandard(windowMenu, @"Move Tab to New Window", @selector(moveTabToNewWindow:), @"");
  addStandard(windowMenu, @"Merge All Windows", @selector(mergeAllWindows:), @"");
  addStandard(windowMenu, @"Bring All to Front", @selector(arrangeInFront:), @"");
  NSApp.windowsMenu = windowMenu;

  NSMenu *help = addMenu(bar, @"Help");
  addItem(help, [appName stringByAppendingString:@" Help"], @"?");
  NSApp.helpMenu = help;
  NSApp.mainMenu = bar;
}

// ---- the toolbar of a script or markdown tab ---------------------------------------------------------------------

@interface RefToolbarDelegate : NSObject <NSToolbarDelegate>
@end
@implementation RefToolbarDelegate
- (NSArray<NSToolbarItemIdentifier> *)toolbarAllowedItemIdentifiers:(NSToolbar *)toolbar {
  return @[ @"run", @"stop", @"bold", @"italic", @"code", @"heading", NSToolbarFlexibleSpaceItemIdentifier ];
}
- (NSArray<NSToolbarItemIdentifier> *)toolbarDefaultItemIdentifiers:(NSToolbar *)toolbar {
  return @[ @"run", @"stop", NSToolbarFlexibleSpaceItemIdentifier, @"bold", @"italic", @"code", @"heading" ];
}
- (NSToolbarItem *)toolbar:(NSToolbar *)toolbar
        itemForItemIdentifier:(NSToolbarItemIdentifier)identifier
    willBeInsertedIntoToolbar:(BOOL)flag {
  static NSDictionary<NSString *, NSString *> *symbols = @{
    @"run" : @"play.fill",
    @"stop" : @"stop.fill",
    @"bold" : @"bold",
    @"italic" : @"italic",
    @"code" : @"chevron.left.forwardslash.chevron.right",
    @"heading" : @"textformat.size"
  };
  NSToolbarItem *item = [[NSToolbarItem alloc] initWithItemIdentifier:identifier];
  item.label = identifier;
  item.image = [NSImage imageWithSystemSymbolName:symbols[identifier] accessibilityDescription:identifier];
  item.target = Target;
  item.action = @selector(command:);
  return item;
}
@end

// ---- the window ---------------------------------------------------------------------------------------------------

static NSString *readFile(const char *path) {
  NSData *data = [NSData dataWithContentsOfFile:[NSString stringWithUTF8String:path]];
  return data != nil ? [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding] : nil;
}

static bool wantsToolbar(NSString *path) {
  NSString *ext = path.pathExtension.lowercaseString;
  return [ext isEqualToString:@"prime"] || [ext isEqualToString:@"md"] || [ext isEqualToString:@"markdown"];
}

// The document of the launch: an argument, or the open-file event Launch Services sends (the Finder and `open` pass a
// file that way). The toolbar and the title are set before the window is shown; the text goes in afterwards.
static NSString *DocumentPath;
static NSWindow *MainWindow;
static NSTextView *MainView;

static void showDocumentChrome(NSString *path) {
  if (wantsToolbar(path)) {
    NSToolbar *bar = [[NSToolbar alloc] initWithIdentifier:@"ref.toolbar"];
    static RefToolbarDelegate *delegate = [[RefToolbarDelegate alloc] init];
    bar.delegate = delegate;
    bar.displayMode = NSToolbarDisplayModeIconOnly;
    bar.allowsUserCustomization = NO;
    MainWindow.toolbar = bar;
  }
  MainWindow.title = path.lastPathComponent;
}

static void loadDocumentText(NSString *path) {
  NSString *text = readFile(path.UTF8String);
  if (text != nil) {
    MainView.string = text;
    [MainView.undoManager removeAllActions];
  }
}

@interface RefAppDelegate : NSObject <NSApplicationDelegate>
@end
@implementation RefAppDelegate
- (BOOL)application:(NSApplication *)sender openFile:(NSString *)filename {
  if (DocumentPath == nil) {
    DocumentPath = filename;
    if (MainWindow != nil) { // the event came after the window was built: it is the first document
      showDocumentChrome(filename);
      loadDocumentText(filename);
    }
  }
  return YES;
}
@end

int main(int argc, char **argv) {
  @autoreleasepool {
    SnapshotMode = getenv("PRIMESTRUCT_UI_SNAPSHOT") != nullptr;
    installHooks();
    // The launch work an editor has no use for, switched off as the PrimeStruct backend does.
    [NSUserDefaults.standardUserDefaults registerDefaults:@{
      @"ApplePersistenceIgnoreState" : @YES,
      @"ApplePersistenceIgnoreStateQuietly" : @YES,
      @"NSQuitAlwaysKeepsWindows" : @NO,
      @"NSAutoFillHeuristicsEnabled" : @NO,
      @"NSEnableTextInputContextBasedAutoFill" : @NO,
      @"NSAutoFillSystemInsertMenuEnabled" : @NO,
      @"NSMenuDoesNotAutomaticallyInsertWritingToolsItems" : @YES,
    }];
    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:SnapshotMode ? NSApplicationActivationPolicyProhibited
                                            : NSApplicationActivationPolicyRegular];
    buildMenuBar(@"ReferenceEditor");
    static RefAppDelegate *appDelegate = [[RefAppDelegate alloc] init];
    NSApp.delegate = appDelegate;
    [NSApp finishLaunching];
    MarkInit = nowUs();

    const NSWindowStyleMask style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                                    NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable;
    NSWindow *window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 880, 480)
                                                   styleMask:style
                                                     backing:NSBackingStoreBuffered
                                                       defer:NO];
    window.releasedWhenClosed = NO;
    window.restorable = NO;
    window.title = @"Untitled";
    window.tabbingMode = NSWindowTabbingModePreferred;
    window.tabbingIdentifier = @"ReferenceDocument";
    [window center];
    if (SnapshotMode) {
      window.alphaValue = 0;
      window.ignoresMouseEvents = YES;
      window.hasShadow = NO;
    }

    NSScrollView *scroll = [NSTextView scrollableTextView];
    NSTextView *view = scroll.documentView;
    view.richText = NO;
    view.allowsUndo = YES;
    view.automaticQuoteSubstitutionEnabled = NO;
    view.automaticDashSubstitutionEnabled = NO;
    view.automaticTextReplacementEnabled = NO;
    view.automaticSpellingCorrectionEnabled = NO;
    view.automaticTextCompletionEnabled = NO;
    view.inlinePredictionType = NSTextInputTraitTypeNo;
    view.writingToolsBehavior = NSWritingToolsBehaviorNone;
    view.textContainerInset = NSMakeSize(8, 8);
    view.font = [NSFont monospacedSystemFontOfSize:13 weight:NSFontWeightRegular];
    window.contentView = scroll;
    [window makeFirstResponder:view];

    MainWindow = window;
    MainView = view;
    if (DocumentPath == nil && argc > 1) {
      DocumentPath = [NSString stringWithUTF8String:argv[1]];
    }
    if (DocumentPath != nil) {
      showDocumentChrome(DocumentPath);
    }

    if (SnapshotMode) {
      [window orderFrontRegardless];
      NSRect screens = NSZeroRect;
      for (NSScreen *screen in [NSScreen screens]) {
        screens = NSUnionRect(screens, screen.frame);
      }
      [window setFrameOrigin:NSMakePoint(NSMaxX(screens) + 2000, NSMinY(screens))];
    } else {
      [window makeKeyAndOrderFront:nil];
      [NSApp activateIgnoringOtherApps:YES];
    }

    // The window is up; the file goes into the text storage in one batch.
    if (DocumentPath != nil) {
      loadDocumentText(DocumentPath);
    }

    double textSeenAt = 0;
    const double giveUpAt = nowUs() + 10e6; // a snapshot run that never draws text must not stay behind
    while (true) {
      if (SnapshotMode && nowUs() > giveUpAt) {
        return 1;
      }
      if (MarkLoop == 0) {
        MarkLoop = nowUs();
      }
      if (SnapshotMode && MarkText != 0) {
        if (textSeenAt == 0) {
          textSeenAt = nowUs();
        } else if (nowUs() - textSeenAt > 100000) {
          return 0;
        }
      }
      NSEvent *event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                          untilDate:SnapshotMode ? [NSDate dateWithTimeIntervalSinceNow:0.05]
                                                                 : [NSDate distantFuture]
                                             inMode:NSDefaultRunLoopMode
                                            dequeue:YES];
      if (event != nil) {
        [NSApp sendEvent:event];
      }
    }
  }
}
