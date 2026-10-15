// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_MEMORY_PRESSURE_FOR_EMBEDDER_H_
#define CONTENT_PUBLIC_BROWSER_MEMORY_PRESSURE_FOR_EMBEDDER_H_

#include "base/memory/memory_pressure_listener.h"
#include "content/common/content_export.h"
#include "content/public/common/buildflags.h"

namespace content {

class RenderProcessHost;

#if BUILDFLAG(ENABLE_CUSTOM_BROWSER)
// Lets the embedder ask one child process to release memory, without the
// browser-wide side effects of a simulated system memory-pressure signal.
// Both functions run on the UI thread and only notify the target process's
// memory-pressure listeners; nothing is terminated or frozen.

// Notifies the renderer `host`. `host` must be a real renderer host (not a
// test double). No-op while the process is not connected.
CONTENT_EXPORT void NotifyRendererMemoryPressure(
    RenderProcessHost* host,
    base::MemoryPressureLevel level);

// Notifies every running GPU process; returns how many were notified.
CONTENT_EXPORT int NotifyGpuMemoryPressure(base::MemoryPressureLevel level);
#endif  // BUILDFLAG(ENABLE_CUSTOM_BROWSER)

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_MEMORY_PRESSURE_FOR_EMBEDDER_H_
