#import <AppKit/AppKit.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include "../../shared/software_surface_bridge.h"

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
  float4 position [[attribute(0)]];
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
  out.position = uniforms.modelViewProjection * in.position;
  out.color = in.color;
  return out;
}

fragment float4 windowFragmentMain(VertexOut in [[stage_in]]) {
  return in.color;
}
)";

constexpr int SimulationStreamFieldsPerFrame = 4;
constexpr int SimulationFixedStepMillis = 16;
constexpr int GfxStreamMagic = 17001;
constexpr int GfxStreamVersion = 1;
constexpr size_t GfxHeaderFieldCount = 26;

enum class GfxErrorCode {
  None,
  WindowCreateFailed,
  DeviceCreateFailed,
  SwapchainCreateFailed,
  MeshCreateFailed,
  PipelineCreateFailed,
  MaterialCreateFailed,
  FrameAcquireFailed,
  QueueSubmitFailed,
  FramePresentFailed,
};

enum class StartupFailureStage {
  SimulationStreamLoad,
  GpuDeviceAcquisition,
  ShaderLoad,
  PipelineSetup,
  WindowCreation,
  FirstFrameSubmission,
};

const char *startupStageName(StartupFailureStage stage) {
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

int startupStageExitCode(StartupFailureStage stage) {
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

const char *deducedGfxProfileName() {
  return "native-desktop";
}

const char *gfxErrorCodeName(GfxErrorCode code) {
  switch (code) {
    case GfxErrorCode::None:
      return "";
    case GfxErrorCode::WindowCreateFailed:
      return "window_create_failed";
    case GfxErrorCode::DeviceCreateFailed:
      return "device_create_failed";
    case GfxErrorCode::SwapchainCreateFailed:
      return "swapchain_create_failed";
    case GfxErrorCode::MeshCreateFailed:
      return "mesh_create_failed";
    case GfxErrorCode::PipelineCreateFailed:
      return "pipeline_create_failed";
    case GfxErrorCode::MaterialCreateFailed:
      return "material_create_failed";
    case GfxErrorCode::FrameAcquireFailed:
      return "frame_acquire_failed";
    case GfxErrorCode::QueueSubmitFailed:
      return "queue_submit_failed";
    case GfxErrorCode::FramePresentFailed:
      return "frame_present_failed";
  }
  return "";
}

int gExitCode = 0;

struct SimulationFrameState {
  int tick = 0;
  int angleMilli = 0;
  int axisXCenti = 0;
  int axisYCenti = 0;
};

struct GfxStreamHeader {
  int windowWidth = 1280;
  int windowHeight = 720;
  int colorFormat = 0;
  int depthFormat = 0;
  int presentMode = 0;
  int clearColorRMilli = 0;
  int clearColorGMilli = 0;
  int clearColorBMilli = 0;
  int clearColorAMilli = 1000;
  int clearDepthMilli = 1000;
  int meshVertexCount = 0;
  int meshIndexCount = 0;
  int windowToken = 0;
  int deviceToken = 0;
  int queueToken = 0;
  int swapchainToken = 0;
  int meshToken = 0;
  int pipelineToken = 0;
  int materialToken = 0;
  int frameToken = 0;
  int renderPassToken = 0;
  int drawToken = 0;
  int endToken = 0;
  int submitPresentMask = 0;
};

std::string shellQuote(const std::string &value);
int decodeExitCode(int rawCode);

struct WindowVertex {
  float px;
  float py;
  float pz;
  float pw;
  float r;
  float g;
  float b;
  float a;
};

static_assert(offsetof(WindowVertex, px) == 0);
static_assert(offsetof(WindowVertex, pw) == 12);
static_assert(offsetof(WindowVertex, r) == 16);
static_assert(sizeof(WindowVertex) == 32);
static_assert(alignof(WindowVertex) == 4);

struct WindowUniforms {
  simd_float4x4 modelViewProjection;
};

constexpr std::array<WindowVertex, 8> CubeVertices = {{
    {-0.6f, -0.6f, -0.6f, 1.0f, 1.0f, 0.2f, 0.2f, 1.0f},
    {0.6f, -0.6f, -0.6f, 1.0f, 0.2f, 1.0f, 0.2f, 1.0f},
    {0.6f, 0.6f, -0.6f, 1.0f, 0.2f, 0.2f, 1.0f, 1.0f},
    {-0.6f, 0.6f, -0.6f, 1.0f, 1.0f, 1.0f, 0.2f, 1.0f},
    {-0.6f, -0.6f, 0.6f, 1.0f, 1.0f, 0.2f, 1.0f, 1.0f},
    {0.6f, -0.6f, 0.6f, 1.0f, 0.2f, 1.0f, 1.0f, 1.0f},
    {0.6f, 0.6f, 0.6f, 1.0f, 1.0f, 0.6f, 0.2f, 1.0f},
    {-0.6f, 0.6f, 0.6f, 1.0f, 0.7f, 0.7f, 0.7f, 1.0f},
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
  simd_float4x4 matrix{};
  matrix.columns[0] = {1.0f, 0.0f, 0.0f, 0.0f};
  matrix.columns[1] = {0.0f, 1.0f, 0.0f, 0.0f};
  matrix.columns[2] = {0.0f, 0.0f, 1.0f, 0.0f};
  matrix.columns[3] = {0.0f, 0.0f, 0.0f, 1.0f};
  return matrix;
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

  simd_float4x4 matrix{};
  matrix.columns[0] = {c + (x * x * oneMinusC), (x * y * oneMinusC) + (z * s), (x * z * oneMinusC) - (y * s), 0.0f};
  matrix.columns[1] = {(y * x * oneMinusC) - (z * s), c + (y * y * oneMinusC), (y * z * oneMinusC) + (x * s), 0.0f};
  matrix.columns[2] = {(z * x * oneMinusC) + (y * s), (z * y * oneMinusC) - (x * s), c + (z * z * oneMinusC), 0.0f};
  matrix.columns[3] = {0.0f, 0.0f, 0.0f, 1.0f};
  return matrix;
}

simd_float4x4 makePerspectiveMatrix(float fovYRadians, float aspect, float nearPlane, float farPlane) {
  const float yScale = 1.0f / std::tan(fovYRadians * 0.5f);
  const float xScale = yScale / aspect;
  const float zRange = farPlane - nearPlane;

  simd_float4x4 matrix{};
  matrix.columns[0] = {xScale, 0.0f, 0.0f, 0.0f};
  matrix.columns[1] = {0.0f, yScale, 0.0f, 0.0f};
  matrix.columns[2] = {0.0f, 0.0f, -(farPlane + nearPlane) / zRange, -1.0f};
  matrix.columns[3] = {0.0f, 0.0f, -(2.0f * farPlane * nearPlane) / zRange, 0.0f};
  return matrix;
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

bool loadIntegerStream(const std::string &programPath,
                       const char *emptyStreamError,
                       std::vector<int> &valuesOut,
                       std::string &errorOut) {
  valuesOut.clear();
  const std::string command = shellQuote(programPath);
  FILE *pipe = popen(command.c_str(), "r");
  if (pipe == nullptr) {
    errorOut = "failed to execute stream binary";
    return false;
  }

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
      errorOut = "invalid stream value: " + line;
      pclose(pipe);
      return false;
    }
    valuesOut.push_back(parsedValue);
  }

  const int rawExitCode = pclose(pipe);
  const int exitCode = decodeExitCode(rawExitCode);
  if (exitCode != 0) {
    errorOut = "stream binary exited with code " + std::to_string(exitCode);
    return false;
  }
  if (valuesOut.empty()) {
    errorOut = emptyStreamError;
    return false;
  }
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

bool loadGfxStream(const std::string &programPath,
                   GfxStreamHeader &headerOut,
                   std::vector<SimulationFrameState> &framesOut,
                   std::string &errorOut) {
  framesOut.clear();
  std::vector<int> values;
  if (!loadIntegerStream(programPath, "gfx stream was empty", values, errorOut)) {
    if (errorOut == "failed to execute stream binary") {
      errorOut = "failed to execute gfx stream binary";
    } else if (errorOut.rfind("invalid stream value: ", 0) == 0) {
      errorOut.replace(0, std::strlen("invalid stream value: "), "invalid gfx stream value: ");
    } else if (errorOut.rfind("stream binary exited with code ", 0) == 0) {
      errorOut.replace(0, std::strlen("stream binary exited with code "), "gfx stream binary exited with code ");
    }
    return false;
  }

  if (values.size() < GfxHeaderFieldCount) {
    errorOut = "gfx stream header was truncated";
    return false;
  }
  if (values[0] != GfxStreamMagic) {
    errorOut = "gfx stream magic mismatch";
    return false;
  }
  if (values[1] != GfxStreamVersion) {
    errorOut = "gfx stream version mismatch";
    return false;
  }

  headerOut.windowWidth = values[2];
  headerOut.windowHeight = values[3];
  headerOut.colorFormat = values[4];
  headerOut.depthFormat = values[5];
  headerOut.presentMode = values[6];
  headerOut.clearColorRMilli = values[7];
  headerOut.clearColorGMilli = values[8];
  headerOut.clearColorBMilli = values[9];
  headerOut.clearColorAMilli = values[10];
  headerOut.clearDepthMilli = values[11];
  headerOut.meshVertexCount = values[12];
  headerOut.meshIndexCount = values[13];
  headerOut.windowToken = values[14];
  headerOut.deviceToken = values[15];
  headerOut.queueToken = values[16];
  headerOut.swapchainToken = values[17];
  headerOut.meshToken = values[18];
  headerOut.pipelineToken = values[19];
  headerOut.materialToken = values[20];
  headerOut.frameToken = values[21];
  headerOut.renderPassToken = values[22];
  headerOut.drawToken = values[23];
  headerOut.endToken = values[24];
  headerOut.submitPresentMask = values[25];

  if (headerOut.windowWidth < 1 || headerOut.windowHeight < 1) {
    errorOut = "gfx stream reported invalid window size";
    return false;
  }
  if (headerOut.colorFormat != 0 || headerOut.depthFormat != 0 || headerOut.presentMode != 0) {
    errorOut = "gfx stream requested unsupported swapchain format";
    return false;
  }
  if (headerOut.meshVertexCount != static_cast<int>(CubeVertices.size()) ||
      headerOut.meshIndexCount != static_cast<int>(CubeIndices.size())) {
    errorOut = "gfx stream mesh counts did not match the locked cube layout";
    return false;
  }
  if (headerOut.submitPresentMask != 3) {
    errorOut = "gfx stream did not report successful submit/present";
    return false;
  }
  if (headerOut.windowToken < 1 || headerOut.deviceToken < 1 || headerOut.queueToken < 1 ||
      headerOut.swapchainToken < 1 || headerOut.meshToken < 1 || headerOut.pipelineToken < 1 ||
      headerOut.materialToken < 1 || headerOut.frameToken < 1 || headerOut.renderPassToken < 1 ||
      headerOut.drawToken < 1 || headerOut.endToken < 1) {
    errorOut = "gfx stream reported invalid resource tokens";
    return false;
  }

  const size_t frameValueCount = values.size() - GfxHeaderFieldCount;
  if (frameValueCount == 0 || (frameValueCount % SimulationStreamFieldsPerFrame) != 0) {
    errorOut = "gfx stream frame payload was not divisible by 4";
    return false;
  }

  for (size_t i = GfxHeaderFieldCount; i < values.size(); i += SimulationStreamFieldsPerFrame) {
    SimulationFrameState frame;
    frame.tick = values[i];
    frame.angleMilli = values[i + 1];
    frame.axisXCenti = values[i + 2];
    frame.axisYCenti = values[i + 3];
    framesOut.push_back(frame);
  }

  return true;
}

bool uploadSoftwareSurfaceFrameToTexture(id<MTLDevice> device,
                                         const primestruct::software_surface::SoftwareSurfaceFrame &frame,
                                         id<MTLTexture> __strong &textureOut,
                                         std::string &errorOut) {
  if (device == nil) {
    errorOut = "failed to create Metal device";
    return false;
  }
  if (!primestruct::software_surface::validateFrame(frame, errorOut)) {
    return false;
  }

  MTLTextureDescriptor *descriptor =
      [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                         width:static_cast<NSUInteger>(frame.width)
                                                        height:static_cast<NSUInteger>(frame.height)
                                                     mipmapped:NO];
  descriptor.storageMode = MTLStorageModeShared;
  descriptor.usage = MTLTextureUsageShaderRead;
  textureOut = [device newTextureWithDescriptor:descriptor];
  if (textureOut == nil) {
    errorOut = "failed to create software surface texture";
    return false;
  }

  const MTLRegion region = MTLRegionMake2D(0, 0, static_cast<NSUInteger>(frame.width), static_cast<NSUInteger>(frame.height));
  [textureOut replaceRegion:region
                mipmapLevel:0
                  withBytes:frame.pixelsBgra8.data()
                bytesPerRow:static_cast<NSUInteger>(frame.width * 4)];
  return true;
}

} // namespace

@class PrimeStructWindowHostDelegate;

@interface PrimeStructWindow : NSWindow
@property(nonatomic, assign) PrimeStructWindowHostDelegate *hostDelegate;
@end

@interface PrimeStructWindowHostDelegate : NSObject <NSApplicationDelegate, NSWindowDelegate>
- (instancetype)initWithMaxFrames:(int)maxFrames
                           gfxPath:(const std::string &)gfxPath
               simulationSmokeMode:(bool)simulationSmokeMode
           softwareSurfaceDemoMode:(bool)softwareSurfaceDemoMode;
- (void)failStartupStage:(StartupFailureStage)stage
                  reason:(const char *)reason
                 details:(const char *)details
            gfxErrorCode:(GfxErrorCode)gfxErrorCode;
- (void)failRuntimeCode:(int)code
                 reason:(const char *)reason
                details:(const char *)details
           gfxErrorCode:(GfxErrorCode)gfxErrorCode;
- (void)handleEscapeKey;
- (void)renderFrame:(NSTimer *)timer;
@end

@implementation PrimeStructWindow

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
  bool _softwareSurfaceDemoMode;
  bool _gfxMode;
  bool _firstFrameSubmitted;
  std::string _gfxPath;
  std::vector<SimulationFrameState> _simulationFrames;
  GfxStreamHeader _gfxHeader;
  id<MTLTexture> _softwareSurfaceTexture;
  NSUInteger _indexCount;
  CGSize _depthTextureSize;
}

- (instancetype)initWithMaxFrames:(int)maxFrames
                           gfxPath:(const std::string &)gfxPath
               simulationSmokeMode:(bool)simulationSmokeMode
           softwareSurfaceDemoMode:(bool)softwareSurfaceDemoMode {
  self = [super init];
  if (self != nil) {
    _maxFrames = maxFrames;
    _frameIndex = 0;
    _renderedFrameCount = 0;
    _printedFirstFrame = false;
    _printedSimulationFrame = false;
    _simulationSmokeMode = simulationSmokeMode;
    _softwareSurfaceDemoMode = softwareSurfaceDemoMode;
    _gfxMode = !gfxPath.empty();
    _firstFrameSubmitted = false;
    _gfxPath = gfxPath;
    _indexCount = CubeIndices.size();
    _depthTextureSize = CGSizeMake(0.0, 0.0);
  }
  return self;
}

- (void)failStartupStage:(StartupFailureStage)stage
                  reason:(const char *)reason
                 details:(const char *)details
            gfxErrorCode:(GfxErrorCode)gfxErrorCode {
  const int exitCode = startupStageExitCode(stage);
  std::cerr << "startup_failure=1\n";
  std::cerr << "gfx_profile=" << deducedGfxProfileName() << "\n";
  std::cerr << "startup_failure_stage=" << startupStageName(stage) << "\n";
  std::cerr << "startup_failure_reason=" << reason << "\n";
  std::cerr << "startup_failure_exit_code=" << exitCode << "\n";
  if (gfxErrorCode != GfxErrorCode::None) {
    std::cerr << "gfx_error_code=" << gfxErrorCodeName(gfxErrorCode) << "\n";
  }
  if (details != nullptr && details[0] != '\0') {
    std::cerr << "startup_failure_detail=" << details << "\n";
    if (gfxErrorCode != GfxErrorCode::None) {
      std::cerr << "gfx_error_why=" << details << "\n";
    }
  }
  gExitCode = exitCode;
  [NSApp terminate:nil];
}

- (void)failRuntimeCode:(int)code
                 reason:(const char *)reason
                details:(const char *)details
           gfxErrorCode:(GfxErrorCode)gfxErrorCode {
  std::cerr << "runtime_failure=1\n";
  std::cerr << "gfx_profile=" << deducedGfxProfileName() << "\n";
  std::cerr << "runtime_failure_reason=" << reason << "\n";
  std::cerr << "runtime_failure_exit_code=" << code << "\n";
  if (gfxErrorCode != GfxErrorCode::None) {
    std::cerr << "gfx_error_code=" << gfxErrorCodeName(gfxErrorCode) << "\n";
  }
  if (details != nullptr && details[0] != '\0') {
    std::cerr << "runtime_failure_detail=" << details << "\n";
    if (gfxErrorCode != GfxErrorCode::None) {
      std::cerr << "gfx_error_why=" << details << "\n";
    }
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

  if (!_softwareSurfaceDemoMode) {
    std::string simulationError;
    if (!loadGfxStream(_gfxPath, _gfxHeader, _simulationFrames, simulationError)) {
      [self failStartupStage:StartupFailureStage::SimulationStreamLoad
                      reason:"gfx_stream_load_failed"
                     details:simulationError.c_str()
                gfxErrorCode:GfxErrorCode::None];
      return;
    }
    std::cout << "gfx_stream_loaded=1\n";
    std::cout << "gfx_window_width=" << _gfxHeader.windowWidth << "\n";
    std::cout << "gfx_window_height=" << _gfxHeader.windowHeight << "\n";
    std::cout << "gfx_mesh_vertex_count=" << _gfxHeader.meshVertexCount << "\n";
    std::cout << "gfx_mesh_index_count=" << _gfxHeader.meshIndexCount << "\n";
    std::cout << "gfx_submit_present_mask=" << _gfxHeader.submitPresentMask << "\n";
    std::cout << "gfx_frame_token=" << _gfxHeader.frameToken << "\n";
    std::cout << "gfx_render_pass_token=" << _gfxHeader.renderPassToken << "\n";

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
  }

  _device = MTLCreateSystemDefaultDevice();
  if (_device == nil) {
    [self failStartupStage:StartupFailureStage::GpuDeviceAcquisition
                    reason:"metal_device_creation_failed"
                   details:"failed to create Metal device"
              gfxErrorCode:GfxErrorCode::DeviceCreateFailed];
    return;
  }

  _queue = [_device newCommandQueue];
  if (_queue == nil) {
    [self failStartupStage:StartupFailureStage::GpuDeviceAcquisition
                    reason:"command_queue_creation_failed"
                   details:"failed to create command queue"
              gfxErrorCode:GfxErrorCode::DeviceCreateFailed];
    return;
  }

  if (_softwareSurfaceDemoMode) {
    const primestruct::software_surface::SoftwareSurfaceFrame surfaceFrame =
        primestruct::software_surface::makeDemoFrame();
    std::string surfaceError;
    if (!uploadSoftwareSurfaceFrameToTexture(_device, surfaceFrame, _softwareSurfaceTexture, surfaceError)) {
      [self failStartupStage:StartupFailureStage::PipelineSetup
                      reason:"software_surface_texture_creation_failed"
                     details:surfaceError.c_str()
                gfxErrorCode:GfxErrorCode::MaterialCreateFailed];
      return;
    }
    std::cout << "software_surface_bridge=1\n";
    std::cout << "software_surface_width=" << surfaceFrame.width << "\n";
    std::cout << "software_surface_height=" << surfaceFrame.height << "\n";
  } else {
    NSError *libraryError = nil;
    NSString *shaderSource = [NSString stringWithUTF8String:WindowShaderSource];
    id<MTLLibrary> library = [_device newLibraryWithSource:shaderSource options:nil error:&libraryError];
    if (library == nil) {
      const char *details = libraryError == nil ? "unknown" : libraryError.localizedDescription.UTF8String;
      [self failStartupStage:StartupFailureStage::ShaderLoad
                      reason:"shader_library_compile_failed"
                     details:details
                gfxErrorCode:GfxErrorCode::PipelineCreateFailed];
      return;
    }
    std::cout << "shader_library_ready=1\n";

    id<MTLFunction> vertexFn = [library newFunctionWithName:@"windowVertexMain"];
    id<MTLFunction> fragmentFn = [library newFunctionWithName:@"windowFragmentMain"];
    if (vertexFn == nil || fragmentFn == nil) {
      [self failStartupStage:StartupFailureStage::ShaderLoad
                      reason:"shader_entrypoint_missing"
                     details:"missing window host shader entrypoints"
                gfxErrorCode:GfxErrorCode::PipelineCreateFailed];
      return;
    }

    MTLVertexDescriptor *vertexDescriptor = [[MTLVertexDescriptor alloc] init];
    vertexDescriptor.attributes[0].format = MTLVertexFormatFloat4;
    vertexDescriptor.attributes[0].offset = offsetof(WindowVertex, px);
    vertexDescriptor.attributes[0].bufferIndex = 0;
    vertexDescriptor.attributes[1].format = MTLVertexFormatFloat4;
    vertexDescriptor.attributes[1].offset = offsetof(WindowVertex, r);
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
      [self failStartupStage:StartupFailureStage::PipelineSetup
                      reason:"render_pipeline_creation_failed"
                     details:details
                gfxErrorCode:GfxErrorCode::PipelineCreateFailed];
      return;
    }

    MTLDepthStencilDescriptor *depthDescriptor = [[MTLDepthStencilDescriptor alloc] init];
    depthDescriptor.depthCompareFunction = MTLCompareFunctionLess;
    depthDescriptor.depthWriteEnabled = YES;
    _depthState = [_device newDepthStencilStateWithDescriptor:depthDescriptor];
    if (_depthState == nil) {
      [self failStartupStage:StartupFailureStage::PipelineSetup
                      reason:"depth_state_creation_failed"
                     details:"failed to create depth state"
                gfxErrorCode:GfxErrorCode::PipelineCreateFailed];
      return;
    }

    _vertexBuffer = [_device newBufferWithBytes:CubeVertices.data()
                                          length:(CubeVertices.size() * sizeof(WindowVertex))
                                         options:MTLResourceStorageModeShared];
    if (_vertexBuffer == nil) {
      [self failStartupStage:StartupFailureStage::PipelineSetup
                      reason:"vertex_buffer_creation_failed"
                     details:"failed to create vertex buffer"
                gfxErrorCode:GfxErrorCode::MeshCreateFailed];
      return;
    }

    _indexBuffer = [_device newBufferWithBytes:CubeIndices.data()
                                         length:(CubeIndices.size() * sizeof(uint16_t))
                                        options:MTLResourceStorageModeShared];
    if (_indexBuffer == nil) {
      [self failStartupStage:StartupFailureStage::PipelineSetup
                      reason:"index_buffer_creation_failed"
                     details:"failed to create index buffer"
                gfxErrorCode:GfxErrorCode::MeshCreateFailed];
      return;
    }

    _uniformBuffer = [_device newBufferWithLength:sizeof(WindowUniforms) options:MTLResourceStorageModeShared];
    if (_uniformBuffer == nil) {
      [self failStartupStage:StartupFailureStage::PipelineSetup
                      reason:"uniform_buffer_creation_failed"
                     details:"failed to create uniform buffer"
                gfxErrorCode:GfxErrorCode::MaterialCreateFailed];
      return;
    }

    std::cout << "vertex_buffer_ready=1\n";
    std::cout << "index_buffer_ready=1\n";
    std::cout << "uniform_buffer_ready=1\n";
  }

  const CGFloat windowWidth = _gfxMode ? static_cast<CGFloat>(_gfxHeader.windowWidth) : 960.0;
  const CGFloat windowHeight = _gfxMode ? static_cast<CGFloat>(_gfxHeader.windowHeight) : 640.0;
  NSRect frame = NSMakeRect(120.0, 120.0, windowWidth, windowHeight);
  const NSWindowStyleMask styleMask = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                                      NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable;
  _window = [[PrimeStructWindow alloc] initWithContentRect:frame
                                                  styleMask:styleMask
                                                    backing:NSBackingStoreBuffered
                                                      defer:NO];
  if (_window == nil) {
    [self failStartupStage:StartupFailureStage::WindowCreation
                    reason:"window_creation_failed"
                   details:"failed to create native window"
              gfxErrorCode:GfxErrorCode::WindowCreateFailed];
    return;
  }
  _window.hostDelegate = self;
  _window.title = _gfxMode ? @"PrimeStruct /std/gfx (Window Host)"
                           : @"PrimeStruct Spinning Cube (Window Host)";
  _window.delegate = self;

  NSView *contentView = _window.contentView;
  if (contentView == nil) {
    [self failStartupStage:StartupFailureStage::WindowCreation
                    reason:"window_content_view_missing"
                   details:"native window content view was unavailable"
              gfxErrorCode:GfxErrorCode::WindowCreateFailed];
    return;
  }

  _metalLayer = [CAMetalLayer layer];
  if (_metalLayer == nil) {
    [self failStartupStage:StartupFailureStage::WindowCreation
                    reason:"swapchain_layer_creation_failed"
                   details:"failed to create CAMetalLayer swapchain"
              gfxErrorCode:GfxErrorCode::SwapchainCreateFailed];
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
  [NSApp activateIgnoringOtherApps:YES];

  std::cout << "window_created=1\n";
  std::cout << "swapchain_layer_created=1\n";
  if (_softwareSurfaceDemoMode) {
    std::cout << "software_surface_presenter_ready=1\n";
  } else {
    std::cout << "pipeline_ready=1\n";
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
    if (_metalLayer == nil || _queue == nil) {
      return;
    }
    if (_softwareSurfaceDemoMode) {
      if (_softwareSurfaceTexture == nil) {
        return;
      }
    } else if (_pipeline == nil || _simulationFrames.empty()) {
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
        [self failStartupStage:StartupFailureStage::FirstFrameSubmission
                        reason:"frame_drawable_acquisition_failed"
                       details:"failed to acquire drawable from CAMetalLayer"
                  gfxErrorCode:GfxErrorCode::FrameAcquireFailed];
      } else {
        [self failRuntimeCode:82
                       reason:"frame_drawable_acquisition_failed"
                      details:"failed to acquire drawable from CAMetalLayer"
                 gfxErrorCode:GfxErrorCode::FrameAcquireFailed];
      }
      return;
    }

    if (_softwareSurfaceDemoMode) {
      id<MTLCommandBuffer> commandBuffer = [_queue commandBuffer];
      if (commandBuffer == nil) {
        if (!_firstFrameSubmitted) {
          [self failStartupStage:StartupFailureStage::FirstFrameSubmission
                          reason:"command_buffer_creation_failed"
                         details:"failed to create command buffer"
                    gfxErrorCode:GfxErrorCode::QueueSubmitFailed];
        } else {
          [self failRuntimeCode:83
                         reason:"command_buffer_creation_failed"
                        details:"failed to create command buffer"
                   gfxErrorCode:GfxErrorCode::QueueSubmitFailed];
        }
        return;
      }
      id<MTLBlitCommandEncoder> blitEncoder = [commandBuffer blitCommandEncoder];
      if (blitEncoder == nil) {
        if (!_firstFrameSubmitted) {
          [self failStartupStage:StartupFailureStage::FirstFrameSubmission
                          reason:"software_surface_blit_encoder_creation_failed"
                         details:"failed to create software surface blit encoder"
                    gfxErrorCode:GfxErrorCode::QueueSubmitFailed];
        } else {
          [self failRuntimeCode:81
                         reason:"software_surface_blit_encoder_creation_failed"
                        details:"failed to create software surface blit encoder"
                   gfxErrorCode:GfxErrorCode::QueueSubmitFailed];
        }
        return;
      }

      const NSUInteger blitWidth = std::min(drawable.texture.width, _softwareSurfaceTexture.width);
      const NSUInteger blitHeight = std::min(drawable.texture.height, _softwareSurfaceTexture.height);
      [blitEncoder copyFromTexture:_softwareSurfaceTexture
                       sourceSlice:0
                       sourceLevel:0
                      sourceOrigin:MTLOriginMake(0, 0, 0)
                        sourceSize:MTLSizeMake(blitWidth, blitHeight, 1)
                         toTexture:drawable.texture
                  destinationSlice:0
                  destinationLevel:0
                 destinationOrigin:MTLOriginMake(0, 0, 0)];
      [blitEncoder endEncoding];

      [commandBuffer presentDrawable:drawable];
      [commandBuffer commit];

      if (!_firstFrameSubmitted) {
        [commandBuffer waitUntilCompleted];
        if (commandBuffer.status != MTLCommandBufferStatusCompleted) {
          NSError *commandBufferError = commandBuffer.error;
          const char *details = commandBufferError == nil ? "software surface command buffer did not complete"
                                                          : commandBufferError.localizedDescription.UTF8String;
          [self failStartupStage:StartupFailureStage::FirstFrameSubmission
                          reason:"software_surface_submit_failed"
                         details:details
                    gfxErrorCode:GfxErrorCode::QueueSubmitFailed];
          return;
        }
        std::cout << "gfx_profile=" << deducedGfxProfileName() << "\n";
        std::cout << "startup_success=1\n";
        _firstFrameSubmitted = true;
      }

      if (!_printedFirstFrame) {
        std::cout << "software_surface_presented=1\n";
        std::cout << "frame_rendered=1\n";
        _printedFirstFrame = true;
      }

      _renderedFrameCount += 1;
      if (_maxFrames > 0 && _renderedFrameCount >= _maxFrames) {
        std::cout << "exit_reason=max_frames\n";
        gExitCode = 0;
        [NSApp terminate:nil];
      }
      return;
    }

    [self ensureDepthTextureForSize:drawableSize];
    if (_depthTexture == nil) {
      if (!_firstFrameSubmitted) {
        [self failStartupStage:StartupFailureStage::FirstFrameSubmission
                        reason:"depth_texture_creation_failed"
                       details:"failed to create depth texture"
                  gfxErrorCode:GfxErrorCode::FrameAcquireFailed];
      } else {
        [self failRuntimeCode:80
                       reason:"depth_texture_creation_failed"
                      details:"failed to create depth texture"
                 gfxErrorCode:GfxErrorCode::FrameAcquireFailed];
      }
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
    const double clearRed = _gfxMode ? (static_cast<double>(_gfxHeader.clearColorRMilli) / 1000.0)
                                     : 0.03 + (0.08 * axisXNorm);
    const double clearGreen = _gfxMode ? (static_cast<double>(_gfxHeader.clearColorGMilli) / 1000.0)
                                       : 0.04 + (0.08 * axisYNorm);
    const double clearBlue = _gfxMode ? (static_cast<double>(_gfxHeader.clearColorBMilli) / 1000.0) : 0.06;
    const double clearAlpha = _gfxMode ? (static_cast<double>(_gfxHeader.clearColorAMilli) / 1000.0) : 1.0;
    const double clearDepth = _gfxMode ? (static_cast<double>(_gfxHeader.clearDepthMilli) / 1000.0) : 1.0;

    MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].texture = drawable.texture;
    pass.colorAttachments[0].loadAction = MTLLoadActionClear;
    pass.colorAttachments[0].storeAction = MTLStoreActionStore;
    pass.colorAttachments[0].clearColor = MTLClearColorMake(clearRed, clearGreen, clearBlue, clearAlpha);
    pass.depthAttachment.texture = _depthTexture;
    pass.depthAttachment.loadAction = MTLLoadActionClear;
    pass.depthAttachment.storeAction = MTLStoreActionDontCare;
    pass.depthAttachment.clearDepth = clearDepth;

    id<MTLCommandBuffer> commandBuffer = [_queue commandBuffer];
    if (commandBuffer == nil) {
      if (!_firstFrameSubmitted) {
        [self failStartupStage:StartupFailureStage::FirstFrameSubmission
                        reason:"command_buffer_creation_failed"
                       details:"failed to create command buffer"
                  gfxErrorCode:GfxErrorCode::QueueSubmitFailed];
      } else {
        [self failRuntimeCode:83
                       reason:"command_buffer_creation_failed"
                      details:"failed to create command buffer"
                 gfxErrorCode:GfxErrorCode::QueueSubmitFailed];
      }
      return;
    }
    id<MTLRenderCommandEncoder> encoder = [commandBuffer renderCommandEncoderWithDescriptor:pass];
    if (encoder == nil) {
      if (!_firstFrameSubmitted) {
        [self failStartupStage:StartupFailureStage::FirstFrameSubmission
                        reason:"render_encoder_creation_failed"
                       details:"failed to create render command encoder"
                  gfxErrorCode:GfxErrorCode::QueueSubmitFailed];
      } else {
        [self failRuntimeCode:81
                     reason:"render_encoder_creation_failed"
                    details:"failed to create render command encoder"
               gfxErrorCode:GfxErrorCode::QueueSubmitFailed];
      }
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

    if (!_firstFrameSubmitted) {
      [commandBuffer waitUntilCompleted];
      if (commandBuffer.status != MTLCommandBufferStatusCompleted) {
        NSError *commandBufferError = commandBuffer.error;
        const char *details = commandBufferError == nil ? "first frame command buffer did not complete"
                                                        : commandBufferError.localizedDescription.UTF8String;
        [self failStartupStage:StartupFailureStage::FirstFrameSubmission
                        reason:"first_frame_submit_failed"
                       details:details
                  gfxErrorCode:GfxErrorCode::QueueSubmitFailed];
        return;
      }
      std::cout << "gfx_profile=" << deducedGfxProfileName() << "\n";
      std::cout << "startup_success=1\n";
      _firstFrameSubmitted = true;
    }

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

int main(int argc, char **argv) {
  int maxFrames = 0;
  std::string gfxPath;
  bool simulationSmokeMode = false;
  bool softwareSurfaceDemoMode = false;

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--help") {
      std::cout
          << "usage: window_host (--gfx <path> | --software-surface-demo) "
             "[--max-frames <positive-int>] [--simulation-smoke]\n";
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
    if (arg == "--gfx") {
      if (i + 1 >= argc) {
        std::cerr << "missing value for --gfx\n";
        return 64;
      }
      gfxPath = argv[i + 1];
      i += 1;
      continue;
    }
    if (arg == "--simulation-smoke") {
      simulationSmokeMode = true;
      continue;
    }
    if (arg == "--software-surface-demo") {
      softwareSurfaceDemoMode = true;
      continue;
    }
    std::cerr << "unknown arg: " << arg << "\n";
    return 64;
  }

  if (!gfxPath.empty() && softwareSurfaceDemoMode) {
    std::cerr << "--gfx is incompatible with --software-surface-demo\n";
    return 64;
  }
  if (simulationSmokeMode && softwareSurfaceDemoMode) {
    std::cerr << "--simulation-smoke is incompatible with --software-surface-demo\n";
    return 64;
  }
  if (gfxPath.empty() && !softwareSurfaceDemoMode) {
    std::cerr << "missing required --gfx <path> or --software-surface-demo\n";
    return 64;
  }

  @autoreleasepool {
    NSApplication *app = [NSApplication sharedApplication];
    app.activationPolicy =
        simulationSmokeMode ? NSApplicationActivationPolicyProhibited : NSApplicationActivationPolicyRegular;
    PrimeStructWindowHostDelegate *delegate = [[PrimeStructWindowHostDelegate alloc] initWithMaxFrames:maxFrames
                                                                                                gfxPath:gfxPath
                                                                                    simulationSmokeMode:simulationSmokeMode
                                                                                softwareSurfaceDemoMode:softwareSurfaceDemoMode];
    app.delegate = delegate;
    [app run];
  }
  return gExitCode;
}
