// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef/browser/osr/gl_output_surface_external.h"

#include <stdint.h>

#include "base/bind.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/service/display/output_surface_client.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "ui/gl/gl_utils.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "services/viz/privileged/mojom/compositing/external_renderer_updater.mojom.h"

#ifdef OS_WIN
#include "ui/gl/gl_image_dxgi.h"
#endif

namespace viz {

class ExternalImageData {
  public:
   ExternalImageData(gpu::gles2::GLES2Interface *gl) :
     gl_(gl) {}
  ~ExternalImageData() {
    if (texture_id_) {
      gl_->DeleteTextures(1, &texture_id_);
    }

    if (image_id_) {
      gl_->DestroyImageCHROMIUM(image_id_);
    }

#ifdef OS_WIN
    glDXGIImage_.reset();
#endif
  }

  gfx::GpuMemoryBufferHandle Create(gfx::Size size, gfx::ColorSpace color_space,
                                    gpu::GpuMemoryBufferManager *manager) {
    size_ = size;
    color_space_ = color_space;
    buffer_ =
        manager->CreateGpuMemoryBuffer(
            size, gfx::BufferFormat::RGBA_8888, gfx::BufferUsage::SCANOUT, gpu::kNullSurfaceHandle);
    if (!buffer_) {
      return gfx::GpuMemoryBufferHandle();
    }
    buffer_->SetColorSpace(color_space);

    image_id_ = gl_->CreateImageCHROMIUM(buffer_->AsClientBuffer(), size.width(), size.height(),
                                         GL_RGBA);
    if (!image_id_) {
      buffer_ = 0;
      return gfx::GpuMemoryBufferHandle();
    }

    gl_->GenTextures(1, &texture_id_);
    return buffer_->CloneHandle();
  }

  void BindTexture(uint32_t fbo) {
    if (!texture_id_ || !image_id_ || bound_) {
      return;
    }

    gl_->BindTexture(GL_TEXTURE_2D, texture_id_);
    gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl_->BindTexImage2DCHROMIUM(GL_TEXTURE_2D, image_id_);
    gl_->SetColorSpaceMetadataCHROMIUM(
      texture_id_, reinterpret_cast<GLColorSpace>(&color_space_));
    gl_->BindTexture(GL_TEXTURE_2D, 0);

    gl_->BindFramebuffer(GL_FRAMEBUFFER, fbo);
    gl_->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                             GL_TEXTURE_2D, texture_id_, 0);

    bound_ = true;
  }

  void UnbindTexture(uint32_t fbo) {
    if (!texture_id_ || !image_id_ || !bound_)
      return;

    gl_->BindFramebuffer(GL_FRAMEBUFFER, fbo);
    gl_->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                             GL_TEXTURE_2D, 0, 0);

    gl_->BindTexture(GL_TEXTURE_2D, texture_id_);
    gl_->ReleaseTexImage2DCHROMIUM(GL_TEXTURE_2D, image_id_);
    gl_->BindTexture(GL_TEXTURE_2D, 0);

    gl_->BindFramebuffer(GL_FRAMEBUFFER, 0);

    gl_->Flush();
    bound_ = false;
  }

  void ResetBuffer() {
#ifdef OS_WIN
    if (!glDXGIImage_) {
      base::win::ScopedHandle handle;
      handle.Set(buffer_->CloneHandle().dxgi_handle.GetHandle());
      glDXGIImage_ = base::MakeRefCounted<gl::GLImageDXGI>(size_, nullptr);
      glDXGIImage_->InitializeHandle(std::move(handle), 0, gfx::BufferFormat::RGBA_8888);
    }

    // cycle the mutex back to 0, in case the client never picks up this frame
    // the client will always use acquire 0/release 0
    glDXGIImage_->keyed_mutex()->AcquireSync(1, 5);
    glDXGIImage_->keyed_mutex()->ReleaseSync(0);
#endif
  }

  gpu::gles2::GLES2Interface *gl_;

  gfx::Size size_;
  gfx::ColorSpace color_space_;
  uint32_t texture_id_ = 0;
  uint32_t image_id_ = 0;
  bool bound_ = false;
  std::unique_ptr<gfx::GpuMemoryBuffer> buffer_;
#ifdef OS_WIN
  scoped_refptr<gl::GLImageDXGI> glDXGIImage_;
#endif
};

GLOutputSurfaceExternal::GLOutputSurfaceExternal(
    scoped_refptr<VizProcessContextProvider> context_provider,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    mojom::ExternalRendererUpdaterPtr external_renderer_updater)
    : GLOutputSurface(context_provider, gpu::kNullSurfaceHandle),
    gpu_memory_buffer_manager_(gpu_memory_buffer_manager),
    external_renderer_updater_(std::move(external_renderer_updater)) {}

GLOutputSurfaceExternal::~GLOutputSurfaceExternal() {
  DiscardBackbuffer();
}

void GLOutputSurfaceExternal::EnsureBackbuffer() {
  if (size_.IsEmpty())
    return;

  if (!surfaces_[0]) {
    gpu::gles2::GLES2Interface* gl = context_provider_->ContextGL();

    const int max_texture_size =
        context_provider_->ContextCapabilities().max_texture_size;
    gfx::Size texture_size(std::min(size_.width(), max_texture_size),
                           std::min(size_.height(), max_texture_size));

    for (int i = 0; i < kMaxSurfaces; i++) {
        surfaces_[i] = new ExternalImageData(gl);

        gfx::GpuMemoryBufferHandle handle =
            surfaces_[i]->Create(texture_size, color_space_, gpu_memory_buffer_manager_);
        if (handle.type != gfx::GpuMemoryBufferType::EMPTY_BUFFER) {
            external_renderer_updater_->OnGpuBufferAllocated(std::move(handle), i);
        }
    }

    gl->GenFramebuffers(1, &fbo_);
    current_surface_ = 0;
  }
  surfaces_[current_surface_]->BindTexture(fbo_);
}

void GLOutputSurfaceExternal::DiscardBackbuffer() {
  external_renderer_updater_->OnGpuBufferFreed();

  if (surfaces_[0]) {
    for (int i = 0; i < kMaxSurfaces; i++) {
      surfaces_[i]->UnbindTexture(fbo_);
    }
  }

  gpu::gles2::GLES2Interface* gl = context_provider_->ContextGL();

  if (fbo_) {
    gl->BindFramebuffer(GL_FRAMEBUFFER, fbo_);
    gl->DeleteFramebuffers(1, &fbo_);
    fbo_ = 0;
  }

  if (surfaces_[0]) {
    for (int i = 0; i < kMaxSurfaces; i++) {
      delete surfaces_[i];
      surfaces_[i] = nullptr;
    }
  }

  current_surface_ = 0;

  gl->Flush();
}

void GLOutputSurfaceExternal::BindFramebuffer() {
  if (!surfaces_[current_surface_]) {
    EnsureBackbuffer();
  } else {
    surfaces_[current_surface_]->BindTexture(fbo_);
  }
}

void GLOutputSurfaceExternal::Reshape(const gfx::Size& size,
                                       float scale_factor,
                                       const gfx::ColorSpace& color_space,
                                       bool alpha,
                                       bool stencil) {
  size_ = size;
  color_space_ = color_space;
  DiscardBackbuffer();
  current_surface_ = 0;
}

void GLOutputSurfaceExternal::SwapBuffers(OutputSurfaceFrame frame) {
  DCHECK(frame.size == size_);

  gpu::gles2::GLES2Interface* gl = context_provider_->ContextGL();

  gl->Flush();

  surfaces_[current_surface_]->UnbindTexture(fbo_);

  gpu::SyncToken sync_token;
  gl->GenUnverifiedSyncTokenCHROMIUM(sync_token.GetData());
  context_provider_->ContextSupport()->SignalSyncToken(
      sync_token,
      base::BindOnce(&GLOutputSurfaceExternal::OnSwapBuffersComplete,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(frame.latency_info)));
}

void GLOutputSurfaceExternal::OnSwapBuffersComplete(
    std::vector<ui::LatencyInfo> latency_info) {

  if (!surfaces_[current_surface_]) {
    LOG(ERROR) << "something went wrong";
  }
  surfaces_[current_surface_]->ResetBuffer();
  external_renderer_updater_->OnAfterFlip(current_surface_, gfx::Rect(size_));

  current_surface_++;
  if (current_surface_ >= kMaxSurfaces)
      current_surface_ = 0;

  latency_tracker()->OnGpuSwapBuffersCompleted(latency_info);
  // Swap timings are not available since for offscreen there is no Swap, just a
  // SignalSyncToken. We use base::TimeTicks::Now() as an overestimate.
  auto now = base::TimeTicks::Now();
  client()->DidReceiveSwapBuffersAck({.swap_start = now});
  client()->DidReceivePresentationFeedback(gfx::PresentationFeedback(
      now, base::TimeDelta::FromMilliseconds(16), /*flags=*/0));

  if (needs_swap_size_notifications())
    client()->DidSwapWithSize(size_);
}

}  // namespace viz