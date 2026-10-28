// Copyright 2026 The RENKU project
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/haiku_extensions_renderer.h"

#include <memory>

#include "content/public/renderer/render_thread.h"
#include "content/shell/common/extensions/shell_extensions_api_provider.h"
#include "extensions/common/constants.h"
#include "extensions/common/extensions_client.h"
#include "extensions/renderer/dispatcher.h"
#include "extensions/renderer/dispatcher_delegate.h"
#include "extensions/renderer/extension_frame_helper.h"
#include "extensions/renderer/extensions_renderer_client.h"
#include "extensions/shell/common/shell_extensions_client.h"
#include "url/gurl.h"

namespace content {
namespace haiku_extensions {

namespace {

// app_shell's renderer client, except that an extension's service worker gets
// the extension APIs: a Manifest V3 extension's background is one, and
// app_shell (made for Chrome Apps) answers no.
class RendererClient : public extensions::ExtensionsRendererClient {
 public:
  RendererClient()
      : dispatcher_(std::make_unique<extensions::Dispatcher>(
            std::make_unique<extensions::DispatcherDelegate>())) {
    dispatcher_->OnRenderThreadStarted(RenderThread::Get());
  }

  bool IsIncognitoProcess() const override { return false; }
  int GetLowestIsolatedWorldId() const override { return 1; }
  extensions::Dispatcher* GetDispatcher() override { return dispatcher_.get(); }
  bool ExtensionAPIEnabledForServiceWorkerScript(
      const GURL& scope,
      const GURL& script_url) const override {
    return script_url.SchemeIs(extensions::kExtensionScheme);
  }

 private:
  std::unique_ptr<extensions::Dispatcher> dispatcher_;
};

struct State {
  std::unique_ptr<extensions::ExtensionsClient> extensions_client;
  std::unique_ptr<RendererClient> renderer_client;
};

State& state() {
  static State* state = new State;
  return *state;
}

extensions::Dispatcher* dispatcher() {
  return state().renderer_client ? state().renderer_client->GetDispatcher()
                                 : nullptr;
}

}  // namespace

void RenderThreadStarted() {
  State& s = state();
  s.extensions_client = std::make_unique<extensions::ShellExtensionsClient>();
  s.extensions_client->AddAPIProvider(
      std::make_unique<extensions::RChExtensionsAPIProvider>());
  extensions::ExtensionsClient::Set(s.extensions_client.get());
  s.renderer_client = std::make_unique<RendererClient>();
  extensions::ExtensionsRendererClient::Set(s.renderer_client.get());
  RenderThread::Get()->AddObserver(s.renderer_client->GetDispatcher());
}

void RenderFrameCreated(RenderFrame* render_frame) {
  extensions::Dispatcher* d = dispatcher();
  if (!d)
    return;
  // ExtensionFrameHelper destroys itself with the frame.
  new extensions::ExtensionFrameHelper(render_frame, d);
  d->OnRenderFrameCreated(render_frame);
}

void RunScriptsAtDocumentStart(RenderFrame* render_frame) {
  if (extensions::Dispatcher* d = dispatcher())
    d->RunScriptsAtDocumentStart(render_frame);
}

void RunScriptsAtDocumentEnd(RenderFrame* render_frame) {
  if (extensions::Dispatcher* d = dispatcher())
    d->RunScriptsAtDocumentEnd(render_frame);
}

void RunScriptsAtDocumentIdle(RenderFrame* render_frame) {
  if (extensions::Dispatcher* d = dispatcher())
    d->RunScriptsAtDocumentIdle(render_frame);
}

void DidInitializeServiceWorkerContextOnWorkerThread(
    blink::WebServiceWorkerContextProxy* context_proxy,
    const GURL& service_worker_scope,
    const GURL& script_url) {
  if (extensions::Dispatcher* d = dispatcher()) {
    d->DidInitializeServiceWorkerContextOnWorkerThread(
        context_proxy, service_worker_scope, script_url);
  }
}

void WillEvaluateServiceWorkerOnWorkerThread(
    blink::WebServiceWorkerContextProxy* context_proxy,
    v8::Local<v8::Context> v8_context,
    int64_t service_worker_version_id,
    const GURL& service_worker_scope,
    const GURL& script_url) {
  if (extensions::Dispatcher* d = dispatcher()) {
    d->WillEvaluateServiceWorkerOnWorkerThread(
        context_proxy, v8_context, service_worker_version_id,
        service_worker_scope, script_url);
  }
}

void DidStartServiceWorkerContextOnWorkerThread(
    int64_t service_worker_version_id,
    const GURL& service_worker_scope,
    const GURL& script_url) {
  if (extensions::Dispatcher* d = dispatcher()) {
    d->DidStartServiceWorkerContextOnWorkerThread(
        service_worker_version_id, service_worker_scope, script_url);
  }
}

void WillDestroyServiceWorkerContextOnWorkerThread(
    v8::Local<v8::Context> context,
    int64_t service_worker_version_id,
    const GURL& service_worker_scope,
    const GURL& script_url) {
  if (extensions::Dispatcher* d = dispatcher()) {
    d->WillDestroyServiceWorkerContextOnWorkerThread(
        context, service_worker_version_id, service_worker_scope, script_url);
  }
}

}  // namespace haiku_extensions
}  // namespace content
