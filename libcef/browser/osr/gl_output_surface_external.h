// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_OSR_GL_OUTPUT_SURFACE_EXTERNAL_H_
#define CEF_OSR_GL_OUTPUT_SURFACE_EXTERNAL_H_

#include <memory>

#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/service/display_embedder/gl_output_surface.h"
#include "components/viz/service/display_embedder/viz_process_context_provider.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "ui/gfx/color_space.h"
#include "services/viz/privileged/mojom/compositing/external_renderer_updater.mojom.h"

namespace viz {

class ExternalImageData;

class VIZ_SERVICE_EXPORT GLOutputSurfaceExternal : public GLOutputSurface {
 public:
  explicit GLOutputSurfaceExternal(
      scoped_refptr<VizProcessContextProvider> context_provider,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
      mojom::ExternalRendererUpdaterPtr external_renderer_updater);
  ~GLOutputSurfaceExternal() override;

  // OutputSurface implementation.
  void EnsureBackbuffer() override;
  void DiscardBackbuffer() override;
  void BindFramebuffer() override;
  void Reshape(const gfx::Size& size,
               float scale_factor,
               const gfx::ColorSpace& color_space,
               bool alpha,
               bool stencil) override;
  void SwapBuffers(OutputSurfaceFrame frame) override;

 private:
  void OnSwapBuffersComplete(std::vector<ui::LatencyInfo> latency_info);
  void BindTexture();
  void UnbindTexture();
  void ResetBuffer();

  static const int kMaxSurfaces = 3;

  ExternalImageData *surfaces_[kMaxSurfaces] = { nullptr };
  uint32_t current_surface_ = 0;

  uint32_t fbo_ = 0;
  gfx::Size size_;
  gfx::ColorSpace color_space_;

  base::WeakPtrFactory<GLOutputSurfaceExternal> weak_ptr_factory_{this};

  gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager_;

  mojom::ExternalRendererUpdaterPtr external_renderer_updater_;

  DISALLOW_COPY_AND_ASSIGN(GLOutputSurfaceExternal);
};

}  // namespace viz

#endif  // CEF_OSR_GL_OUTPUT_SURFACE_OFFSCREEN_H_

