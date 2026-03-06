#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <simd/simd.h>

#include <cstddef>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

struct Vertex {
  simd_float3 position;
  simd_float4 color;
};

constexpr Vertex TriangleVertices[3] = {
    {{-0.6f, -0.5f, 0.0f}, {0.95f, 0.20f, 0.20f, 1.0f}},
    {{0.6f, -0.5f, 0.0f}, {0.20f, 0.80f, 1.00f, 1.0f}},
    {{0.0f, 0.65f, 0.0f}, {0.20f, 1.00f, 0.50f, 1.0f}},
};

bool hasArgument(int argc, char **argv, const std::string &flag) {
  for (int i = 1; i < argc; ++i) {
    if (flag == argv[i]) {
      return true;
    }
  }
  return false;
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

  @autoreleasepool {
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    if (device == nil) {
      std::cerr << "failed to create Metal device\n";
      return 70;
    }

    NSError *error = nil;
    NSString *libraryPath = [NSString stringWithUTF8String:argv[1]];
    NSURL *libraryUrl = [NSURL fileURLWithPath:libraryPath];
    id<MTLLibrary> library = [device newLibraryWithURL:libraryUrl error:&error];
    if (library == nil) {
      std::cerr << "failed to load metallib: "
                << (error == nil ? "unknown" : error.localizedDescription.UTF8String) << "\n";
      return 71;
    }

    id<MTLFunction> vertexFn = [library newFunctionWithName:@"cubeVertexMain"];
    id<MTLFunction> fragmentFn = [library newFunctionWithName:@"cubeFragmentMain"];
    if (vertexFn == nil || fragmentFn == nil) {
      std::cerr << "missing cube shader entrypoints\n";
      return 72;
    }

    MTLRenderPipelineDescriptor *pipelineDesc = [[MTLRenderPipelineDescriptor alloc] init];
    pipelineDesc.vertexFunction = vertexFn;
    pipelineDesc.fragmentFunction = fragmentFn;
    pipelineDesc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
    MTLVertexDescriptor *vertexDesc = [[MTLVertexDescriptor alloc] init];
    vertexDesc.attributes[0].format = MTLVertexFormatFloat3;
    vertexDesc.attributes[0].offset = offsetof(Vertex, position);
    vertexDesc.attributes[0].bufferIndex = 0;
    vertexDesc.attributes[1].format = MTLVertexFormatFloat4;
    vertexDesc.attributes[1].offset = offsetof(Vertex, color);
    vertexDesc.attributes[1].bufferIndex = 0;
    vertexDesc.layouts[0].stride = sizeof(Vertex);
    vertexDesc.layouts[0].stepRate = 1;
    vertexDesc.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;
    pipelineDesc.vertexDescriptor = vertexDesc;

    id<MTLRenderPipelineState> pipeline =
        [device newRenderPipelineStateWithDescriptor:pipelineDesc error:&error];
    if (pipeline == nil) {
      std::cerr << "failed to create render pipeline: "
                << (error == nil ? "unknown" : error.localizedDescription.UTF8String) << "\n";
      return 73;
    }

    id<MTLCommandQueue> queue = [device newCommandQueue];
    if (queue == nil) {
      std::cerr << "failed to create command queue\n";
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
      std::cerr << "failed to create render target texture\n";
      return 75;
    }

    id<MTLBuffer> vertexBuffer =
        [device newBufferWithBytes:TriangleVertices
                            length:sizeof(TriangleVertices)
                           options:MTLResourceStorageModeShared];
    if (vertexBuffer == nil) {
      std::cerr << "failed to create vertex buffer\n";
      return 76;
    }

    MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].texture = target;
    pass.colorAttachments[0].loadAction = MTLLoadActionClear;
    pass.colorAttachments[0].storeAction = MTLStoreActionStore;
    pass.colorAttachments[0].clearColor = MTLClearColorMake(0.03, 0.04, 0.06, 1.0);

    id<MTLCommandBuffer> commandBuffer = [queue commandBuffer];
    id<MTLRenderCommandEncoder> encoder = [commandBuffer renderCommandEncoderWithDescriptor:pass];
    if (encoder == nil) {
      std::cerr << "failed to create render command encoder\n";
      return 77;
    }

    [encoder setRenderPipelineState:pipeline];
    [encoder setVertexBuffer:vertexBuffer offset:0 atIndex:0];
    [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
    [encoder endEncoding];

    [commandBuffer commit];
    [commandBuffer waitUntilCompleted];
    if (commandBuffer.status != MTLCommandBufferStatusCompleted) {
      std::cerr << "render command buffer failed\n";
      return 78;
    }

    std::cout << "frame_rendered=1\n";
    return 0;
  }
}
