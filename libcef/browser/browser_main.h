// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_BROWSER_MAIN_H_
#define CEF_LIBCEF_BROWSER_BROWSER_MAIN_H_
#pragma once

#include "libcef/browser/request_context_impl.h"

#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "build/build_config.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_main_parts.h"

namespace content {
struct MainFunctionParams;
}

namespace extensions {
class ExtensionsBrowserClient;
class ExtensionsClient;
}  // namespace extensions

#if defined(USE_AURA)
namespace wm {
class WMState;
}
#endif

class CefDevToolsDelegate;

class CefBrowserMainParts : public content::BrowserMainParts {
 public:
  explicit CefBrowserMainParts(const content::MainFunctionParams& parameters);
  ~CefBrowserMainParts() override;

  int PreEarlyInitialization() override;
  void ToolkitInitialized() override;
  void PreMainMessageLoopStart() override;
  void PostMainMessageLoopStart() override;
  int PreCreateThreads() override;
  void PreMainMessageLoopRun() override;
  void PostMainMessageLoopRun() override;
  void PostDestroyThreads() override;

  CefRefPtr<CefRequestContextImpl> request_context() const {
    return global_request_context_;
  }
  CefDevToolsDelegate* devtools_delegate() const { return devtools_delegate_; }

  scoped_refptr<base::SingleThreadTaskRunner> background_task_runner() const {
    return background_task_runner_;
  }
  scoped_refptr<base::SingleThreadTaskRunner> user_visible_task_runner() const {
    return user_visible_task_runner_;
  }
  scoped_refptr<base::SingleThreadTaskRunner> user_blocking_task_runner()
      const {
    return user_blocking_task_runner_;
  }

 private:
#if defined(OS_WIN)
  void PlatformInitialize();
#endif  // defined(OS_WIN)

  CefRefPtr<CefRequestContextImpl> global_request_context_;
  CefDevToolsDelegate* devtools_delegate_;  // Deletes itself.

  std::unique_ptr<extensions::ExtensionsClient> extensions_client_;
  std::unique_ptr<extensions::ExtensionsBrowserClient>
      extensions_browser_client_;

  // Blocking task runners exposed via CefTaskRunner. For consistency with
  // previous named thread behavior always execute all pending tasks before
  // shutdown (e.g. to make sure critical data is saved to disk).
  // |background_task_runner_| is also passed to SQLitePersistentCookieStore.
  scoped_refptr<base::SingleThreadTaskRunner> background_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> user_visible_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> user_blocking_task_runner_;

#if defined(USE_AURA)
  std::unique_ptr<wm::WMState> wm_state_;
#endif

  DISALLOW_COPY_AND_ASSIGN(CefBrowserMainParts);
};

#endif  // CEF_LIBCEF_BROWSER_BROWSER_MAIN_H_
