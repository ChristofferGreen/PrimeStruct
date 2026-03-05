#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <simd/simd.h>

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

} // namespace

int main(int argc, char **argv) {
  if (hasArgument(argc, argv, "--help") || argc < 2) {
    std::cerr << "usage: metal_host <cube.metallib>\n";
    return argc < 2 ? 64 : 0;
  }

  @autoreleasepool {
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    if (device == nil) {
      std::cerr << "failed to create Metal device\n";
      return 70;
    }

    NSError *error = nil;
    NSString *libraryPath = [NSString stringWithUTF8String:argv[1]];
    id<MTLLibrary> library = [device newLibraryWithFile:libraryPath error:&error];
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
