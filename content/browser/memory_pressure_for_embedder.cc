// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/memory_pressure_for_embedder.h"

#if BUILDFLAG(ENABLE_CUSTOM_BROWSER)
#include "content/browser/child_process_host_impl.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/common/child_process.mojom.h"
#include "content/public/browser/browser_child_process_host_iterator.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/process_type.h"
#endif

namespace content {

#if BUILDFLAG(ENABLE_CUSTOM_BROWSER)
void NotifyRendererMemoryPressure(RenderProcessHost* host,
                                  base::MemoryPressureLevel level) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!host) {
    return;
  }
  static_cast<RenderProcessHostImpl*>(host)->NotifyMemoryPressureForEmbedder(
      level);
}

int NotifyGpuMemoryPressure(base::MemoryPressureLevel level) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  int notified = 0;
  for (BrowserChildProcessHostIterator it(PROCESS_TYPE_GPU); !it.Done();
       ++it) {
    mojom::ChildProcess* child_process =
        static_cast<ChildProcessHostImpl*>(it.GetHost())->child_process();
    if (child_process) {
      child_process->OnMemoryPressure(level);
      ++notified;
    }
  }
  return notified;
}
#endif  // BUILDFLAG(ENABLE_CUSTOM_BROWSER)

}  // namespace content
