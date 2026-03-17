#pragma once

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include "gfx_contract_shared.h"
#include "software_surface_bridge.h"

#include <iostream>
#include <string>

namespace primestruct::metal_offscreen_host {

using GfxErrorCode = gfx_contract::GfxErrorCode;

struct FailureInfo {
  int exitCode = 0;
  GfxErrorCode gfxErrorCode = GfxErrorCode::None;
  std::string why;
};

struct SoftwareSurfaceDemoConfig {
  const char *gfxProfile = "";
  int deviceFailureExitCode = 70;
  int queueFailureExitCode = 74;
  int uploadFailureExitCode = 75;
  int targetTextureFailureExitCode = 76;
  int commandCreationExitCode = 77;
  int commandCompletionExitCode = 78;
};

struct RenderConfig {
  const char *gfxProfile = "";
  const char *libraryPath = "";
  const char *vertexFunctionName = "";
  const char *fragmentFunctionName = "";
  NSUInteger targetWidth = 64;
  NSUInteger targetHeight = 64;
  double clearRed = 0.03;
  double clearGreen = 0.04;
  double clearBlue = 0.06;
  double clearAlpha = 1.0;
  int deviceFailureExitCode = 70;
  int libraryFailureExitCode = 71;
  int entrypointFailureExitCode = 72;
  int pipelineFailureExitCode = 73;
  int queueFailureExitCode = 74;
  int targetTextureFailureExitCode = 75;
  int prepareFailureExitCode = 76;
  int commandFailureExitCode = 77;
  int commandCompletionExitCode = 78;
};

using ConfigurePipelineFn = bool (*)(void *context,
                                     MTLRenderPipelineDescriptor *pipelineDesc,
                                     FailureInfo &failureOut);
using PrepareResourcesFn = bool (*)(void *context,
                                    id<MTLDevice> device,
                                    FailureInfo &failureOut);
using EncodeFrameFn = bool (*)(void *context,
                               id<MTLRenderPipelineState> pipeline,
                               id<MTLRenderCommandEncoder> encoder,
                               FailureInfo &failureOut);
using ShutdownFn = void (*)(void *context);

struct RenderCallbacks {
  ConfigurePipelineFn configurePipeline = nullptr;
  PrepareResourcesFn prepareResources = nullptr;
  EncodeFrameFn encodeFrame = nullptr;
  ShutdownFn shutdown = nullptr;
};

inline int emitGfxError(std::ostream &out, const char *gfxProfile, GfxErrorCode code, const std::string &why) {
  out << "gfx_profile=" << gfxProfile << "\n";
  out << "gfx_error_code=" << gfx_contract::gfxErrorCodeName(code) << "\n";
  out << "gfx_error_why=" << why << "\n";
  return 0;
}

inline bool uploadSoftwareSurfaceFrame(id<MTLDevice> device,
                                       const software_surface::SoftwareSurfaceFrame &frame,
                                       id<MTLTexture> __strong &textureOut,
                                       std::string &whyOut) {
  if (device == nil) {
    whyOut = "failed to create Metal device";
    return false;
  }
  if (!software_surface::validateFrame(frame, whyOut)) {
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

  const MTLRegion region =
      MTLRegionMake2D(0, 0, static_cast<NSUInteger>(frame.width), static_cast<NSUInteger>(frame.height));
  [textureOut replaceRegion:region
                mipmapLevel:0
                  withBytes:frame.pixelsBgra8.data()
                bytesPerRow:static_cast<NSUInteger>(frame.width * 4)];
  return true;
}

inline int runSoftwareSurfaceDemo(const SoftwareSurfaceDemoConfig &config) {
  @autoreleasepool {
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    if (device == nil) {
      emitGfxError(std::cerr, config.gfxProfile, GfxErrorCode::DeviceCreateFailed, "failed to create Metal device");
      return config.deviceFailureExitCode;
    }

    id<MTLCommandQueue> queue = [device newCommandQueue];
    if (queue == nil) {
      emitGfxError(std::cerr, config.gfxProfile, GfxErrorCode::DeviceCreateFailed, "failed to create command queue");
      return config.queueFailureExitCode;
    }

    const software_surface::SoftwareSurfaceFrame surfaceFrame = software_surface::makeDemoFrame();
    id<MTLTexture> softwareSurfaceTexture = nil;
    std::string softwareSurfaceWhy;
    if (!uploadSoftwareSurfaceFrame(device, surfaceFrame, softwareSurfaceTexture, softwareSurfaceWhy)) {
      emitGfxError(std::cerr, config.gfxProfile, GfxErrorCode::MeshCreateFailed, softwareSurfaceWhy);
      return config.uploadFailureExitCode;
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
      emitGfxError(std::cerr,
                   config.gfxProfile,
                   GfxErrorCode::FrameAcquireFailed,
                   "failed to create software surface target texture");
      return config.targetTextureFailureExitCode;
    }

    id<MTLCommandBuffer> commandBuffer = [queue commandBuffer];
    if (commandBuffer == nil) {
      emitGfxError(std::cerr, config.gfxProfile, GfxErrorCode::QueueSubmitFailed, "failed to create command buffer");
      return config.commandCreationExitCode;
    }
    id<MTLBlitCommandEncoder> blitEncoder = [commandBuffer blitCommandEncoder];
    if (blitEncoder == nil) {
      emitGfxError(std::cerr,
                   config.gfxProfile,
                   GfxErrorCode::QueueSubmitFailed,
                   "failed to create software surface blit encoder");
      return config.commandCreationExitCode;
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
      const std::string why =
          "software surface blit failed: " +
          std::string(commandBufferError == nil ? "unknown" : commandBufferError.localizedDescription.UTF8String);
      emitGfxError(std::cerr, config.gfxProfile, GfxErrorCode::QueueSubmitFailed, why);
      return config.commandCompletionExitCode;
    }

    std::cout << "gfx_profile=" << config.gfxProfile << "\n";
    std::cout << "software_surface_bridge=1\n";
    std::cout << "software_surface_width=" << surfaceFrame.width << "\n";
    std::cout << "software_surface_height=" << surfaceFrame.height << "\n";
    std::cout << "software_surface_presented=1\n";
    std::cout << "frame_rendered=1\n";
    return 0;
  }
}

inline int runLibraryRender(const RenderConfig &config, void *context, const RenderCallbacks &callbacks) {
  @autoreleasepool {
    const auto shutdown = [&]() {
      if (callbacks.shutdown != nullptr) {
        callbacks.shutdown(context);
      }
    };
    const auto emitFailure = [&](const FailureInfo &failure, int defaultExitCode, GfxErrorCode defaultCode, const char *defaultWhy) {
      const int exitCode = failure.exitCode > 0 ? failure.exitCode : defaultExitCode;
      const GfxErrorCode code = failure.gfxErrorCode == GfxErrorCode::None ? defaultCode : failure.gfxErrorCode;
      const std::string why = failure.why.empty() ? std::string(defaultWhy) : failure.why;
      emitGfxError(std::cerr, config.gfxProfile, code, why);
      shutdown();
      return exitCode;
    };

    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    if (device == nil) {
      emitGfxError(std::cerr, config.gfxProfile, GfxErrorCode::DeviceCreateFailed, "failed to create Metal device");
      shutdown();
      return config.deviceFailureExitCode;
    }

    NSError *error = nil;
    NSString *libraryPath = [NSString stringWithUTF8String:config.libraryPath];
    NSURL *libraryUrl = [NSURL fileURLWithPath:libraryPath];
    id<MTLLibrary> library = [device newLibraryWithURL:libraryUrl error:&error];
    if (library == nil) {
      const std::string why =
          "failed to load metallib: " + std::string(error == nil ? "unknown" : error.localizedDescription.UTF8String);
      emitGfxError(std::cerr, config.gfxProfile, GfxErrorCode::PipelineCreateFailed, why);
      shutdown();
      return config.libraryFailureExitCode;
    }

    id<MTLFunction> vertexFn = [library newFunctionWithName:[NSString stringWithUTF8String:config.vertexFunctionName]];
    id<MTLFunction> fragmentFn =
        [library newFunctionWithName:[NSString stringWithUTF8String:config.fragmentFunctionName]];
    if (vertexFn == nil || fragmentFn == nil) {
      emitGfxError(std::cerr, config.gfxProfile, GfxErrorCode::PipelineCreateFailed, "missing cube shader entrypoints");
      shutdown();
      return config.entrypointFailureExitCode;
    }

    MTLRenderPipelineDescriptor *pipelineDesc = [[MTLRenderPipelineDescriptor alloc] init];
    pipelineDesc.vertexFunction = vertexFn;
    pipelineDesc.fragmentFunction = fragmentFn;
    pipelineDesc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
    if (callbacks.configurePipeline != nullptr) {
      FailureInfo failure;
      if (!callbacks.configurePipeline(context, pipelineDesc, failure)) {
        return emitFailure(failure, config.pipelineFailureExitCode, GfxErrorCode::PipelineCreateFailed,
                           "failed to configure render pipeline");
      }
    }

    id<MTLRenderPipelineState> pipeline = [device newRenderPipelineStateWithDescriptor:pipelineDesc error:&error];
    if (pipeline == nil) {
      const std::string why =
          "failed to create render pipeline: " + std::string(error == nil ? "unknown" : error.localizedDescription.UTF8String);
      emitGfxError(std::cerr, config.gfxProfile, GfxErrorCode::PipelineCreateFailed, why);
      shutdown();
      return config.pipelineFailureExitCode;
    }

    id<MTLCommandQueue> queue = [device newCommandQueue];
    if (queue == nil) {
      emitGfxError(std::cerr, config.gfxProfile, GfxErrorCode::DeviceCreateFailed, "failed to create command queue");
      shutdown();
      return config.queueFailureExitCode;
    }

    if (callbacks.prepareResources != nullptr) {
      FailureInfo failure;
      if (!callbacks.prepareResources(context, device, failure)) {
        return emitFailure(failure, config.prepareFailureExitCode, GfxErrorCode::MeshCreateFailed,
                           "failed to prepare render resources");
      }
    }

    MTLTextureDescriptor *targetDesc =
        [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                           width:config.targetWidth
                                                          height:config.targetHeight
                                                       mipmapped:NO];
    targetDesc.storageMode = MTLStorageModePrivate;
    targetDesc.usage = MTLTextureUsageRenderTarget;

    id<MTLTexture> targetTexture = [device newTextureWithDescriptor:targetDesc];
    if (targetTexture == nil) {
      emitGfxError(std::cerr, config.gfxProfile, GfxErrorCode::FrameAcquireFailed, "failed to create render target texture");
      shutdown();
      return config.targetTextureFailureExitCode;
    }

    id<MTLCommandBuffer> commandBuffer = [queue commandBuffer];
    if (commandBuffer == nil) {
      emitGfxError(std::cerr, config.gfxProfile, GfxErrorCode::QueueSubmitFailed, "failed to create command buffer");
      shutdown();
      return config.commandFailureExitCode;
    }

    MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].texture = targetTexture;
    pass.colorAttachments[0].loadAction = MTLLoadActionClear;
    pass.colorAttachments[0].storeAction = MTLStoreActionStore;
    pass.colorAttachments[0].clearColor =
        MTLClearColorMake(config.clearRed, config.clearGreen, config.clearBlue, config.clearAlpha);

    id<MTLRenderCommandEncoder> encoder = [commandBuffer renderCommandEncoderWithDescriptor:pass];
    if (encoder == nil) {
      emitGfxError(std::cerr, config.gfxProfile, GfxErrorCode::QueueSubmitFailed, "failed to create render command encoder");
      shutdown();
      return config.commandFailureExitCode;
    }

    if (callbacks.encodeFrame != nullptr) {
      FailureInfo failure;
      if (!callbacks.encodeFrame(context, pipeline, encoder, failure)) {
        [encoder endEncoding];
        return emitFailure(failure, config.commandFailureExitCode, GfxErrorCode::QueueSubmitFailed,
                           "failed to encode render commands");
      }
    }
    [encoder endEncoding];

    [commandBuffer commit];
    [commandBuffer waitUntilCompleted];
    if (commandBuffer.status != MTLCommandBufferStatusCompleted) {
      NSError *commandBufferError = commandBuffer.error;
      const std::string why =
          "render command buffer failed: " + std::string(commandBufferError == nil ? "unknown"
                                                                                   : commandBufferError.localizedDescription.UTF8String);
      emitGfxError(std::cerr, config.gfxProfile, GfxErrorCode::QueueSubmitFailed, why);
      shutdown();
      return config.commandCompletionExitCode;
    }

    std::cout << "gfx_profile=" << config.gfxProfile << "\n";
    std::cout << "frame_rendered=1\n";
    shutdown();
    return 0;
  }
}

} // namespace primestruct::metal_offscreen_host
