/* The platform's share of a launch, for the image-count guard of PrimeStruct native UI apps (TODO-5603,
 * cmake/NativeUiStartupGuardTest.cmake): a plain AppKit program with one window and one text view, linked against
 * AppKit and Foundation only, that shows nothing on screen (transparent, parked like a snapshot run's window) and exits
 * once its window has been drawn. The images dyld loads for it are what any AppKit app loads on this macOS; a
 * PrimeStruct app may load only a few more.
 *
 * Build: clang -fobjc-arc -framework AppKit -o reference_app reference_app.m
 */
#import <AppKit/AppKit.h>

int main(void) {
  @autoreleasepool {
    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyProhibited];
    [NSApp finishLaunching];
    NSWindow *window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 880, 480)
                                                   styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                                                             NSWindowStyleMaskMiniaturizable |
                                                             NSWindowStyleMaskResizable
                                                     backing:NSBackingStoreBuffered
                                                       defer:NO];
    window.releasedWhenClosed = NO;
    window.restorable = NO;
    window.alphaValue = 0;
    window.ignoresMouseEvents = YES;
    window.hasShadow = NO;
    NSScrollView *scroll = [NSTextView scrollableTextView];
    window.contentView = scroll;
    [window orderFrontRegardless];
    NSRect screens = NSZeroRect;
    for (NSScreen *screen in [NSScreen screens]) {
      screens = NSUnionRect(screens, screen.frame);
    }
    [window setFrameOrigin:NSMakePoint(NSMaxX(screens) + 2000, NSMinY(screens))];
    NSDate *until = [NSDate dateWithTimeIntervalSinceNow:0.3];
    while ([until timeIntervalSinceNow] > 0) {
      NSEvent *event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                          untilDate:until
                                             inMode:NSDefaultRunLoopMode
                                            dequeue:YES];
      if (event != nil) {
        [NSApp sendEvent:event];
      }
    }
  }
  return 0;
}
