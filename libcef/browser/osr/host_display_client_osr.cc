// Copyright 2019 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/osr/host_display_client_osr.h"

#include <utility>

#include "libcef/browser/browser_host_impl.h"
#include "libcef/browser/osr/render_widget_host_view_osr.h"

#include "base/memory/shared_memory.h"
#include "components/viz/common/resources/resource_format.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "services/viz/privileged/mojom/compositing/layered_window_updater.mojom.h"
#include "services/viz/privileged/mojom/compositing/external_renderer_updater.mojom.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/src/core/SkDevice.h"
#include "ui/gfx/skia_util.h"

#if defined(OS_WIN)
#include "skia/ext/skia_utils_win.h"
#endif

class CefExternalRendererUpdaterOSR : public viz::mojom::ExternalRendererUpdater {
 public:
  CefExternalRendererUpdaterOSR(CefRenderWidgetHostViewOSR* const view,
                                viz::mojom::ExternalRendererUpdaterRequest request);
  ~CefExternalRendererUpdaterOSR() override;

  // viz::mojom::ExternalRendererUpdater implementation.
  void OnGpuBufferAllocated(gfx::GpuMemoryBufferHandle handle, uint32_t id) override;
  void OnGpuBufferFreed(OnGpuBufferFreedCallback callback) override;
  void OnAfterFlip(uint32_t id, const gfx::Rect& damage_rect) override;

 private:
  static const int kMaxSurfaces = 3;

  CefRenderWidgetHostViewOSR* const view_;
  mojo::Binding<viz::mojom::ExternalRendererUpdater> binding_;
#ifdef OS_WIN
  base::win::ScopedHandle textureHandle_[kMaxSurfaces];
#endif

  DISALLOW_COPY_AND_ASSIGN(CefExternalRendererUpdaterOSR);
};

CefExternalRendererUpdaterOSR::CefExternalRendererUpdaterOSR(
    CefRenderWidgetHostViewOSR* const view,
    viz::mojom::ExternalRendererUpdaterRequest request)
    : view_(view), binding_(this, std::move(request)) {}

CefExternalRendererUpdaterOSR::~CefExternalRendererUpdaterOSR() = default;

void CefExternalRendererUpdaterOSR::OnGpuBufferAllocated(gfx::GpuMemoryBufferHandle buffer, uint32_t id) {
#ifdef OS_WIN
    textureHandle_[id].Set(buffer.dxgi_handle.GetHandle());
#endif
}

void CefExternalRendererUpdaterOSR::OnGpuBufferFreed(OnGpuBufferFreedCallback callback) {
#ifdef OS_WIN
    for (int i = 0; i < kMaxSurfaces; i++)
        textureHandle_[i].Set(nullptr);
#endif
    std::move(callback).Run();
}

void CefExternalRendererUpdaterOSR::OnAfterFlip(uint32_t id, const gfx::Rect& damage_rect) {
#ifdef OS_WIN
    view_->OnAcceleratedPaint(damage_rect, textureHandle_[id].Get());
#else
    view_->OnAcceleratedPaint(damage_Rect, nullptr);
#endif
}

class CefLayeredWindowUpdaterOSR : public viz::mojom::LayeredWindowUpdater {
 public:
  CefLayeredWindowUpdaterOSR(CefRenderWidgetHostViewOSR* const view,
                             viz::mojom::LayeredWindowUpdaterRequest request);
  ~CefLayeredWindowUpdaterOSR() override;

  void SetActive(bool active);
  const void* GetPixelMemory() const;
  gfx::Size GetPixelSize() const;

  // viz::mojom::LayeredWindowUpdater implementation.
  void OnAllocatedSharedMemory(const gfx::Size& pixel_size,
                               base::UnsafeSharedMemoryRegion region) override;
  void Draw(const gfx::Rect& damage_rect, DrawCallback draw_callback) override;

 private:
  CefRenderWidgetHostViewOSR* const view_;
  mojo::Binding<viz::mojom::LayeredWindowUpdater> binding_;
  bool active_ = false;
  base::WritableSharedMemoryMapping shared_memory_;
  gfx::Size pixel_size_;

  DISALLOW_COPY_AND_ASSIGN(CefLayeredWindowUpdaterOSR);
};

CefLayeredWindowUpdaterOSR::CefLayeredWindowUpdaterOSR(
    CefRenderWidgetHostViewOSR* const view,
    viz::mojom::LayeredWindowUpdaterRequest request)
    : view_(view), binding_(this, std::move(request)) {}

CefLayeredWindowUpdaterOSR::~CefLayeredWindowUpdaterOSR() = default;

void CefLayeredWindowUpdaterOSR::SetActive(bool active) {
  active_ = active;
}

const void* CefLayeredWindowUpdaterOSR::GetPixelMemory() const {
  return shared_memory_.memory();
}

gfx::Size CefLayeredWindowUpdaterOSR::GetPixelSize() const {
  return pixel_size_;
}

void CefLayeredWindowUpdaterOSR::OnAllocatedSharedMemory(
    const gfx::Size& pixel_size,
    base::UnsafeSharedMemoryRegion region) {
  // Make sure |pixel_size| is sane.
  size_t expected_bytes;
  bool size_result = viz::ResourceSizes::MaybeSizeInBytes(
      pixel_size, viz::ResourceFormat::RGBA_8888, &expected_bytes);
  if (!size_result)
    return;

  pixel_size_ = pixel_size;
  shared_memory_ = region.Map();
  DCHECK(shared_memory_.IsValid());
}

void CefLayeredWindowUpdaterOSR::Draw(const gfx::Rect& damage_rect,
                                      DrawCallback draw_callback) {
  if (active_) {
    const void* memory = GetPixelMemory();
    if (memory) {
      view_->OnPaint(damage_rect, pixel_size_, memory);
    } else {
      LOG(WARNING) << "Failed to read pixels";
    }
  }

  std::move(draw_callback).Run();
}

CefHostDisplayClientOSR::CefHostDisplayClientOSR(
    CefRenderWidgetHostViewOSR* const view,
    gfx::AcceleratedWidget widget,
    bool use_proxy_output)
    : viz::HostDisplayClient(widget), view_(view), use_proxy_output_(use_proxy_output) {}

CefHostDisplayClientOSR::~CefHostDisplayClientOSR() {}

void CefHostDisplayClientOSR::SetActive(bool active) {
  active_ = active;
  if (layered_window_updater_) {
    layered_window_updater_->SetActive(active_);
  }
}

const void* CefHostDisplayClientOSR::GetPixelMemory() const {
  return layered_window_updater_ ? layered_window_updater_->GetPixelMemory()
                                 : nullptr;
}

gfx::Size CefHostDisplayClientOSR::GetPixelSize() const {
  return layered_window_updater_ ? layered_window_updater_->GetPixelSize()
                                 : gfx::Size{};
}

void CefHostDisplayClientOSR::UseProxyOutputDevice(
    UseProxyOutputDeviceCallback callback) {
  std::move(callback).Run(use_proxy_output_);
}

void CefHostDisplayClientOSR::CreateLayeredWindowUpdater(
    viz::mojom::LayeredWindowUpdaterRequest request) {
  layered_window_updater_ =
      std::make_unique<CefLayeredWindowUpdaterOSR>(view_, std::move(request));
  layered_window_updater_->SetActive(active_);
}

void CefHostDisplayClientOSR::CreateExternalRendererUpdater(
    viz::mojom::ExternalRendererUpdaterRequest request) {
  external_renderer_updater_ =
      std::make_unique<CefExternalRendererUpdaterOSR>(view_, std::move(request));
}