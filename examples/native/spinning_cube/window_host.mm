#import <AppKit/AppKit.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

namespace {

constexpr const char *WindowShaderSource = R"(
#include <metal_stdlib>
using namespace metal;

struct VertexOut {
  float4 position [[position]];
  float4 color;
};

vertex VertexOut windowVertexMain(uint vertexId [[vertex_id]],
                                  constant float &angle [[buffer(0)]]) {
  const float2 positions[3] = {
      float2(0.0f, 0.60f),
      float2(-0.55f, -0.45f),
      float2(0.55f, -0.45f),
  };
  const float4 colors[3] = {
      float4(0.95f, 0.24f, 0.32f, 1.0f),
      float4(0.23f, 0.86f, 0.41f, 1.0f),
      float4(0.18f, 0.56f, 1.00f, 1.0f),
  };

  float s = sin(angle);
  float c = cos(angle);
  float2 p = positions[vertexId];
  float2 rotated = float2(c * p.x - s * p.y, s * p.x + c * p.y);

  VertexOut out;
  out.position = float4(rotated, 0.0f, 1.0f);
  out.color = colors[vertexId];
  return out;
}

fragment float4 windowFragmentMain(VertexOut in [[stage_in]]) {
  return in.color;
}
)";

int gExitCode = 0;

bool parsePositiveInt(const char *text, int &valueOut) {
  if (text == nullptr || *text == '\0') {
    return false;
  }
  char *end = nullptr;
  const long parsed = std::strtol(text, &end, 10);
  if (end == text || *end != '\0') {
    return false;
  }
  if (parsed <= 0 || parsed > 1000000) {
    return false;
  }
  valueOut = static_cast<int>(parsed);
  return true;
}

@interface PrimeStructWindowHostDelegate : NSObject <NSApplicationDelegate, NSWindowDelegate>
- (instancetype)initWithMaxFrames:(int)maxFrames;
- (void)renderFrame:(NSTimer *)timer;
@end

@implementation PrimeStructWindowHostDelegate {
  NSWindow *_window;
  CAMetalLayer *_metalLayer;
  id<MTLDevice> _device;
  id<MTLCommandQueue> _queue;
  id<MTLRenderPipelineState> _pipeline;
  NSTimer *_frameTimer;
  int _maxFrames;
  int _frameIndex;
  bool _printedFirstFrame;
}

- (instancetype)initWithMaxFrames:(int)maxFrames {
  self = [super init];
  if (self != nil) {
    _maxFrames = maxFrames;
    _frameIndex = 0;
    _printedFirstFrame = false;
  }
  return self;
}

- (void)failAndTerminate:(int)code message:(const char *)message details:(const char *)details {
  if (details == nullptr || details[0] == '\0') {
    std::cerr << message << "\n";
  } else {
    std::cerr << message << ": " << details << "\n";
  }
  gExitCode = code;
  [NSApp terminate:nil];
}

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
  (void)notification;

  _device = MTLCreateSystemDefaultDevice();
  if (_device == nil) {
    [self failAndTerminate:70 message:"failed to create Metal device" details:""];
    return;
  }

  _queue = [_device newCommandQueue];
  if (_queue == nil) {
    [self failAndTerminate:71 message:"failed to create command queue" details:""];
    return;
  }

  NSError *libraryError = nil;
  NSString *shaderSource = [NSString stringWithUTF8String:WindowShaderSource];
  id<MTLLibrary> library = [_device newLibraryWithSource:shaderSource options:nil error:&libraryError];
  if (library == nil) {
    const char *details = libraryError == nil ? "unknown" : libraryError.localizedDescription.UTF8String;
    [self failAndTerminate:72 message:"failed to compile window host shaders" details:details];
    return;
  }

  id<MTLFunction> vertexFn = [library newFunctionWithName:@"windowVertexMain"];
  id<MTLFunction> fragmentFn = [library newFunctionWithName:@"windowFragmentMain"];
  if (vertexFn == nil || fragmentFn == nil) {
    [self failAndTerminate:73 message:"missing window host shader entrypoints" details:""];
    return;
  }

  MTLRenderPipelineDescriptor *pipelineDesc = [[MTLRenderPipelineDescriptor alloc] init];
  pipelineDesc.vertexFunction = vertexFn;
  pipelineDesc.fragmentFunction = fragmentFn;
  pipelineDesc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;

  NSError *pipelineError = nil;
  _pipeline = [_device newRenderPipelineStateWithDescriptor:pipelineDesc error:&pipelineError];
  if (_pipeline == nil) {
    const char *details = pipelineError == nil ? "unknown" : pipelineError.localizedDescription.UTF8String;
    [self failAndTerminate:74 message:"failed to create render pipeline" details:details];
    return;
  }

  NSRect frame = NSMakeRect(120.0, 120.0, 960.0, 640.0);
  const NSWindowStyleMask styleMask = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                                      NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable;
  _window = [[NSWindow alloc] initWithContentRect:frame
                                        styleMask:styleMask
                                          backing:NSBackingStoreBuffered
                                            defer:NO];
  if (_window == nil) {
    [self failAndTerminate:75 message:"failed to create native window" details:""];
    return;
  }
  _window.title = @"PrimeStruct Spinning Cube (Window Host)";
  _window.delegate = self;

  NSView *contentView = _window.contentView;
  _metalLayer = [CAMetalLayer layer];
  _metalLayer.device = _device;
  _metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
  _metalLayer.frame = contentView.bounds;
  _metalLayer.contentsScale = _window.backingScaleFactor;
  _metalLayer.autoresizingMask = kCALayerWidthSizable | kCALayerHeightSizable;
  contentView.wantsLayer = YES;
  contentView.layer = _metalLayer;

  [_window makeKeyAndOrderFront:nil];
  [NSApp activateIgnoringOtherApps:YES];

  std::cout << "window_created=1\n";
  std::cout << "swapchain_layer_created=1\n";
  std::cout << "pipeline_ready=1\n";

  _frameTimer = [NSTimer scheduledTimerWithTimeInterval:(1.0 / 60.0)
                                                  target:self
                                                selector:@selector(renderFrame:)
                                                userInfo:nil
                                                 repeats:YES];
}

- (void)renderFrame:(NSTimer *)timer {
  (void)timer;
  @autoreleasepool {
    if (_metalLayer == nil || _queue == nil || _pipeline == nil) {
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
      return;
    }

    MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].texture = drawable.texture;
    pass.colorAttachments[0].loadAction = MTLLoadActionClear;
    pass.colorAttachments[0].storeAction = MTLStoreActionStore;
    pass.colorAttachments[0].clearColor = MTLClearColorMake(0.03, 0.04, 0.06, 1.0);

    id<MTLCommandBuffer> commandBuffer = [_queue commandBuffer];
    id<MTLRenderCommandEncoder> encoder = [commandBuffer renderCommandEncoderWithDescriptor:pass];
    if (encoder == nil) {
      [self failAndTerminate:76 message:"failed to create render command encoder" details:""];
      return;
    }

    const float angle = static_cast<float>(_frameIndex) * 0.025f;
    [encoder setRenderPipelineState:_pipeline];
    [encoder setVertexBytes:&angle length:sizeof(float) atIndex:0];
    [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
    [encoder endEncoding];

    [commandBuffer presentDrawable:drawable];
    [commandBuffer commit];

    _frameIndex += 1;
    if (!_printedFirstFrame) {
      std::cout << "frame_rendered=1\n";
      _printedFirstFrame = true;
    }

    if (_maxFrames > 0 && _frameIndex >= _maxFrames) {
      gExitCode = 0;
      [NSApp terminate:nil];
    }
  }
}

- (void)windowWillClose:(NSNotification *)notification {
  (void)notification;
  if (_frameTimer != nil) {
    [_frameTimer invalidate];
    _frameTimer = nil;
  }
  gExitCode = 0;
  [NSApp terminate:nil];
}

@end

} // namespace

int main(int argc, char **argv) {
  int maxFrames = 0;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--help") {
      std::cout << "usage: window_host [--max-frames <positive-int>]\n";
      return 0;
    }
    if (arg == "--max-frames") {
      if (i + 1 >= argc) {
        std::cerr << "missing value for --max-frames\n";
        return 64;
      }
      int parsed = 0;
      if (!parsePositiveInt(argv[i + 1], parsed)) {
        std::cerr << "invalid value for --max-frames: " << argv[i + 1] << "\n";
        return 64;
      }
      maxFrames = parsed;
      i += 1;
      continue;
    }
    std::cerr << "unknown arg: " << arg << "\n";
    return 64;
  }

  @autoreleasepool {
    NSApplication *app = [NSApplication sharedApplication];
    app.activationPolicy = NSApplicationActivationPolicyRegular;
    PrimeStructWindowHostDelegate *delegate =
        [[PrimeStructWindowHostDelegate alloc] initWithMaxFrames:maxFrames];
    app.delegate = delegate;
    [app run];
  }
  return gExitCode;
}
