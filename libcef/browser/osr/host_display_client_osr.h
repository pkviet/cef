// Copyright 2019 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_OSR_HOST_DISPLAY_CLIENT_OSR_H_
#define CEF_LIBCEF_BROWSER_OSR_HOST_DISPLAY_CLIENT_OSR_H_

#include <memory>

#include "base/callback.h"
#include "base/memory/shared_memory.h"
#include "components/viz/host/host_display_client.h"
#include "ui/gfx/native_widget_types.h"

class CefLayeredWindowUpdaterOSR;
class CefRenderWidgetHostViewOSR;
class CefExternalRendererUpdaterOSR;

class CefHostDisplayClientOSR : public viz::HostDisplayClient {
 public:
  CefHostDisplayClientOSR(CefRenderWidgetHostViewOSR* const view,
                          gfx::AcceleratedWidget widget,
                          bool use_proxy_output);
  ~CefHostDisplayClientOSR() override;

  void SetActive(bool active);
  const void* GetPixelMemory() const;
  gfx::Size GetPixelSize() const;

 private:
  // mojom::DisplayClient implementation.
  void UseProxyOutputDevice(UseProxyOutputDeviceCallback callback) override;

  void CreateLayeredWindowUpdater(
      viz::mojom::LayeredWindowUpdaterRequest request) override;
  void CreateExternalRendererUpdater(
      viz::mojom::ExternalRendererUpdaterRequest request) override;

  CefRenderWidgetHostViewOSR* const view_;
  std::unique_ptr<CefLayeredWindowUpdaterOSR> layered_window_updater_;
  std::unique_ptr<CefExternalRendererUpdaterOSR> external_renderer_updater_;
  bool active_ = false;
  bool use_proxy_output_ = true;

  DISALLOW_COPY_AND_ASSIGN(CefHostDisplayClientOSR);
};

#endif  // CEF_LIBCEF_BROWSER_OSR_HOST_DISPLAY_CLIENT_OSR_H_
