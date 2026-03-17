#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <simd/simd.h>

#include "../../shared/gfx_contract_shared.h"
#include "../../shared/metal_offscreen_host.h"
#include "../../shared/spinning_cube_simulation_reference.h"
#include "../../shared/software_surface_bridge.h"

#include <cstddef>
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
    std::cout << "snapshot_code=" << primestruct::spinning_cube_simulation::snapshotCodeForTicks(ticks) << "\n";
    return 0;
  }

  if (std::string(argv[1]) == "--parity-check") {
    int ticks = 0;
    if (argc < 3 || !parseIntArg(argv[2], ticks)) {
      std::cerr << "invalid ticks value for --parity-check\n";
      return 66;
    }
    const bool ok = primestruct::spinning_cube_simulation::parityOk(ticks);
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
