#import <AppKit/AppKit.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include "../../shared/gfx_contract_shared.h"
#include "../../shared/native_metal_window_host.h"
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

using GfxErrorCode = primestruct::gfx_contract::GfxErrorCode;
using FailureInfo = primestruct::native_metal_window::FailureInfo;
using PresenterCallbacks = primestruct::native_metal_window::PresenterCallbacks;
using PresenterConfig = primestruct::native_metal_window::PresenterConfig;
using RenderFrameResult = primestruct::native_metal_window::RenderFrameResult;
using StartupFailureStage = primestruct::native_metal_window::StartupFailureStage;
using WindowVertex = primestruct::gfx_contract::VertexColoredHost;

const char *deducedGfxProfileName() {
  return "native-desktop";
}

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

struct PrimeStructWindowHostState {
  bool softwareSurfaceDemoMode = false;
  bool gfxMode = false;
  id<MTLDevice> device = nil;
  id<MTLRenderPipelineState> pipeline = nil;
  id<MTLDepthStencilState> depthState = nil;
  id<MTLBuffer> vertexBuffer = nil;
  id<MTLBuffer> indexBuffer = nil;
  id<MTLBuffer> uniformBuffer = nil;
  id<MTLTexture> depthTexture = nil;
  id<MTLTexture> softwareSurfaceTexture = nil;
  std::vector<SimulationFrameState> simulationFrames;
  GfxStreamHeader gfxHeader;
  NSUInteger indexCount = static_cast<NSUInteger>(CubeIndices.size());
  CGSize depthTextureSize = CGSizeMake(0.0, 0.0);
  int frameIndex = 0;
  bool printedSimulationFrame = false;
  bool printedSoftwareSurfacePresented = false;
};

void ensureDepthTextureForSize(PrimeStructWindowHostState &state, CGSize drawableSize) {
  if (state.depthTexture != nil && state.depthTextureSize.width == drawableSize.width &&
      state.depthTextureSize.height == drawableSize.height) {
    return;
  }

  MTLTextureDescriptor *depthDescriptor =
      [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float
                                                         width:static_cast<NSUInteger>(drawableSize.width)
                                                        height:static_cast<NSUInteger>(drawableSize.height)
                                                     mipmapped:NO];
  depthDescriptor.storageMode = MTLStorageModePrivate;
  depthDescriptor.usage = MTLTextureUsageRenderTarget;
  state.depthTexture = [state.device newTextureWithDescriptor:depthDescriptor];
  state.depthTextureSize = drawableSize;
}

bool prepareWindowHostContent(void *context,
                              id<MTLDevice> device,
                              id<MTLCommandQueue> queue,
                              FailureInfo &failureOut) {
  (void)queue;
  auto &state = *reinterpret_cast<PrimeStructWindowHostState *>(context);
  state.device = device;

  if (state.softwareSurfaceDemoMode) {
    const primestruct::software_surface::SoftwareSurfaceFrame surfaceFrame =
        primestruct::software_surface::makeDemoFrame();
    std::string surfaceError;
    if (!uploadSoftwareSurfaceFrameToTexture(device, surfaceFrame, state.softwareSurfaceTexture, surfaceError)) {
      failureOut.startupStage = StartupFailureStage::PipelineSetup;
      failureOut.reason = "software_surface_texture_creation_failed";
      failureOut.details = surfaceError;
      failureOut.gfxErrorCode = GfxErrorCode::MaterialCreateFailed;
      return false;
    }
    std::cout << "software_surface_bridge=1\n";
    std::cout << "software_surface_width=" << surfaceFrame.width << "\n";
    std::cout << "software_surface_height=" << surfaceFrame.height << "\n";
    return true;
  }

  NSError *libraryError = nil;
  NSString *shaderSource = [NSString stringWithUTF8String:WindowShaderSource];
  id<MTLLibrary> library = [device newLibraryWithSource:shaderSource options:nil error:&libraryError];
  if (library == nil) {
    failureOut.startupStage = StartupFailureStage::ShaderLoad;
    failureOut.reason = "shader_library_compile_failed";
    failureOut.details = libraryError == nil ? "unknown" : libraryError.localizedDescription.UTF8String;
    failureOut.gfxErrorCode = GfxErrorCode::PipelineCreateFailed;
    return false;
  }
  std::cout << "shader_library_ready=1\n";

  id<MTLFunction> vertexFn = [library newFunctionWithName:@"windowVertexMain"];
  id<MTLFunction> fragmentFn = [library newFunctionWithName:@"windowFragmentMain"];
  if (vertexFn == nil || fragmentFn == nil) {
    failureOut.startupStage = StartupFailureStage::ShaderLoad;
    failureOut.reason = "shader_entrypoint_missing";
    failureOut.details = "missing window host shader entrypoints";
    failureOut.gfxErrorCode = GfxErrorCode::PipelineCreateFailed;
    return false;
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
  state.pipeline = [device newRenderPipelineStateWithDescriptor:pipelineDesc error:&pipelineError];
  if (state.pipeline == nil) {
    failureOut.startupStage = StartupFailureStage::PipelineSetup;
    failureOut.reason = "render_pipeline_creation_failed";
    failureOut.details = pipelineError == nil ? "unknown" : pipelineError.localizedDescription.UTF8String;
    failureOut.gfxErrorCode = GfxErrorCode::PipelineCreateFailed;
    return false;
  }

  MTLDepthStencilDescriptor *depthDescriptor = [[MTLDepthStencilDescriptor alloc] init];
  depthDescriptor.depthCompareFunction = MTLCompareFunctionLess;
  depthDescriptor.depthWriteEnabled = YES;
  state.depthState = [device newDepthStencilStateWithDescriptor:depthDescriptor];
  if (state.depthState == nil) {
    failureOut.startupStage = StartupFailureStage::PipelineSetup;
    failureOut.reason = "depth_state_creation_failed";
    failureOut.details = "failed to create depth state";
    failureOut.gfxErrorCode = GfxErrorCode::PipelineCreateFailed;
    return false;
  }

  state.vertexBuffer = [device newBufferWithBytes:CubeVertices.data()
                                           length:(CubeVertices.size() * sizeof(WindowVertex))
                                          options:MTLResourceStorageModeShared];
  if (state.vertexBuffer == nil) {
    failureOut.startupStage = StartupFailureStage::PipelineSetup;
    failureOut.reason = "vertex_buffer_creation_failed";
    failureOut.details = "failed to create vertex buffer";
    failureOut.gfxErrorCode = GfxErrorCode::MeshCreateFailed;
    return false;
  }

  state.indexBuffer = [device newBufferWithBytes:CubeIndices.data()
                                          length:(CubeIndices.size() * sizeof(uint16_t))
                                         options:MTLResourceStorageModeShared];
  if (state.indexBuffer == nil) {
    failureOut.startupStage = StartupFailureStage::PipelineSetup;
    failureOut.reason = "index_buffer_creation_failed";
    failureOut.details = "failed to create index buffer";
    failureOut.gfxErrorCode = GfxErrorCode::MeshCreateFailed;
    return false;
  }

  state.uniformBuffer = [device newBufferWithLength:sizeof(WindowUniforms) options:MTLResourceStorageModeShared];
  if (state.uniformBuffer == nil) {
    failureOut.startupStage = StartupFailureStage::PipelineSetup;
    failureOut.reason = "uniform_buffer_creation_failed";
    failureOut.details = "failed to create uniform buffer";
    failureOut.gfxErrorCode = GfxErrorCode::MaterialCreateFailed;
    return false;
  }

  std::cout << "vertex_buffer_ready=1\n";
  std::cout << "index_buffer_ready=1\n";
  std::cout << "uniform_buffer_ready=1\n";
  return true;
}

void emitPresenterReadyDiagnostics(void *context) {
  const auto &state = *reinterpret_cast<const PrimeStructWindowHostState *>(context);
  if (state.softwareSurfaceDemoMode) {
    std::cout << "software_surface_presenter_ready=1\n";
  } else {
    std::cout << "pipeline_ready=1\n";
  }
}

RenderFrameResult renderWindowHostFrame(void *context,
                                        id<MTLCommandQueue> queue,
                                        id<CAMetalDrawable> drawable,
                                        CGSize drawableSize,
                                        bool firstFrameSubmitted) {
  auto &state = *reinterpret_cast<PrimeStructWindowHostState *>(context);
  RenderFrameResult result;

  if (state.softwareSurfaceDemoMode) {
    if (state.softwareSurfaceTexture == nil) {
      return result;
    }

    id<MTLCommandBuffer> commandBuffer = [queue commandBuffer];
    if (commandBuffer == nil) {
      result.ok = false;
      result.failure.startupStage = StartupFailureStage::FirstFrameSubmission;
      result.failure.runtimeExitCode = 83;
      result.failure.reason = "command_buffer_creation_failed";
      result.failure.details = "failed to create command buffer";
      result.failure.gfxErrorCode = GfxErrorCode::QueueSubmitFailed;
      return result;
    }

    id<MTLBlitCommandEncoder> blitEncoder = [commandBuffer blitCommandEncoder];
    if (blitEncoder == nil) {
      result.ok = false;
      result.failure.startupStage = StartupFailureStage::FirstFrameSubmission;
      result.failure.runtimeExitCode = 81;
      result.failure.reason = "software_surface_blit_encoder_creation_failed";
      result.failure.details = "failed to create software surface blit encoder";
      result.failure.gfxErrorCode = GfxErrorCode::QueueSubmitFailed;
      return result;
    }

    const NSUInteger blitWidth = std::min(drawable.texture.width, state.softwareSurfaceTexture.width);
    const NSUInteger blitHeight = std::min(drawable.texture.height, state.softwareSurfaceTexture.height);
    [blitEncoder copyFromTexture:state.softwareSurfaceTexture
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

    if (!firstFrameSubmitted) {
      [commandBuffer waitUntilCompleted];
      if (commandBuffer.status != MTLCommandBufferStatusCompleted) {
        NSError *commandBufferError = commandBuffer.error;
        result.ok = false;
        result.failure.startupStage = StartupFailureStage::FirstFrameSubmission;
        result.failure.reason = "software_surface_submit_failed";
        result.failure.details = commandBufferError == nil ? "software surface command buffer did not complete"
                                                           : commandBufferError.localizedDescription.UTF8String;
        result.failure.gfxErrorCode = GfxErrorCode::QueueSubmitFailed;
        return result;
      }
    }

    if (!state.printedSoftwareSurfacePresented) {
      std::cout << "software_surface_presented=1\n";
      state.printedSoftwareSurfacePresented = true;
    }

    result.renderedFrame = true;
    return result;
  }

  if (state.pipeline == nil || state.simulationFrames.empty()) {
    return result;
  }

  ensureDepthTextureForSize(state, drawableSize);
  if (state.depthTexture == nil) {
    result.ok = false;
    result.failure.startupStage = StartupFailureStage::FirstFrameSubmission;
    result.failure.runtimeExitCode = 80;
    result.failure.reason = "depth_texture_creation_failed";
    result.failure.details = "failed to create depth texture";
    result.failure.gfxErrorCode = GfxErrorCode::FrameAcquireFailed;
    return result;
  }

  const size_t frameSlot = std::min(static_cast<size_t>(state.frameIndex), state.simulationFrames.size() - 1);
  const SimulationFrameState &simulationFrame = state.simulationFrames[frameSlot];
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

  auto *uniforms = reinterpret_cast<WindowUniforms *>(state.uniformBuffer.contents);
  uniforms->modelViewProjection = simd_mul(projection, simd_mul(view, model));

  const double axisXNorm =
      std::clamp((static_cast<double>(simulationFrame.axisXCenti) + 100.0) / 200.0, 0.0, 1.0);
  const double axisYNorm =
      std::clamp((static_cast<double>(simulationFrame.axisYCenti) + 100.0) / 200.0, 0.0, 1.0);
  const double clearRed = state.gfxMode ? (static_cast<double>(state.gfxHeader.clearColorRMilli) / 1000.0)
                                        : 0.03 + (0.08 * axisXNorm);
  const double clearGreen = state.gfxMode ? (static_cast<double>(state.gfxHeader.clearColorGMilli) / 1000.0)
                                          : 0.04 + (0.08 * axisYNorm);
  const double clearBlue = state.gfxMode ? (static_cast<double>(state.gfxHeader.clearColorBMilli) / 1000.0) : 0.06;
  const double clearAlpha = state.gfxMode ? (static_cast<double>(state.gfxHeader.clearColorAMilli) / 1000.0) : 1.0;
  const double clearDepth = state.gfxMode ? (static_cast<double>(state.gfxHeader.clearDepthMilli) / 1000.0) : 1.0;

  MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
  pass.colorAttachments[0].texture = drawable.texture;
  pass.colorAttachments[0].loadAction = MTLLoadActionClear;
  pass.colorAttachments[0].storeAction = MTLStoreActionStore;
  pass.colorAttachments[0].clearColor = MTLClearColorMake(clearRed, clearGreen, clearBlue, clearAlpha);
  pass.depthAttachment.texture = state.depthTexture;
  pass.depthAttachment.loadAction = MTLLoadActionClear;
  pass.depthAttachment.storeAction = MTLStoreActionDontCare;
  pass.depthAttachment.clearDepth = clearDepth;

  id<MTLCommandBuffer> commandBuffer = [queue commandBuffer];
  if (commandBuffer == nil) {
    result.ok = false;
    result.failure.startupStage = StartupFailureStage::FirstFrameSubmission;
    result.failure.runtimeExitCode = 83;
    result.failure.reason = "command_buffer_creation_failed";
    result.failure.details = "failed to create command buffer";
    result.failure.gfxErrorCode = GfxErrorCode::QueueSubmitFailed;
    return result;
  }

  id<MTLRenderCommandEncoder> encoder = [commandBuffer renderCommandEncoderWithDescriptor:pass];
  if (encoder == nil) {
    result.ok = false;
    result.failure.startupStage = StartupFailureStage::FirstFrameSubmission;
    result.failure.runtimeExitCode = 81;
    result.failure.reason = "render_encoder_creation_failed";
    result.failure.details = "failed to create render command encoder";
    result.failure.gfxErrorCode = GfxErrorCode::QueueSubmitFailed;
    return result;
  }

  [encoder setRenderPipelineState:state.pipeline];
  [encoder setDepthStencilState:state.depthState];
  [encoder setVertexBuffer:state.vertexBuffer offset:0 atIndex:0];
  [encoder setVertexBuffer:state.uniformBuffer offset:0 atIndex:1];
  [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                      indexCount:state.indexCount
                       indexType:MTLIndexTypeUInt16
                     indexBuffer:state.indexBuffer
               indexBufferOffset:0];
  [encoder endEncoding];

  [commandBuffer presentDrawable:drawable];
  [commandBuffer commit];

  if (!firstFrameSubmitted) {
    [commandBuffer waitUntilCompleted];
    if (commandBuffer.status != MTLCommandBufferStatusCompleted) {
      NSError *commandBufferError = commandBuffer.error;
      result.ok = false;
      result.failure.startupStage = StartupFailureStage::FirstFrameSubmission;
      result.failure.reason = "first_frame_submit_failed";
      result.failure.details = commandBufferError == nil ? "first frame command buffer did not complete"
                                                         : commandBufferError.localizedDescription.UTF8String;
      result.failure.gfxErrorCode = GfxErrorCode::QueueSubmitFailed;
      return result;
    }
  }

  if (!state.printedSimulationFrame) {
    std::cout << "simulation_tick=" << simulationFrame.tick << "\n";
    std::cout << "simulation_angle_milli=" << simulationFrame.angleMilli << "\n";
    std::cout << "simulation_axis_x_centi=" << simulationFrame.axisXCenti << "\n";
    std::cout << "simulation_axis_y_centi=" << simulationFrame.axisYCenti << "\n";
    state.printedSimulationFrame = true;
  }

  if (state.frameIndex + 1 < static_cast<int>(state.simulationFrames.size())) {
    state.frameIndex += 1;
  }

  result.renderedFrame = true;
  return result;
}

} // namespace

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

  PrimeStructWindowHostState state;
  state.softwareSurfaceDemoMode = softwareSurfaceDemoMode;
  state.gfxMode = !gfxPath.empty();

  if (!softwareSurfaceDemoMode) {
    std::string simulationError;
    if (!loadGfxStream(gfxPath, state.gfxHeader, state.simulationFrames, simulationError)) {
      return primestruct::native_metal_window::emitStartupFailure(
          deducedGfxProfileName(),
          StartupFailureStage::SimulationStreamLoad,
          "gfx_stream_load_failed",
          simulationError,
          GfxErrorCode::None);
    }

    std::cout << "gfx_stream_loaded=1\n";
    std::cout << "gfx_window_width=" << state.gfxHeader.windowWidth << "\n";
    std::cout << "gfx_window_height=" << state.gfxHeader.windowHeight << "\n";
    std::cout << "gfx_mesh_vertex_count=" << state.gfxHeader.meshVertexCount << "\n";
    std::cout << "gfx_mesh_index_count=" << state.gfxHeader.meshIndexCount << "\n";
    std::cout << "gfx_submit_present_mask=" << state.gfxHeader.submitPresentMask << "\n";
    std::cout << "gfx_frame_token=" << state.gfxHeader.frameToken << "\n";
    std::cout << "gfx_render_pass_token=" << state.gfxHeader.renderPassToken << "\n";

    std::cout << "simulation_stream_loaded=1\n";
    std::cout << "simulation_frames_loaded=" << state.simulationFrames.size() << "\n";
    std::cout << "simulation_fixed_step_millis=" << SimulationFixedStepMillis << "\n";

    if (simulationSmokeMode) {
      const SimulationFrameState &frame = state.simulationFrames.front();
      std::cout << "simulation_smoke_tick=" << frame.tick << "\n";
      std::cout << "simulation_smoke_angle_milli=" << frame.angleMilli << "\n";
      std::cout << "simulation_smoke_axis_x_centi=" << frame.axisXCenti << "\n";
      std::cout << "simulation_smoke_axis_y_centi=" << frame.axisYCenti << "\n";
      return 0;
    }
  }

  PresenterConfig config;
  config.maxFrames = maxFrames;
  config.windowWidth = state.gfxMode ? static_cast<CGFloat>(state.gfxHeader.windowWidth) : 960.0;
  config.windowHeight = state.gfxMode ? static_cast<CGFloat>(state.gfxHeader.windowHeight) : 640.0;
  config.windowTitle = state.gfxMode ? @"PrimeStruct /std/gfx (Window Host)"
                                     : @"PrimeStruct Spinning Cube (Window Host)";
  config.activationPolicy = NSApplicationActivationPolicyRegular;
  config.gfxProfile = deducedGfxProfileName();

  PresenterCallbacks callbacks;
  callbacks.prepare = prepareWindowHostContent;
  callbacks.presenterReady = emitPresenterReadyDiagnostics;
  callbacks.render = renderWindowHostFrame;

  return primestruct::native_metal_window::runPresenter(config, &state, callbacks);
}
