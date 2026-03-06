#import <AppKit/AppKit.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

#include <simd/simd.h>

#if defined(__unix__) || defined(__APPLE__)
#include <sys/wait.h>
#endif

namespace {

constexpr const char *WindowShaderSource = R"(
#include <metal_stdlib>
using namespace metal;

struct WindowVertexIn {
  float3 position [[attribute(0)]];
  float4 color [[attribute(1)]];
};

struct WindowUniforms {
  float4x4 modelViewProjection;
};

struct VertexOut {
  float4 position [[position]];
  float4 color;
};

vertex VertexOut windowVertexMain(WindowVertexIn in [[stage_in]],
                                  constant WindowUniforms &uniforms [[buffer(1)]]) {
  VertexOut out;
  out.position = uniforms.modelViewProjection * float4(in.position, 1.0f);
  out.color = in.color;
  return out;
}

fragment float4 windowFragmentMain(VertexOut in [[stage_in]]) {
  return in.color;
}
)";

constexpr int SimulationStreamFieldsPerFrame = 4;
constexpr int SimulationFixedStepMillis = 16;

int gExitCode = 0;

struct SimulationFrameState {
  int tick = 0;
  int angleMilli = 0;
  int axisXCenti = 0;
  int axisYCenti = 0;
};

struct WindowVertex {
  simd_float3 position;
  simd_float4 color;
};

struct WindowUniforms {
  simd_float4x4 modelViewProjection;
};

constexpr std::array<WindowVertex, 8> CubeVertices = {{
    {{-0.6f, -0.6f, -0.6f}, {1.0f, 0.2f, 0.2f, 1.0f}},
    {{0.6f, -0.6f, -0.6f}, {0.2f, 1.0f, 0.2f, 1.0f}},
    {{0.6f, 0.6f, -0.6f}, {0.2f, 0.2f, 1.0f, 1.0f}},
    {{-0.6f, 0.6f, -0.6f}, {1.0f, 1.0f, 0.2f, 1.0f}},
    {{-0.6f, -0.6f, 0.6f}, {1.0f, 0.2f, 1.0f, 1.0f}},
    {{0.6f, -0.6f, 0.6f}, {0.2f, 1.0f, 1.0f, 1.0f}},
    {{0.6f, 0.6f, 0.6f}, {1.0f, 0.6f, 0.2f, 1.0f}},
    {{-0.6f, 0.6f, 0.6f}, {0.7f, 0.7f, 0.7f, 1.0f}},
}};

constexpr std::array<uint16_t, 36> CubeIndices = {{
    4, 5, 6, 4, 6, 7,
    0, 2, 1, 0, 3, 2,
    0, 4, 7, 0, 7, 3,
    1, 2, 6, 1, 6, 5,
    3, 7, 6, 3, 6, 2,
    0, 1, 5, 0, 5, 4,
}};

simd_float4x4 makeIdentityMatrix() {
  return (simd_float4x4){
      {1.0f, 0.0f, 0.0f, 0.0f},
      {0.0f, 1.0f, 0.0f, 0.0f},
      {0.0f, 0.0f, 1.0f, 0.0f},
      {0.0f, 0.0f, 0.0f, 1.0f},
  };
}

simd_float4x4 makeTranslationMatrix(float x, float y, float z) {
  simd_float4x4 matrix = makeIdentityMatrix();
  matrix.columns[3] = {x, y, z, 1.0f};
  return matrix;
}

simd_float4x4 makeRotationMatrix(float angleRadians, simd_float3 axis) {
  const simd_float3 normalizedAxis = simd_normalize(axis);
  const float x = normalizedAxis.x;
  const float y = normalizedAxis.y;
  const float z = normalizedAxis.z;
  const float c = std::cos(angleRadians);
  const float s = std::sin(angleRadians);
  const float oneMinusC = 1.0f - c;

  return (simd_float4x4){
      {c + (x * x * oneMinusC), (x * y * oneMinusC) + (z * s), (x * z * oneMinusC) - (y * s), 0.0f},
      {(y * x * oneMinusC) - (z * s), c + (y * y * oneMinusC), (y * z * oneMinusC) + (x * s), 0.0f},
      {(z * x * oneMinusC) + (y * s), (z * y * oneMinusC) - (x * s), c + (z * z * oneMinusC), 0.0f},
      {0.0f, 0.0f, 0.0f, 1.0f},
  };
}

simd_float4x4 makePerspectiveMatrix(float fovYRadians, float aspect, float nearPlane, float farPlane) {
  const float yScale = 1.0f / std::tan(fovYRadians * 0.5f);
  const float xScale = yScale / aspect;
  const float zRange = farPlane - nearPlane;

  return (simd_float4x4){
      {xScale, 0.0f, 0.0f, 0.0f},
      {0.0f, yScale, 0.0f, 0.0f},
      {0.0f, 0.0f, -(farPlane + nearPlane) / zRange, -1.0f},
      {0.0f, 0.0f, -(2.0f * farPlane * nearPlane) / zRange, 0.0f},
  };
}

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

bool parseSignedInt(const char *text, int &valueOut) {
  if (text == nullptr || *text == '\0') {
    return false;
  }
  char *end = nullptr;
  const long parsed = std::strtol(text, &end, 10);
  if (end == text || *end != '\0') {
    return false;
  }
  if (parsed < std::numeric_limits<int>::min() || parsed > std::numeric_limits<int>::max()) {
    return false;
  }
  valueOut = static_cast<int>(parsed);
  return true;
}

std::string shellQuote(const std::string &value) {
  std::string quoted = "'";
  for (char c : value) {
    if (c == '\'') {
      quoted += "'\\''";
    } else {
      quoted += c;
    }
  }
  quoted += "'";
  return quoted;
}

int decodeExitCode(int rawCode) {
#if defined(__unix__) || defined(__APPLE__)
  if (rawCode == -1) {
    return -1;
  }
  if (WIFEXITED(rawCode)) {
    return WEXITSTATUS(rawCode);
  }
  return -1;
#else
  return rawCode;
#endif
}

bool loadSimulationFrames(const std::string &cubeSimulationPath,
                          std::vector<SimulationFrameState> &framesOut,
                          std::string &errorOut) {
  framesOut.clear();
  const std::string command = shellQuote(cubeSimulationPath);
  FILE *pipe = popen(command.c_str(), "r");
  if (pipe == nullptr) {
    errorOut = "failed to execute cube simulation binary";
    return false;
  }

  std::vector<int> values;
  std::array<char, 256> buffer{};
  while (std::fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
    std::string line(buffer.data());
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
      line.pop_back();
    }
    if (line.empty()) {
      continue;
    }
    int parsedValue = 0;
    if (!parseSignedInt(line.c_str(), parsedValue)) {
      errorOut = "invalid cube simulation stream value: " + line;
      pclose(pipe);
      return false;
    }
    values.push_back(parsedValue);
  }

  const int rawExitCode = pclose(pipe);
  const int exitCode = decodeExitCode(rawExitCode);
  if (exitCode != 0) {
    errorOut = "cube simulation binary exited with code " + std::to_string(exitCode);
    return false;
  }
  if (values.empty()) {
    errorOut = "cube simulation stream was empty";
    return false;
  }
  if (values.size() % SimulationStreamFieldsPerFrame != 0) {
    errorOut = "cube simulation stream value count was not divisible by 4";
    return false;
  }

  for (size_t i = 0; i < values.size(); i += SimulationStreamFieldsPerFrame) {
    SimulationFrameState frame;
    frame.tick = values[i];
    frame.angleMilli = values[i + 1];
    frame.axisXCenti = values[i + 2];
    frame.axisYCenti = values[i + 3];
    framesOut.push_back(frame);
  }

  return true;
}

@class PrimeStructWindowHostDelegate;

@interface PrimeStructWindow : NSWindow
@property(nonatomic, assign) PrimeStructWindowHostDelegate *hostDelegate;
@end

@interface PrimeStructWindowHostDelegate : NSObject <NSApplicationDelegate, NSWindowDelegate>
- (instancetype)initWithMaxFrames:(int)maxFrames
                cubeSimulationPath:(const std::string &)cubeSimulationPath
               simulationSmokeMode:(bool)simulationSmokeMode;
- (void)handleEscapeKey;
- (void)renderFrame:(NSTimer *)timer;
@end

@implementation PrimeStructWindow

- (void)keyDown:(NSEvent *)event {
  NSString *characters = [event charactersIgnoringModifiers];
  if (event.keyCode == 53 || [characters isEqualToString:@"\e"]) {
    if (self.hostDelegate != nil) {
      [self.hostDelegate handleEscapeKey];
      return;
    }
  }
  [super keyDown:event];
}

@end

@implementation PrimeStructWindowHostDelegate {
  PrimeStructWindow *_window;
  CAMetalLayer *_metalLayer;
  id<MTLDevice> _device;
  id<MTLCommandQueue> _queue;
  id<MTLRenderPipelineState> _pipeline;
  id<MTLDepthStencilState> _depthState;
  id<MTLBuffer> _vertexBuffer;
  id<MTLBuffer> _indexBuffer;
  id<MTLBuffer> _uniformBuffer;
  id<MTLTexture> _depthTexture;
  NSTimer *_frameTimer;
  int _maxFrames;
  int _frameIndex;
  int _renderedFrameCount;
  bool _printedFirstFrame;
  bool _printedSimulationFrame;
  bool _simulationSmokeMode;
  bool _startupComplete;
  std::string _cubeSimulationPath;
  std::vector<SimulationFrameState> _simulationFrames;
  NSUInteger _indexCount;
  CGSize _depthTextureSize;
}

- (instancetype)initWithMaxFrames:(int)maxFrames
                cubeSimulationPath:(const std::string &)cubeSimulationPath
               simulationSmokeMode:(bool)simulationSmokeMode {
  self = [super init];
  if (self != nil) {
    _maxFrames = maxFrames;
    _frameIndex = 0;
    _renderedFrameCount = 0;
    _printedFirstFrame = false;
    _printedSimulationFrame = false;
    _simulationSmokeMode = simulationSmokeMode;
    _startupComplete = false;
    _cubeSimulationPath = cubeSimulationPath;
    _indexCount = CubeIndices.size();
    _depthTextureSize = CGSizeMake(0.0, 0.0);
  }
  return self;
}

- (void)failAndTerminate:(int)code message:(const char *)message details:(const char *)details {
  if (_startupComplete) {
    std::cerr << "runtime_failure=1\n";
  } else {
    std::cerr << "startup_failure=1\n";
  }
  if (details == nullptr || details[0] == '\0') {
    std::cerr << message << "\n";
  } else {
    std::cerr << message << ": " << details << "\n";
  }
  gExitCode = code;
  [NSApp terminate:nil];
}

- (void)ensureDepthTextureForSize:(CGSize)drawableSize {
  if (_depthTexture != nil && _depthTextureSize.width == drawableSize.width && _depthTextureSize.height == drawableSize.height) {
    return;
  }

  MTLTextureDescriptor *depthDescriptor =
      [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float
                                                         width:static_cast<NSUInteger>(drawableSize.width)
                                                        height:static_cast<NSUInteger>(drawableSize.height)
                                                     mipmapped:NO];
  depthDescriptor.storageMode = MTLStorageModePrivate;
  depthDescriptor.usage = MTLTextureUsageRenderTarget;
  _depthTexture = [_device newTextureWithDescriptor:depthDescriptor];
  _depthTextureSize = drawableSize;
}

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
  (void)notification;

  std::string simulationError;
  if (!loadSimulationFrames(_cubeSimulationPath, _simulationFrames, simulationError)) {
    [self failAndTerminate:69 message:"failed to load cube simulation stream" details:simulationError.c_str()];
    return;
  }

  std::cout << "simulation_stream_loaded=1\n";
  std::cout << "simulation_frames_loaded=" << _simulationFrames.size() << "\n";
  std::cout << "simulation_fixed_step_millis=" << SimulationFixedStepMillis << "\n";

  if (_simulationSmokeMode) {
    const SimulationFrameState &frame = _simulationFrames.front();
    std::cout << "simulation_smoke_tick=" << frame.tick << "\n";
    std::cout << "simulation_smoke_angle_milli=" << frame.angleMilli << "\n";
    std::cout << "simulation_smoke_axis_x_centi=" << frame.axisXCenti << "\n";
    std::cout << "simulation_smoke_axis_y_centi=" << frame.axisYCenti << "\n";
    gExitCode = 0;
    [NSApp terminate:nil];
    return;
  }

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
  std::cout << "shader_library_ready=1\n";

  id<MTLFunction> vertexFn = [library newFunctionWithName:@"windowVertexMain"];
  id<MTLFunction> fragmentFn = [library newFunctionWithName:@"windowFragmentMain"];
  if (vertexFn == nil || fragmentFn == nil) {
    [self failAndTerminate:73 message:"missing window host shader entrypoints" details:""];
    return;
  }

  MTLVertexDescriptor *vertexDescriptor = [[MTLVertexDescriptor alloc] init];
  vertexDescriptor.attributes[0].format = MTLVertexFormatFloat3;
  vertexDescriptor.attributes[0].offset = offsetof(WindowVertex, position);
  vertexDescriptor.attributes[0].bufferIndex = 0;
  vertexDescriptor.attributes[1].format = MTLVertexFormatFloat4;
  vertexDescriptor.attributes[1].offset = offsetof(WindowVertex, color);
  vertexDescriptor.attributes[1].bufferIndex = 0;
  vertexDescriptor.layouts[0].stride = sizeof(WindowVertex);
  vertexDescriptor.layouts[0].stepRate = 1;
  vertexDescriptor.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;

  MTLRenderPipelineDescriptor *pipelineDesc = [[MTLRenderPipelineDescriptor alloc] init];
  pipelineDesc.vertexFunction = vertexFn;
  pipelineDesc.fragmentFunction = fragmentFn;
  pipelineDesc.vertexDescriptor = vertexDescriptor;
  pipelineDesc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
  pipelineDesc.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;

  NSError *pipelineError = nil;
  _pipeline = [_device newRenderPipelineStateWithDescriptor:pipelineDesc error:&pipelineError];
  if (_pipeline == nil) {
    const char *details = pipelineError == nil ? "unknown" : pipelineError.localizedDescription.UTF8String;
    [self failAndTerminate:74 message:"failed to create render pipeline" details:details];
    return;
  }

  MTLDepthStencilDescriptor *depthDescriptor = [[MTLDepthStencilDescriptor alloc] init];
  depthDescriptor.depthCompareFunction = MTLCompareFunctionLess;
  depthDescriptor.depthWriteEnabled = YES;
  _depthState = [_device newDepthStencilStateWithDescriptor:depthDescriptor];
  if (_depthState == nil) {
    [self failAndTerminate:75 message:"failed to create depth state" details:""];
    return;
  }

  _vertexBuffer = [_device newBufferWithBytes:CubeVertices.data()
                                        length:(CubeVertices.size() * sizeof(WindowVertex))
                                       options:MTLResourceStorageModeShared];
  if (_vertexBuffer == nil) {
    [self failAndTerminate:76 message:"failed to create vertex buffer" details:""];
    return;
  }

  _indexBuffer = [_device newBufferWithBytes:CubeIndices.data()
                                       length:(CubeIndices.size() * sizeof(uint16_t))
                                      options:MTLResourceStorageModeShared];
  if (_indexBuffer == nil) {
    [self failAndTerminate:77 message:"failed to create index buffer" details:""];
    return;
  }

  _uniformBuffer = [_device newBufferWithLength:sizeof(WindowUniforms) options:MTLResourceStorageModeShared];
  if (_uniformBuffer == nil) {
    [self failAndTerminate:78 message:"failed to create uniform buffer" details:""];
    return;
  }

  std::cout << "vertex_buffer_ready=1\n";
  std::cout << "index_buffer_ready=1\n";
  std::cout << "uniform_buffer_ready=1\n";

  NSRect frame = NSMakeRect(120.0, 120.0, 960.0, 640.0);
  const NSWindowStyleMask styleMask = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                                      NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable;
  _window = [[PrimeStructWindow alloc] initWithContentRect:frame
                                                  styleMask:styleMask
                                                    backing:NSBackingStoreBuffered
                                                      defer:NO];
  if (_window == nil) {
    [self failAndTerminate:79 message:"failed to create native window" details:""];
    return;
  }
  _window.hostDelegate = self;
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
  std::cout << "startup_success=1\n";
  _startupComplete = true;

  _frameTimer = [NSTimer scheduledTimerWithTimeInterval:(1.0 / 60.0)
                                                  target:self
                                                selector:@selector(renderFrame:)
                                                userInfo:nil
                                                 repeats:YES];
}

- (void)renderFrame:(NSTimer *)timer {
  (void)timer;
  @autoreleasepool {
    if (_metalLayer == nil || _queue == nil || _pipeline == nil || _simulationFrames.empty()) {
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
    [self ensureDepthTextureForSize:drawableSize];
    if (_depthTexture == nil) {
      [self failAndTerminate:80 message:"failed to create depth texture" details:""];
      return;
    }

    id<CAMetalDrawable> drawable = [_metalLayer nextDrawable];
    if (drawable == nil) {
      return;
    }

    const size_t frameSlot =
        std::min(static_cast<size_t>(_frameIndex), _simulationFrames.size() - 1);
    const SimulationFrameState &simulationFrame = _simulationFrames[frameSlot];
    const float angle = static_cast<float>(simulationFrame.angleMilli) / 1000.0f;

    const simd_float3 axisRaw = {
        static_cast<float>(simulationFrame.axisXCenti) / 100.0f,
        static_cast<float>(simulationFrame.axisYCenti) / 100.0f,
        0.35f,
    };
    const simd_float3 axis = simd_length(axisRaw) < 0.001f ? simd_float3{0.0f, 1.0f, 0.0f} : axisRaw;

    const float aspect = static_cast<float>(drawableSize.width / drawableSize.height);
    const simd_float4x4 model = makeRotationMatrix(angle, axis);
    const simd_float4x4 view = makeTranslationMatrix(0.0f, 0.0f, -3.0f);
    const simd_float4x4 projection = makePerspectiveMatrix(1.0471976f, aspect, 0.1f, 20.0f);

    auto *uniforms = reinterpret_cast<WindowUniforms *>(_uniformBuffer.contents);
    uniforms->modelViewProjection = simd_mul(projection, simd_mul(view, model));

    const double axisXNorm =
        std::clamp((static_cast<double>(simulationFrame.axisXCenti) + 100.0) / 200.0, 0.0, 1.0);
    const double axisYNorm =
        std::clamp((static_cast<double>(simulationFrame.axisYCenti) + 100.0) / 200.0, 0.0, 1.0);

    MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].texture = drawable.texture;
    pass.colorAttachments[0].loadAction = MTLLoadActionClear;
    pass.colorAttachments[0].storeAction = MTLStoreActionStore;
    pass.colorAttachments[0].clearColor =
        MTLClearColorMake(0.03 + (0.08 * axisXNorm), 0.04 + (0.08 * axisYNorm), 0.06, 1.0);
    pass.depthAttachment.texture = _depthTexture;
    pass.depthAttachment.loadAction = MTLLoadActionClear;
    pass.depthAttachment.storeAction = MTLStoreActionDontCare;
    pass.depthAttachment.clearDepth = 1.0;

    id<MTLCommandBuffer> commandBuffer = [_queue commandBuffer];
    id<MTLRenderCommandEncoder> encoder = [commandBuffer renderCommandEncoderWithDescriptor:pass];
    if (encoder == nil) {
      [self failAndTerminate:81 message:"failed to create render command encoder" details:""];
      return;
    }

    [encoder setRenderPipelineState:_pipeline];
    [encoder setDepthStencilState:_depthState];
    [encoder setVertexBuffer:_vertexBuffer offset:0 atIndex:0];
    [encoder setVertexBuffer:_uniformBuffer offset:0 atIndex:1];
    [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                        indexCount:_indexCount
                         indexType:MTLIndexTypeUInt16
                       indexBuffer:_indexBuffer
                 indexBufferOffset:0];
    [encoder endEncoding];

    [commandBuffer presentDrawable:drawable];
    [commandBuffer commit];

    if (!_printedSimulationFrame) {
      std::cout << "simulation_tick=" << simulationFrame.tick << "\n";
      std::cout << "simulation_angle_milli=" << simulationFrame.angleMilli << "\n";
      std::cout << "simulation_axis_x_centi=" << simulationFrame.axisXCenti << "\n";
      std::cout << "simulation_axis_y_centi=" << simulationFrame.axisYCenti << "\n";
      _printedSimulationFrame = true;
    }

    if (!_printedFirstFrame) {
      std::cout << "frame_rendered=1\n";
      _printedFirstFrame = true;
    }

    if (_frameIndex + 1 < static_cast<int>(_simulationFrames.size())) {
      _frameIndex += 1;
    }
    _renderedFrameCount += 1;

    if (_maxFrames > 0 && _renderedFrameCount >= _maxFrames) {
      std::cout << "exit_reason=max_frames\n";
      gExitCode = 0;
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
  gExitCode = 0;
  [NSApp terminate:nil];
}

- (void)windowWillClose:(NSNotification *)notification {
  (void)notification;
  if (_frameTimer != nil) {
    [_frameTimer invalidate];
    _frameTimer = nil;
  }
  std::cout << "exit_reason=window_close\n";
  gExitCode = 0;
  [NSApp terminate:nil];
}

@end

} // namespace

int main(int argc, char **argv) {
  int maxFrames = 0;
  std::string cubeSimulationPath;
  bool simulationSmokeMode = false;

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--help") {
      std::cout
          << "usage: window_host --cube-sim <path> [--max-frames <positive-int>] [--simulation-smoke]\n";
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
    if (arg == "--cube-sim") {
      if (i + 1 >= argc) {
        std::cerr << "missing value for --cube-sim\n";
        return 64;
      }
      cubeSimulationPath = argv[i + 1];
      i += 1;
      continue;
    }
    if (arg == "--simulation-smoke") {
      simulationSmokeMode = true;
      continue;
    }
    std::cerr << "unknown arg: " << arg << "\n";
    return 64;
  }

  if (cubeSimulationPath.empty()) {
    std::cerr << "missing required --cube-sim <path>\n";
    return 64;
  }

  @autoreleasepool {
    NSApplication *app = [NSApplication sharedApplication];
    app.activationPolicy =
        simulationSmokeMode ? NSApplicationActivationPolicyProhibited : NSApplicationActivationPolicyRegular;
    PrimeStructWindowHostDelegate *delegate = [[PrimeStructWindowHostDelegate alloc] initWithMaxFrames:maxFrames
                                                                                     cubeSimulationPath:cubeSimulationPath
                                                                                    simulationSmokeMode:simulationSmokeMode];
    app.delegate = delegate;
    [app run];
  }
  return gExitCode;
}
