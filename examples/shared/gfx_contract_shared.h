#pragma once

#include <cstddef>

namespace primestruct::gfx_contract {

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

inline const char *gfxErrorCodeName(GfxErrorCode code) {
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

struct VertexColoredHost {
  float px;
  float py;
  float pz;
  float pw;
  float r;
  float g;
  float b;
  float a;
};

static_assert(offsetof(VertexColoredHost, px) == 0);
static_assert(offsetof(VertexColoredHost, pw) == 12);
static_assert(offsetof(VertexColoredHost, r) == 16);
static_assert(sizeof(VertexColoredHost) == 32);
static_assert(alignof(VertexColoredHost) == 4);

} // namespace primestruct::gfx_contract
