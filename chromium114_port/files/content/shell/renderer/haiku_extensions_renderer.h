// Copyright 2026 The RENKU project
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The renderer half of content/shell/browser/haiku_extensions.h: what
// ShellContentRendererClient forwards to the extension dispatcher. Defined in
// :haiku_extensions, for the same dependency reason.

#ifndef CONTENT_SHELL_RENDERER_HAIKU_EXTENSIONS_RENDERER_H_
#define CONTENT_SHELL_RENDERER_HAIKU_EXTENSIONS_RENDERER_H_

#include <stdint.h>

#include "v8/include/v8-forward.h"

class GURL;

namespace blink {
class WebServiceWorkerContextProxy;
}

namespace content {

class RenderFrame;

namespace haiku_extensions {

void RenderThreadStarted();
void RenderFrameCreated(RenderFrame* render_frame);
void RunScriptsAtDocumentStart(RenderFrame* render_frame);
void RunScriptsAtDocumentEnd(RenderFrame* render_frame);
void RunScriptsAtDocumentIdle(RenderFrame* render_frame);

void DidInitializeServiceWorkerContextOnWorkerThread(
    blink::WebServiceWorkerContextProxy* context_proxy,
    const GURL& service_worker_scope,
    const GURL& script_url);
void WillEvaluateServiceWorkerOnWorkerThread(
    blink::WebServiceWorkerContextProxy* context_proxy,
    v8::Local<v8::Context> v8_context,
    int64_t service_worker_version_id,
    const GURL& service_worker_scope,
    const GURL& script_url);
void DidStartServiceWorkerContextOnWorkerThread(
    int64_t service_worker_version_id,
    const GURL& service_worker_scope,
    const GURL& script_url);
void WillDestroyServiceWorkerContextOnWorkerThread(
    v8::Local<v8::Context> context,
    int64_t service_worker_version_id,
    const GURL& service_worker_scope,
    const GURL& script_url);

}  // namespace haiku_extensions
}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_HAIKU_EXTENSIONS_RENDERER_H_
