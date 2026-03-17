#pragma once

#import <AppKit/AppKit.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include "gfx_contract_shared.h"

#include <iostream>
#include <string>

namespace primestruct::native_metal_window {

using GfxErrorCode = gfx_contract::GfxErrorCode;

enum class StartupFailureStage {
  SimulationStreamLoad,
  GpuDeviceAcquisition,
  ShaderLoad,
  PipelineSetup,
  WindowCreation,
  FirstFrameSubmission,
};

inline const char *startupStageName(StartupFailureStage stage) {
  switch (stage) {
    case StartupFailureStage::SimulationStreamLoad:
      return "simulation_stream_load";
    case StartupFailureStage::GpuDeviceAcquisition:
      return "gpu_device_acquisition";
    case StartupFailureStage::ShaderLoad:
      return "shader_load";
    case StartupFailureStage::PipelineSetup:
      return "pipeline_setup";
    case StartupFailureStage::WindowCreation:
      return "window_creation";
    case StartupFailureStage::FirstFrameSubmission:
      return "first_frame_submission";
  }
  return "unknown";
}

inline int startupStageExitCode(StartupFailureStage stage) {
  switch (stage) {
    case StartupFailureStage::SimulationStreamLoad:
      return 69;
    case StartupFailureStage::GpuDeviceAcquisition:
      return 70;
    case StartupFailureStage::ShaderLoad:
      return 71;
    case StartupFailureStage::PipelineSetup:
      return 72;
    case StartupFailureStage::WindowCreation:
      return 73;
    case StartupFailureStage::FirstFrameSubmission:
      return 74;
  }
  return 78;
}

struct FailureInfo {
  StartupFailureStage startupStage = StartupFailureStage::FirstFrameSubmission;
  int runtimeExitCode = 0;
  const char *reason = "";
  std::string details;
  GfxErrorCode gfxErrorCode = GfxErrorCode::None;
};

struct RenderFrameResult {
  bool ok = true;
  bool renderedFrame = false;
  bool requestExit = false;
  int exitCode = 0;
  const char *exitReason = "";
  FailureInfo failure;
};

struct PresenterConfig {
  int maxFrames = 0;
  CGFloat windowWidth = 960.0;
  CGFloat windowHeight = 640.0;
  NSString *windowTitle = nil;
  NSApplicationActivationPolicy activationPolicy = NSApplicationActivationPolicyRegular;
  const char *gfxProfile = "";
  bool activateApp = true;
};

using PrepareContentFn = bool (*)(void *context,
                                  id<MTLDevice> device,
                                  id<MTLCommandQueue> queue,
                                  FailureInfo &failureOut);
using PresenterReadyFn = void (*)(void *context);
using RenderFrameFn = RenderFrameResult (*)(void *context,
                                            id<MTLCommandQueue> queue,
                                            id<CAMetalDrawable> drawable,
                                            CGSize drawableSize,
                                            bool firstFrameSubmitted);
using ShutdownFn = void (*)(void *context);

struct PresenterCallbacks {
  PrepareContentFn prepare = nullptr;
  PresenterReadyFn presenterReady = nullptr;
  RenderFrameFn render = nullptr;
  ShutdownFn shutdown = nullptr;
};

inline int emitStartupFailure(const char *gfxProfile,
                              StartupFailureStage stage,
                              const char *reason,
                              const std::string &details,
                              GfxErrorCode gfxErrorCode) {
  const int exitCode = startupStageExitCode(stage);
  std::cerr << "startup_failure=1\n";
  std::cerr << "gfx_profile=" << gfxProfile << "\n";
  std::cerr << "startup_failure_stage=" << startupStageName(stage) << "\n";
  std::cerr << "startup_failure_reason=" << reason << "\n";
  std::cerr << "startup_failure_exit_code=" << exitCode << "\n";
  if (gfxErrorCode != GfxErrorCode::None) {
    std::cerr << "gfx_error_code=" << gfx_contract::gfxErrorCodeName(gfxErrorCode) << "\n";
  }
  if (!details.empty()) {
    std::cerr << "startup_failure_detail=" << details << "\n";
    if (gfxErrorCode != GfxErrorCode::None) {
      std::cerr << "gfx_error_why=" << details << "\n";
    }
  }
  return exitCode;
}

inline int emitRuntimeFailure(const char *gfxProfile,
                              int exitCode,
                              const char *reason,
                              const std::string &details,
                              GfxErrorCode gfxErrorCode) {
  std::cerr << "runtime_failure=1\n";
  std::cerr << "gfx_profile=" << gfxProfile << "\n";
  std::cerr << "runtime_failure_reason=" << reason << "\n";
  std::cerr << "runtime_failure_exit_code=" << exitCode << "\n";
  if (gfxErrorCode != GfxErrorCode::None) {
    std::cerr << "gfx_error_code=" << gfx_contract::gfxErrorCodeName(gfxErrorCode) << "\n";
  }
  if (!details.empty()) {
    std::cerr << "runtime_failure_detail=" << details << "\n";
    if (gfxErrorCode != GfxErrorCode::None) {
      std::cerr << "gfx_error_why=" << details << "\n";
    }
  }
  return exitCode;
}

} // namespace primestruct::native_metal_window

@class PrimeStructNativeMetalWindowHostDelegate;

@interface PrimeStructNativeMetalWindow : NSWindow
@property(nonatomic, assign) PrimeStructNativeMetalWindowHostDelegate *hostDelegate;
@end

@interface PrimeStructNativeMetalWindowHostDelegate : NSObject <NSApplicationDelegate, NSWindowDelegate>
- (instancetype)initWithConfig:(const primestruct::native_metal_window::PresenterConfig &)config
                     callbacks:(const primestruct::native_metal_window::PresenterCallbacks &)callbacks
                       context:(void *)context
                      exitCode:(int *)exitCodePtr;
- (void)handleEscapeKey;
- (void)renderFrame:(NSTimer *)timer;
@end

@implementation PrimeStructNativeMetalWindow

- (void)keyDown:(NSEvent *)event {
  NSString *characters = [event charactersIgnoringModifiers];
  const bool isEscapeChar = (characters != nil && [characters length] == 1 && [characters characterAtIndex:0] == 27);
  if (event.keyCode == 53 || isEscapeChar) {
    if (self.hostDelegate != nil) {
      [self.hostDelegate handleEscapeKey];
      return;
    }
  }
  [super keyDown:event];
}

@end

@implementation PrimeStructNativeMetalWindowHostDelegate {
  PrimeStructNativeMetalWindow *_window;
  CAMetalLayer *_metalLayer;
  id<MTLDevice> _device;
  id<MTLCommandQueue> _queue;
  NSTimer *_frameTimer;
  int _renderedFrameCount;
  bool _printedFirstFrame;
  bool _firstFrameSubmitted;
  primestruct::native_metal_window::PresenterConfig _config;
  primestruct::native_metal_window::PresenterCallbacks _callbacks;
  void *_context;
  int *_exitCodePtr;
}

- (instancetype)initWithConfig:(const primestruct::native_metal_window::PresenterConfig &)config
                     callbacks:(const primestruct::native_metal_window::PresenterCallbacks &)callbacks
                       context:(void *)context
                      exitCode:(int *)exitCodePtr {
  self = [super init];
  if (self != nil) {
    _renderedFrameCount = 0;
    _printedFirstFrame = false;
    _firstFrameSubmitted = false;
    _config = config;
    _callbacks = callbacks;
    _context = context;
    _exitCodePtr = exitCodePtr;
  }
  return self;
}

- (void)emitStartupFailure:(const primestruct::native_metal_window::FailureInfo &)failure {
  if (_exitCodePtr != nullptr) {
    *_exitCodePtr = primestruct::native_metal_window::emitStartupFailure(
        _config.gfxProfile,
        failure.startupStage,
        failure.reason,
        failure.details,
        failure.gfxErrorCode);
  }
  [NSApp terminate:nil];
}

- (void)emitRuntimeFailure:(const primestruct::native_metal_window::FailureInfo &)failure {
  if (_exitCodePtr != nullptr) {
    *_exitCodePtr = primestruct::native_metal_window::emitRuntimeFailure(
        _config.gfxProfile,
        failure.runtimeExitCode,
        failure.reason,
        failure.details,
        failure.gfxErrorCode);
  }
  [NSApp terminate:nil];
}

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
  (void)notification;

  _device = MTLCreateSystemDefaultDevice();
  if (_device == nil) {
    primestruct::native_metal_window::FailureInfo failure;
    failure.startupStage = primestruct::native_metal_window::StartupFailureStage::GpuDeviceAcquisition;
    failure.reason = "metal_device_creation_failed";
    failure.details = "failed to create Metal device";
    failure.gfxErrorCode = primestruct::native_metal_window::GfxErrorCode::DeviceCreateFailed;
    [self emitStartupFailure:failure];
    return;
  }

  _queue = [_device newCommandQueue];
  if (_queue == nil) {
    primestruct::native_metal_window::FailureInfo failure;
    failure.startupStage = primestruct::native_metal_window::StartupFailureStage::GpuDeviceAcquisition;
    failure.reason = "command_queue_creation_failed";
    failure.details = "failed to create command queue";
    failure.gfxErrorCode = primestruct::native_metal_window::GfxErrorCode::DeviceCreateFailed;
    [self emitStartupFailure:failure];
    return;
  }

  if (_callbacks.prepare != nullptr) {
    primestruct::native_metal_window::FailureInfo failure;
    if (!_callbacks.prepare(_context, _device, _queue, failure)) {
      [self emitStartupFailure:failure];
      return;
    }
  }

  NSRect frame = NSMakeRect(120.0, 120.0, _config.windowWidth, _config.windowHeight);
  const NSWindowStyleMask styleMask = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                                      NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable;
  _window = [[PrimeStructNativeMetalWindow alloc] initWithContentRect:frame
                                                            styleMask:styleMask
                                                              backing:NSBackingStoreBuffered
                                                                defer:NO];
  if (_window == nil) {
    primestruct::native_metal_window::FailureInfo failure;
    failure.startupStage = primestruct::native_metal_window::StartupFailureStage::WindowCreation;
    failure.reason = "window_creation_failed";
    failure.details = "failed to create native window";
    failure.gfxErrorCode = primestruct::native_metal_window::GfxErrorCode::WindowCreateFailed;
    [self emitStartupFailure:failure];
    return;
  }
  _window.hostDelegate = self;
  if (_config.windowTitle != nil) {
    _window.title = _config.windowTitle;
  }
  _window.delegate = self;

  NSView *contentView = _window.contentView;
  if (contentView == nil) {
    primestruct::native_metal_window::FailureInfo failure;
    failure.startupStage = primestruct::native_metal_window::StartupFailureStage::WindowCreation;
    failure.reason = "window_content_view_missing";
    failure.details = "native window content view was unavailable";
    failure.gfxErrorCode = primestruct::native_metal_window::GfxErrorCode::WindowCreateFailed;
    [self emitStartupFailure:failure];
    return;
  }

  _metalLayer = [CAMetalLayer layer];
  if (_metalLayer == nil) {
    primestruct::native_metal_window::FailureInfo failure;
    failure.startupStage = primestruct::native_metal_window::StartupFailureStage::WindowCreation;
    failure.reason = "swapchain_layer_creation_failed";
    failure.details = "failed to create CAMetalLayer swapchain";
    failure.gfxErrorCode = primestruct::native_metal_window::GfxErrorCode::SwapchainCreateFailed;
    [self emitStartupFailure:failure];
    return;
  }
  _metalLayer.device = _device;
  _metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
  _metalLayer.frame = contentView.bounds;
  _metalLayer.contentsScale = _window.backingScaleFactor;
  _metalLayer.autoresizingMask = kCALayerWidthSizable | kCALayerHeightSizable;
  contentView.wantsLayer = YES;
  contentView.layer = _metalLayer;

  [_window makeKeyAndOrderFront:nil];
  if (_config.activateApp) {
    [NSApp activateIgnoringOtherApps:YES];
  }

  std::cout << "window_created=1\n";
  std::cout << "swapchain_layer_created=1\n";
  if (_callbacks.presenterReady != nullptr) {
    _callbacks.presenterReady(_context);
  }

  _frameTimer = [NSTimer scheduledTimerWithTimeInterval:(1.0 / 60.0)
                                                  target:self
                                                selector:@selector(renderFrame:)
                                                userInfo:nil
                                                 repeats:YES];
}

- (void)renderFrame:(NSTimer *)timer {
  (void)timer;
  @autoreleasepool {
    if (_metalLayer == nil || _queue == nil || _callbacks.render == nullptr) {
      return;
    }

    NSView *contentView = _window.contentView;
    if (contentView == nil) {
      return;
    }
    const CGSize drawableSize =
        CGSizeMake(contentView.bounds.size.width * _window.backingScaleFactor,
                   contentView.bounds.size.height * _window.backingScaleFactor);
    if (drawableSize.width <= 1.0 || drawableSize.height <= 1.0) {
      return;
    }
    _metalLayer.drawableSize = drawableSize;
    id<CAMetalDrawable> drawable = [_metalLayer nextDrawable];
    if (drawable == nil) {
      if (!_firstFrameSubmitted) {
        primestruct::native_metal_window::FailureInfo failure;
        failure.startupStage = primestruct::native_metal_window::StartupFailureStage::FirstFrameSubmission;
        failure.reason = "frame_drawable_acquisition_failed";
        failure.details = "failed to acquire drawable from CAMetalLayer";
        failure.gfxErrorCode = primestruct::native_metal_window::GfxErrorCode::FrameAcquireFailed;
        [self emitStartupFailure:failure];
      } else {
        primestruct::native_metal_window::FailureInfo failure;
        failure.runtimeExitCode = 82;
        failure.reason = "frame_drawable_acquisition_failed";
        failure.details = "failed to acquire drawable from CAMetalLayer";
        failure.gfxErrorCode = primestruct::native_metal_window::GfxErrorCode::FrameAcquireFailed;
        [self emitRuntimeFailure:failure];
      }
      return;
    }

    const primestruct::native_metal_window::RenderFrameResult result =
        _callbacks.render(_context, _queue, drawable, drawableSize, _firstFrameSubmitted);
    if (!result.ok) {
      if (!_firstFrameSubmitted) {
        [self emitStartupFailure:result.failure];
      } else {
        [self emitRuntimeFailure:result.failure];
      }
      return;
    }

    if (result.renderedFrame) {
      if (!_firstFrameSubmitted) {
        std::cout << "gfx_profile=" << _config.gfxProfile << "\n";
        std::cout << "startup_success=1\n";
        _firstFrameSubmitted = true;
      }
      if (!_printedFirstFrame) {
        std::cout << "frame_rendered=1\n";
        _printedFirstFrame = true;
      }
      _renderedFrameCount += 1;
    }

    if (result.requestExit) {
      if (result.exitReason != nullptr && result.exitReason[0] != '\0') {
        std::cout << "exit_reason=" << result.exitReason << "\n";
      }
      if (_exitCodePtr != nullptr) {
        *_exitCodePtr = result.exitCode;
      }
      [NSApp terminate:nil];
      return;
    }

    if (_config.maxFrames > 0 && _renderedFrameCount >= _config.maxFrames) {
      std::cout << "exit_reason=max_frames\n";
      if (_exitCodePtr != nullptr) {
        *_exitCodePtr = 0;
      }
      [NSApp terminate:nil];
    }
  }
}

- (void)handleEscapeKey {
  if (_frameTimer != nil) {
    [_frameTimer invalidate];
    _frameTimer = nil;
  }
  std::cout << "exit_reason=esc_key\n";
  if (_exitCodePtr != nullptr) {
    *_exitCodePtr = 0;
  }
  [NSApp terminate:nil];
}

- (void)windowWillClose:(NSNotification *)notification {
  (void)notification;
  if (_frameTimer != nil) {
    [_frameTimer invalidate];
    _frameTimer = nil;
  }
  std::cout << "exit_reason=window_close\n";
  if (_exitCodePtr != nullptr) {
    *_exitCodePtr = 0;
  }
  [NSApp terminate:nil];
}

- (void)applicationWillTerminate:(NSNotification *)notification {
  (void)notification;
  if (_callbacks.shutdown != nullptr) {
    _callbacks.shutdown(_context);
  }
}

@end

namespace primestruct::native_metal_window {

inline int runPresenter(const PresenterConfig &config, void *context, const PresenterCallbacks &callbacks) {
  @autoreleasepool {
    int exitCode = 0;
    NSApplication *app = [NSApplication sharedApplication];
    app.activationPolicy = config.activationPolicy;
    PrimeStructNativeMetalWindowHostDelegate *delegate =
        [[PrimeStructNativeMetalWindowHostDelegate alloc] initWithConfig:config
                                                               callbacks:callbacks
                                                                 context:context
                                                                exitCode:&exitCode];
    app.delegate = delegate;
    [app run];
    return exitCode;
  }
}

} // namespace primestruct::native_metal_window
