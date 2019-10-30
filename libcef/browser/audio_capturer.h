// Copyright (c) 2019 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_AUDIO_CAPTURER_H_
#define CEF_LIBCEF_BROWSER_AUDIO_CAPTURER_H_
#pragma once

#include "include/cef_audio_handler.h"

#include "base/threading/thread_checker.h"
#include "media/base/audio_capturer_source.h"

namespace content {
class AudioLoopbackStreamCreator;
class WebContents;
}  // namespace content

namespace media {
class AudioInputDevice;
}  // namespace media

class CefBrowserHostImpl;

class CefAudioCapturer : public media::AudioCapturerSource::CaptureCallback {
 public:
  CefAudioCapturer(const CefAudioParameters& params,
                   CefRefPtr<CefBrowserHostImpl> browser,
                   CefRefPtr<CefAudioHandler> audio_handler);
  ~CefAudioCapturer() override;

  void Start();
  void Stop();

 private:
  void OnCaptureStarted() override;
  void Capture(const media::AudioBus* audio_source,
               base::TimeTicks audio_capture_time,
               double volume,
               bool key_pressed) override;
  void OnCaptureError(const std::string& /* message */) override {}
  void OnCaptureMuted(bool is_muted) override {}

  void handleCaptureStartedOnUIThread();
  void handleCaptureOnUIThread(std::unique_ptr<media::AudioBus> source,
                               base::TimeTicks audio_capture_time);

  media::AudioParameters params_;
  CefRefPtr<CefBrowserHostImpl> browser_;
  CefRefPtr<CefAudioHandler> audio_handler_;
  int audio_stream_id_;
  std::unique_ptr<content::AudioLoopbackStreamCreator> audio_stream_creator_;
  scoped_refptr<media::AudioInputDevice> audio_input_device_;

  bool stream_stopped_;
  base::ThreadChecker thread_checker_;

  static int audio_stream_id;
};

#endif  // CEF_LIBCEF_BROWSER_AUDIO_CAPTURER_H_