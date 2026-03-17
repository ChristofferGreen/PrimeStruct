#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <simd/simd.h>

#include "../../shared/gfx_contract_shared.h"
#include "../../shared/metal_offscreen_host.h"
#include "../../shared/software_surface_bridge.h"

#include <cstddef>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

using GfxErrorCode = primestruct::gfx_contract::GfxErrorCode;
using FailureInfo = primestruct::metal_offscreen_host::FailureInfo;
using RenderCallbacks = primestruct::metal_offscreen_host::RenderCallbacks;
using RenderConfig = primestruct::metal_offscreen_host::RenderConfig;
using SoftwareSurfaceDemoConfig = primestruct::metal_offscreen_host::SoftwareSurfaceDemoConfig;
using Vertex = primestruct::gfx_contract::VertexColoredHost;

constexpr Vertex TriangleVertices[3] = {
    {-0.6f, -0.5f, 0.0f, 1.0f, 0.95f, 0.20f, 0.20f, 1.0f},
    {0.6f, -0.5f, 0.0f, 1.0f, 0.20f, 0.80f, 1.00f, 1.0f},
    {0.0f, 0.65f, 0.0f, 1.0f, 0.20f, 1.00f, 0.50f, 1.0f},
};

bool hasArgument(int argc, char **argv, const std::string &flag) {
  for (int i = 1; i < argc; ++i) {
    if (flag == argv[i]) {
      return true;
    }
  }
  return false;
}

const char *deducedGfxProfileName() {
  return "metal-osx";
}

struct CubeSimulationState {
  int tick = 0;
  float angleRadians = 0.0f;
  float angularVelocity = 1.0471976f;
  float axisX = 1.0f;
  float axisY = 0.0f;
};

struct CubeStepResult {
  CubeSimulationState state;
  float remainingSeconds = 0.0f;
  int stepCount = 0;
};

float wrapAngle(float angle) {
  constexpr float Tau = 6.2831853f;
  float wrapped = angle;
  while (wrapped >= Tau) {
    wrapped -= Tau;
  }
  while (wrapped < 0.0f) {
    wrapped += Tau;
  }
  return wrapped;
}

CubeSimulationState cubeTick(const CubeSimulationState &state, float deltaSeconds) {
  CubeSimulationState next = state;
  next.tick += 1;
  next.angleRadians = wrapAngle(state.angleRadians + state.angularVelocity * deltaSeconds);
  next.axisX = state.axisX * 0.9998477f + state.axisY * -0.0174524f;
  next.axisY = state.axisX * 0.0174524f + state.axisY * 0.9998477f;
  return next;
}

CubeStepResult cubeAdvanceFixed(const CubeSimulationState &state,
                                float accumulatedSeconds,
                                float fixedDeltaSeconds,
                                int maxSteps) {
  CubeStepResult result;
  result.state = state;
  result.remainingSeconds = accumulatedSeconds;
  if (fixedDeltaSeconds <= 0.0f || maxSteps < 1) {
    return result;
  }
  while (result.remainingSeconds >= fixedDeltaSeconds) {
    if (result.stepCount >= maxSteps) {
      return result;
    }
    result.state = cubeTick(result.state, fixedDeltaSeconds);
    result.remainingSeconds -= fixedDeltaSeconds;
    result.stepCount += 1;
  }
  return result;
}

CubeSimulationState cubeRunFixedTicks(int ticks) {
  CubeSimulationState state;
  constexpr float FixedDelta = 0.016666667f;
  for (int i = 0; i < ticks; ++i) {
    const CubeStepResult step = cubeAdvanceFixed(state, FixedDelta, FixedDelta, 1);
    state = step.state;
  }
  return state;
}

CubeSimulationState cubeRunFixedTicksChunked(int ticks) {
  CubeSimulationState state;
  constexpr float FixedDelta = 0.016666667f;
  float accumulator = 0.0f;
  for (int i = 0; i < ticks; ++i) {
    accumulator += 0.010000000f;
    const CubeStepResult stepA = cubeAdvanceFixed(state, accumulator, FixedDelta, 4);
    state = stepA.state;
    accumulator = stepA.remainingSeconds;

    accumulator += 0.006666667f;
    const CubeStepResult stepB = cubeAdvanceFixed(state, accumulator, FixedDelta, 4);
    state = stepB.state;
    accumulator = stepB.remainingSeconds;
  }
  return state;
}

int cubeFixedStepSnapshotCode(const CubeSimulationState &state) {
  const int tickBucket = static_cast<int>(static_cast<float>(state.tick) * 0.1f);
  const int angleDeci = static_cast<int>(state.angleRadians * 10.0f);
  const int axisXShiftedDeci = static_cast<int>((state.axisX + 1.0f) * 10.0f);
  const int axisYDeci = static_cast<int>(state.axisY * 10.0f);
  return tickBucket + angleDeci + axisXShiftedDeci + axisYDeci;
}

bool parseIntArg(const char *text, int &value) {
  if (text == nullptr) {
    return false;
  }
  char *end = nullptr;
  const long parsed = std::strtol(text, &end, 10);
  if (end == text || *end != '\0') {
    return false;
  }
  value = static_cast<int>(parsed);
  return true;
}

struct MetalHostState {
  id<MTLBuffer> vertexBuffer = nil;
};

bool configureTrianglePipeline(void *context,
                               MTLRenderPipelineDescriptor *pipelineDesc,
                               FailureInfo &failureOut) {
  (void)context;
  (void)failureOut;

  MTLVertexDescriptor *vertexDesc = [[MTLVertexDescriptor alloc] init];
  vertexDesc.attributes[0].format = MTLVertexFormatFloat4;
  vertexDesc.attributes[0].offset = offsetof(Vertex, px);
  vertexDesc.attributes[0].bufferIndex = 0;
  vertexDesc.attributes[1].format = MTLVertexFormatFloat4;
  vertexDesc.attributes[1].offset = offsetof(Vertex, r);
  vertexDesc.attributes[1].bufferIndex = 0;
  vertexDesc.layouts[0].stride = sizeof(Vertex);
  vertexDesc.layouts[0].stepRate = 1;
  vertexDesc.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;
  pipelineDesc.vertexDescriptor = vertexDesc;
  return true;
}

bool prepareTriangleResources(void *context, id<MTLDevice> device, FailureInfo &failureOut) {
  auto &state = *reinterpret_cast<MetalHostState *>(context);
  state.vertexBuffer =
      [device newBufferWithBytes:TriangleVertices
                          length:sizeof(TriangleVertices)
                         options:MTLResourceStorageModeShared];
  if (state.vertexBuffer == nil) {
    failureOut.exitCode = 76;
    failureOut.gfxErrorCode = GfxErrorCode::MeshCreateFailed;
    failureOut.why = "failed to create vertex buffer";
    return false;
  }
  return true;
}

bool encodeTriangleFrame(void *context,
                         id<MTLRenderPipelineState> pipeline,
                         id<MTLRenderCommandEncoder> encoder,
                         FailureInfo &failureOut) {
  auto &state = *reinterpret_cast<MetalHostState *>(context);
  if (state.vertexBuffer == nil) {
    failureOut.exitCode = 76;
    failureOut.gfxErrorCode = GfxErrorCode::MeshCreateFailed;
    failureOut.why = "vertex buffer was unavailable";
    return false;
  }

  [encoder setRenderPipelineState:pipeline];
  [encoder setVertexBuffer:state.vertexBuffer offset:0 atIndex:0];
  [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
  return true;
}

} // namespace

int main(int argc, char **argv) {
  if (hasArgument(argc, argv, "--help") || argc < 2) {
    std::cerr << "usage: metal_host <cube.metallib>\n";
    std::cerr << "       metal_host --snapshot-code <ticks>\n";
    std::cerr << "       metal_host --parity-check <ticks>\n";
    std::cerr << "       metal_host --software-surface-demo\n";
    return argc < 2 ? 64 : 0;
  }

  if (std::string(argv[1]) == "--snapshot-code") {
    int ticks = 0;
    if (argc < 3 || !parseIntArg(argv[2], ticks)) {
      std::cerr << "invalid ticks value for --snapshot-code\n";
      return 65;
    }
    std::cout << "snapshot_code=" << cubeFixedStepSnapshotCode(cubeRunFixedTicks(ticks)) << "\n";
    return 0;
  }

  if (std::string(argv[1]) == "--parity-check") {
    int ticks = 0;
    if (argc < 3 || !parseIntArg(argv[2], ticks)) {
      std::cerr << "invalid ticks value for --parity-check\n";
      return 66;
    }
    const CubeSimulationState fixed = cubeRunFixedTicks(ticks);
    const CubeSimulationState chunked = cubeRunFixedTicksChunked(ticks);
    const bool ok = std::fabs(fixed.angleRadians - chunked.angleRadians) <= 0.02f &&
                    std::fabs(fixed.axisX - chunked.axisX) <= 0.02f &&
                    std::fabs(fixed.axisY - chunked.axisY) <= 0.02f;
    std::cout << "parity_ok=" << (ok ? 1 : 0) << "\n";
    return ok ? 0 : 1;
  }

  if (std::string(argv[1]) == "--software-surface-demo") {
    SoftwareSurfaceDemoConfig config;
    config.gfxProfile = deducedGfxProfileName();
    return primestruct::metal_offscreen_host::runSoftwareSurfaceDemo(config);
  }

  MetalHostState state;
  RenderConfig config;
  config.gfxProfile = deducedGfxProfileName();
  config.libraryPath = argv[1];
  config.vertexFunctionName = "cubeVertexMain";
  config.fragmentFunctionName = "cubeFragmentMain";

  RenderCallbacks callbacks;
  callbacks.configurePipeline = configureTrianglePipeline;
  callbacks.prepareResources = prepareTriangleResources;
  callbacks.encodeFrame = encodeTriangleFrame;
  return primestruct::metal_offscreen_host::runLibraryRender(config, &state, callbacks);
}
