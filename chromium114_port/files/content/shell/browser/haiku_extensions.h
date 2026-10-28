// Copyright 2026 The RENKU project
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Chromium's extension system in content_shell, for the 114 Haiku port.
//
// The embedder is app_shell's (//extensions/shell:app_shell_lib), which
// depends on content_shell_lib; content_shell_lib cannot depend back on it.
// So content_shell_lib calls only these functions, which take content types
// alone, and the content_shell executable links their definitions
// (:haiku_extensions, haiku_extensions.cc) together with app_shell_lib.

#ifndef CONTENT_SHELL_BROWSER_HAIKU_EXTENSIONS_H_
#define CONTENT_SHELL_BROWSER_HAIKU_EXTENSIONS_H_

#include <memory>
#include <string>
#include <vector>

#include "content/public/browser/content_browser_client.h"

namespace base {
class CommandLine;
}

namespace blink {
class AssociatedInterfaceRegistry;
}

namespace content {

class BrowserContext;
class NavigationHandle;
class NavigationThrottle;
class RenderFrameHost;
class RenderProcessHost;
class SiteInstance;
class WebContents;

namespace haiku_extensions {

// ShellBrowserMainParts.
void PreCreateThreads();
// After the browser contexts exist, before the first window: the extension
// system, its keyed services, and the extensions in the profile's
// Extensions folder and on --load-extension.
void Init(BrowserContext* browser_context);
void Shutdown(BrowserContext* browser_context);
void PostDestroyThreads();

// Every WebContents a Shell is made for.
void WebContentsCreated(WebContents* web_contents);

// An extension's toolbar button in the window showing `web_contents`.
void OnActionClicked(WebContents* web_contents, const std::string& id);

// ShellContentBrowserClient.
using NonNetworkURLLoaderFactoryMap =
    ContentBrowserClient::NonNetworkURLLoaderFactoryMap;

void RenderProcessWillLaunch(RenderProcessHost* host);
void SiteInstanceGotProcess(SiteInstance* site_instance);
void SiteInstanceDeleting(SiteInstance* site_instance);
void AppendExtraCommandLineSwitches(base::CommandLine* command_line,
                                    int child_process_id);
void ExposeInterfacesToRenderer(
    service_manager::BinderRegistry* registry,
    blink::AssociatedInterfaceRegistry* associated_registry,
    RenderProcessHost* render_process_host);
void RegisterAssociatedInterfaceBindersForRenderFrameHost(
    RenderFrameHost& render_frame_host,
    blink::AssociatedInterfaceRegistry& associated_registry);
void RegisterBrowserInterfaceBindersForFrame(
    RenderFrameHost* render_frame_host,
    mojo::BinderMapWithContext<RenderFrameHost*>* map);
void AddNavigationThrottles(
    NavigationHandle* navigation_handle,
    std::vector<std::unique_ptr<NavigationThrottle>>* throttles);
std::unique_ptr<NavigationUIData> GetNavigationUIData(
    NavigationHandle* navigation_handle);
void RegisterNonNetworkNavigationURLLoaderFactories(
    int frame_tree_node_id,
    ukm::SourceIdObj ukm_source_id,
    NonNetworkURLLoaderFactoryMap* factories);
void RegisterNonNetworkWorkerMainResourceURLLoaderFactories(
    BrowserContext* browser_context,
    NonNetworkURLLoaderFactoryMap* factories);
void RegisterNonNetworkServiceWorkerUpdateURLLoaderFactories(
    BrowserContext* browser_context,
    NonNetworkURLLoaderFactoryMap* factories);
void RegisterNonNetworkSubresourceURLLoaderFactories(
    int render_process_id,
    int render_frame_id,
    NonNetworkURLLoaderFactoryMap* factories);
bool WillCreateURLLoaderFactory(
    BrowserContext* browser_context,
    RenderFrameHost* frame,
    int render_process_id,
    ContentBrowserClient::URLLoaderFactoryType type,
    absl::optional<int64_t> navigation_id,
    ukm::SourceIdObj ukm_source_id,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory>* factory_receiver,
    mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>*
        header_client,
    bool* bypass_redirect_checks);
void OverrideURLLoaderFactoryParams(
    BrowserContext* browser_context,
    const url::Origin& origin,
    bool is_for_isolated_world,
    network::mojom::URLLoaderFactoryParams* factory_params);

}  // namespace haiku_extensions
}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_HAIKU_EXTENSIONS_H_
