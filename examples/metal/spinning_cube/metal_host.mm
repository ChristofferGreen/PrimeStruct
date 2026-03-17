#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <simd/simd.h>

#include "../../shared/gfx_contract_shared.h"
#include "../../shared/software_surface_bridge.h"

#include <cstddef>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

using GfxErrorCode = primestruct::gfx_contract::GfxErrorCode;
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

void emitGfxError(std::ostream &out, GfxErrorCode code, const std::string &why) {
  out << "gfx_profile=" << deducedGfxProfileName() << "\n";
  out << "gfx_error_code=" << primestruct::gfx_contract::gfxErrorCodeName(code) << "\n";
  out << "gfx_error_why=" << why << "\n";
}

bool uploadSoftwareSurfaceFrame(id<MTLDevice> device,
                                const primestruct::software_surface::SoftwareSurfaceFrame &frame,
                                id<MTLTexture> __strong &textureOut,
                                std::string &whyOut) {
  if (device == nil) {
    whyOut = "failed to create Metal device";
    return false;
  }
  if (!primestruct::software_surface::validateFrame(frame, whyOut)) {
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
    whyOut = "failed to create software surface texture";
    return false;
  }

  const MTLRegion region = MTLRegionMake2D(0, 0, static_cast<NSUInteger>(frame.width), static_cast<NSUInteger>(frame.height));
  [textureOut replaceRegion:region
                mipmapLevel:0
                  withBytes:frame.pixelsBgra8.data()
                bytesPerRow:static_cast<NSUInteger>(frame.width * 4)];
  return true;
}

int renderSoftwareSurfaceDemo() {
  @autoreleasepool {
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    if (device == nil) {
      emitGfxError(std::cerr, GfxErrorCode::DeviceCreateFailed, "failed to create Metal device");
      return 70;
    }

    id<MTLCommandQueue> queue = [device newCommandQueue];
    if (queue == nil) {
      emitGfxError(std::cerr, GfxErrorCode::DeviceCreateFailed, "failed to create command queue");
      return 74;
    }

    const primestruct::software_surface::SoftwareSurfaceFrame surfaceFrame =
        primestruct::software_surface::makeDemoFrame();
    id<MTLTexture> softwareSurfaceTexture = nil;
    std::string softwareSurfaceWhy;
    if (!uploadSoftwareSurfaceFrame(device, surfaceFrame, softwareSurfaceTexture, softwareSurfaceWhy)) {
      emitGfxError(std::cerr, GfxErrorCode::MeshCreateFailed, softwareSurfaceWhy);
      return 75;
    }

    MTLTextureDescriptor *targetDesc =
        [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                           width:static_cast<NSUInteger>(surfaceFrame.width)
                                                          height:static_cast<NSUInteger>(surfaceFrame.height)
                                                       mipmapped:NO];
    targetDesc.storageMode = MTLStorageModePrivate;
    targetDesc.usage = MTLTextureUsageRenderTarget;
    id<MTLTexture> targetTexture = [device newTextureWithDescriptor:targetDesc];
    if (targetTexture == nil) {
      emitGfxError(std::cerr, GfxErrorCode::FrameAcquireFailed, "failed to create software surface target texture");
      return 76;
    }

    id<MTLCommandBuffer> commandBuffer = [queue commandBuffer];
    if (commandBuffer == nil) {
      emitGfxError(std::cerr, GfxErrorCode::QueueSubmitFailed, "failed to create command buffer");
      return 77;
    }
    id<MTLBlitCommandEncoder> blitEncoder = [commandBuffer blitCommandEncoder];
    if (blitEncoder == nil) {
      emitGfxError(std::cerr, GfxErrorCode::QueueSubmitFailed, "failed to create software surface blit encoder");
      return 77;
    }
    [blitEncoder copyFromTexture:softwareSurfaceTexture
                     sourceSlice:0
                     sourceLevel:0
                    sourceOrigin:MTLOriginMake(0, 0, 0)
                      sourceSize:MTLSizeMake(static_cast<NSUInteger>(surfaceFrame.width),
                                             static_cast<NSUInteger>(surfaceFrame.height),
                                             1)
                       toTexture:targetTexture
                destinationSlice:0
                destinationLevel:0
               destinationOrigin:MTLOriginMake(0, 0, 0)];
    [blitEncoder endEncoding];

    [commandBuffer commit];
    [commandBuffer waitUntilCompleted];
    if (commandBuffer.status != MTLCommandBufferStatusCompleted) {
      NSError *commandBufferError = commandBuffer.error;
      const std::string why = "software surface blit failed: " +
                              std::string(commandBufferError == nil ? "unknown"
                                                                    : commandBufferError.localizedDescription.UTF8String);
      emitGfxError(std::cerr, GfxErrorCode::QueueSubmitFailed, why);
      return 78;
    }

    std::cout << "gfx_profile=" << deducedGfxProfileName() << "\n";
    std::cout << "software_surface_bridge=1\n";
    std::cout << "software_surface_width=" << surfaceFrame.width << "\n";
    std::cout << "software_surface_height=" << surfaceFrame.height << "\n";
    std::cout << "software_surface_presented=1\n";
    std::cout << "frame_rendered=1\n";
    return 0;
  }
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
    return renderSoftwareSurfaceDemo();
  }

  @autoreleasepool {
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    if (device == nil) {
      emitGfxError(std::cerr, GfxErrorCode::DeviceCreateFailed, "failed to create Metal device");
      return 70;
    }

    NSError *error = nil;
    NSString *libraryPath = [NSString stringWithUTF8String:argv[1]];
    NSURL *libraryUrl = [NSURL fileURLWithPath:libraryPath];
    id<MTLLibrary> library = [device newLibraryWithURL:libraryUrl error:&error];
    if (library == nil) {
      const std::string why =
          "failed to load metallib: " + std::string(error == nil ? "unknown" : error.localizedDescription.UTF8String);
      emitGfxError(std::cerr, GfxErrorCode::PipelineCreateFailed, why);
      return 71;
    }

    id<MTLFunction> vertexFn = [library newFunctionWithName:@"cubeVertexMain"];
    id<MTLFunction> fragmentFn = [library newFunctionWithName:@"cubeFragmentMain"];
    if (vertexFn == nil || fragmentFn == nil) {
      emitGfxError(std::cerr, GfxErrorCode::PipelineCreateFailed, "missing cube shader entrypoints");
      return 72;
    }

    MTLRenderPipelineDescriptor *pipelineDesc = [[MTLRenderPipelineDescriptor alloc] init];
    pipelineDesc.vertexFunction = vertexFn;
    pipelineDesc.fragmentFunction = fragmentFn;
    pipelineDesc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
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

    id<MTLRenderPipelineState> pipeline =
        [device newRenderPipelineStateWithDescriptor:pipelineDesc error:&error];
    if (pipeline == nil) {
      const std::string why = "failed to create render pipeline: " +
                              std::string(error == nil ? "unknown" : error.localizedDescription.UTF8String);
      emitGfxError(std::cerr, GfxErrorCode::PipelineCreateFailed, why);
      return 73;
    }

    id<MTLCommandQueue> queue = [device newCommandQueue];
    if (queue == nil) {
      emitGfxError(std::cerr, GfxErrorCode::DeviceCreateFailed, "failed to create command queue");
      return 74;
    }

    MTLTextureDescriptor *targetDesc =
        [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                           width:64
                                                          height:64
                                                       mipmapped:NO];
    targetDesc.storageMode = MTLStorageModePrivate;
    targetDesc.usage = MTLTextureUsageRenderTarget;

    id<MTLTexture> target = [device newTextureWithDescriptor:targetDesc];
    if (target == nil) {
      emitGfxError(std::cerr, GfxErrorCode::FrameAcquireFailed, "failed to create render target texture");
      return 75;
    }

    id<MTLBuffer> vertexBuffer =
        [device newBufferWithBytes:TriangleVertices
                            length:sizeof(TriangleVertices)
                           options:MTLResourceStorageModeShared];
    if (vertexBuffer == nil) {
      emitGfxError(std::cerr, GfxErrorCode::MeshCreateFailed, "failed to create vertex buffer");
      return 76;
    }

    MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].texture = target;
    pass.colorAttachments[0].loadAction = MTLLoadActionClear;
    pass.colorAttachments[0].storeAction = MTLStoreActionStore;
    pass.colorAttachments[0].clearColor = MTLClearColorMake(0.03, 0.04, 0.06, 1.0);

    id<MTLCommandBuffer> commandBuffer = [queue commandBuffer];
    if (commandBuffer == nil) {
      emitGfxError(std::cerr, GfxErrorCode::QueueSubmitFailed, "failed to create command buffer");
      return 77;
    }
    id<MTLRenderCommandEncoder> encoder = [commandBuffer renderCommandEncoderWithDescriptor:pass];
    if (encoder == nil) {
      emitGfxError(std::cerr, GfxErrorCode::QueueSubmitFailed, "failed to create render command encoder");
      return 77;
    }

    [encoder setRenderPipelineState:pipeline];
    [encoder setVertexBuffer:vertexBuffer offset:0 atIndex:0];
    [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
    [encoder endEncoding];

    [commandBuffer commit];
    [commandBuffer waitUntilCompleted];
    if (commandBuffer.status != MTLCommandBufferStatusCompleted) {
      NSError *commandBufferError = commandBuffer.error;
      const std::string why = "render command buffer failed: " +
                              std::string(commandBufferError == nil ? "unknown"
                                                                    : commandBufferError.localizedDescription.UTF8String);
      emitGfxError(std::cerr, GfxErrorCode::QueueSubmitFailed, why);
      return 78;
    }

    std::cout << "gfx_profile=" << deducedGfxProfileName() << "\n";
    std::cout << "frame_rendered=1\n";
    return 0;
  }
}
