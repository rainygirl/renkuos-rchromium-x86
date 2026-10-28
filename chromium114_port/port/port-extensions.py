#!/usr/bin/env python3
"""Chromium's extension system in the 114 content_shell.

The arm64 and x86_64 ports (Chromium 154) embed //extensions in content_shell
following //chrome, because app_shell is gone there. 114 still has app_shell,
and its embedder -- ExtensionsBrowserClient, ExtensionSystem, the renderer
client -- is what this port builds on. //extensions itself compiles with the
Haiku gcc as it is; what stops it is three platform switches with no Haiku
branch, fixed here.

Every edit checks whether it is already in place, and prints PATTERN NOT FOUND
when its anchor has moved.
"""
import os
import sys

root = sys.argv[1]


def edit(rel, old, new, label):
    path = os.path.join(root, rel)
    s = open(path).read()
    if new in s:
        print("  %s: already applied" % label)
        return
    if s.count(old) != 1:
        print("  %s: PATTERN NOT FOUND (%d)" % (label, s.count(old)))
        return
    open(path, "w").write(s.replace(old, new, 1))
    print("  %s: applied" % label)


# The update client tells the server which OS it runs on; extensions link it
# for the Web Store's update checks, which this port never makes.
edit("components/update_client/update_query_params.cc",
     '#elif BUILDFLAG(IS_OPENBSD)\n'
     '    "openbsd";\n',
     '#elif BUILDFLAG(IS_OPENBSD)\n'
     '    "openbsd";\n'
     '#elif BUILDFLAG(IS_HAIKU)\n'
     '    "haiku";\n',
     "update_query_params os")

# Which of a manifest's per-OS "commands" key bindings apply. Haiku takes the
# Linux ones, as Fuchsia does here and desktop Android does upstream later.
edit("extensions/common/command.cc",
     '#elif BUILDFLAG(IS_FUCHSIA)\n'
     '  // TODO(crbug.com/1312215): Change this once we decide what string should be\n'
     '  // used for Fuchsia.\n'
     '  return values::kKeybindingPlatformLinux;\n',
     '#elif BUILDFLAG(IS_FUCHSIA)\n'
     '  // TODO(crbug.com/1312215): Change this once we decide what string should be\n'
     '  // used for Fuchsia.\n'
     '  return values::kKeybindingPlatformLinux;\n'
     '#elif BUILDFLAG(IS_HAIKU)\n'
     '  return values::kKeybindingPlatformLinux;\n',
     "command platform")

# app_shell's main delegate declares the zygote hook for every POSIX system;
# content declares it only where there is a zygote, and Haiku has none.
edit("extensions/shell/app/shell_main_delegate.h",
     "#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_ANDROID)\n",
     "#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_ANDROID) && \\\n"
     "    !BUILDFLAG(IS_HAIKU)\n",
     "shell_main_delegate.h zygote")
edit("extensions/shell/app/shell_main_delegate.cc",
     "#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_ANDROID)\n"
     "void ShellMainDelegate::ZygoteStarting(",
     "#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_ANDROID) && \\\n"
     "    !BUILDFLAG(IS_HAIKU)\n"
     "void ShellMainDelegate::ZygoteStarting(",
     "shell_main_delegate.cc zygote")


# ---- content_shell -------------------------------------------------------
#
# content_shell_lib calls content/shell/{browser,renderer}/haiku_extensions*.h
# (files/), defined in :haiku_extensions, which the executable links with
# app_shell_lib. Everything is behind IS_HAIKU.

BMP = "content/shell/browser/shell_browser_main_parts.cc"
edit(BMP,
     '#include "content/shell/browser/shell_browser_main_parts.h"\n',
     '#include "content/shell/browser/shell_browser_main_parts.h"\n'
     '\n'
     '#if BUILDFLAG(IS_HAIKU)\n'
     '#include "content/shell/browser/haiku_extensions.h"\n'
     '#endif\n',
     "main parts include")
edit(BMP,
     'int ShellBrowserMainParts::PreCreateThreads() {\n',
     'int ShellBrowserMainParts::PreCreateThreads() {\n'
     '#if BUILDFLAG(IS_HAIKU)\n'
     '  haiku_extensions::PreCreateThreads();\n'
     '#endif\n',
     "main parts PreCreateThreads")
edit(BMP,
     '  InitializeBrowserContexts();\n'
     '  Shell::Initialize(CreateShellPlatformDelegate());\n',
     '  InitializeBrowserContexts();\n'
     '#if BUILDFLAG(IS_HAIKU)\n'
     '  // Before the first window, so that its navigation sees the rules of\n'
     '  // the extensions loaded at start-up.\n'
     '  haiku_extensions::Init(browser_context_.get());\n'
     '#endif\n'
     '  Shell::Initialize(CreateShellPlatformDelegate());\n',
     "main parts Init")
edit(BMP,
     '  ShellDevToolsManagerDelegate::StopHttpHandler();\n'
     '  browser_context_.reset();\n',
     '  ShellDevToolsManagerDelegate::StopHttpHandler();\n'
     '#if BUILDFLAG(IS_HAIKU)\n'
     '  haiku_extensions::Shutdown(browser_context_.get());\n'
     '#endif\n'
     '  browser_context_.reset();\n',
     "main parts Shutdown")
edit(BMP,
     'void ShellBrowserMainParts::PostDestroyThreads() {\n',
     'void ShellBrowserMainParts::PostDestroyThreads() {\n'
     '#if BUILDFLAG(IS_HAIKU)\n'
     '  haiku_extensions::PostDestroyThreads();\n'
     '#endif\n',
     "main parts PostDestroyThreads")

edit("content/shell/browser/shell.cc",
     '  windows_.push_back(this);\n',
     '  windows_.push_back(this);\n'
     '#if BUILDFLAG(IS_HAIKU)\n'
     '  haiku_extensions::WebContentsCreated(web_contents_.get());\n'
     '#endif\n',
     "shell WebContentsCreated")
edit("content/shell/browser/shell.cc",
     '#include "content/shell/browser/shell.h"\n',
     '#include "content/shell/browser/shell.h"\n'
     '\n'
     '#if BUILDFLAG(IS_HAIKU)\n'
     '#include "content/shell/browser/haiku_extensions.h"\n'
     '#endif\n',
     "shell include")

# chrome-extension:// is a standard, secure scheme whose pages bypass the
# page's CSP, as in app_shell's ShellContentClient.
edit("content/shell/common/shell_content_client.cc",
     'void ShellContentClient::AddAdditionalSchemes(Schemes* schemes) {\n',
     'void ShellContentClient::AddAdditionalSchemes(Schemes* schemes) {\n'
     '#if BUILDFLAG(IS_HAIKU)\n'
     '  static const char kExtensionScheme[] = "chrome-extension";\n'
     '  schemes->standard_schemes.push_back(kExtensionScheme);\n'
     '  schemes->savable_schemes.push_back(kExtensionScheme);\n'
     '  schemes->secure_schemes.push_back(kExtensionScheme);\n'
     '  schemes->cors_enabled_schemes.push_back(kExtensionScheme);\n'
     '  schemes->csp_bypassing_schemes.push_back(kExtensionScheme);\n'
     '  schemes->service_worker_schemes.push_back(kExtensionScheme);\n'
     '#endif\n',
     "content client schemes")

BCH = "content/shell/browser/shell_content_browser_client.h"
edit(BCH,
     '  void OverrideURLLoaderFactoryParams(\n'
     '      BrowserContext* browser_context,\n'
     '      const url::Origin& origin,\n'
     '      bool is_for_isolated_world,\n'
     '      network::mojom::URLLoaderFactoryParams* factory_params) override;\n',
     '  void OverrideURLLoaderFactoryParams(\n'
     '      BrowserContext* browser_context,\n'
     '      const url::Origin& origin,\n'
     '      bool is_for_isolated_world,\n'
     '      network::mojom::URLLoaderFactoryParams* factory_params) override;\n'
     '#if BUILDFLAG(IS_HAIKU)\n'
     '  // Extensions (haiku_extensions.h).\n'
     '  void RenderProcessWillLaunch(RenderProcessHost* host) override;\n'
     '  void SiteInstanceGotProcess(SiteInstance* site_instance) override;\n'
     '  void SiteInstanceDeleting(SiteInstance* site_instance) override;\n'
     '  void GetAdditionalAllowedSchemesForFileSystem(\n'
     '      std::vector<std::string>* additional_schemes) override;\n'
     '  void RegisterAssociatedInterfaceBindersForRenderFrameHost(\n'
     '      RenderFrameHost& render_frame_host,\n'
     '      blink::AssociatedInterfaceRegistry& associated_registry) override;\n'
     '  std::unique_ptr<NavigationUIData> GetNavigationUIData(\n'
     '      NavigationHandle* navigation_handle) override;\n'
     '  void RegisterNonNetworkNavigationURLLoaderFactories(\n'
     '      int frame_tree_node_id,\n'
     '      ukm::SourceIdObj ukm_source_id,\n'
     '      NonNetworkURLLoaderFactoryMap* factories) override;\n'
     '  void RegisterNonNetworkWorkerMainResourceURLLoaderFactories(\n'
     '      BrowserContext* browser_context,\n'
     '      NonNetworkURLLoaderFactoryMap* factories) override;\n'
     '  void RegisterNonNetworkServiceWorkerUpdateURLLoaderFactories(\n'
     '      BrowserContext* browser_context,\n'
     '      NonNetworkURLLoaderFactoryMap* factories) override;\n'
     '  void RegisterNonNetworkSubresourceURLLoaderFactories(\n'
     '      int render_process_id,\n'
     '      int render_frame_id,\n'
     '      const absl::optional<url::Origin>& request_initiator_origin,\n'
     '      NonNetworkURLLoaderFactoryMap* factories) override;\n'
     '  bool WillCreateURLLoaderFactory(\n'
     '      BrowserContext* browser_context,\n'
     '      RenderFrameHost* frame,\n'
     '      int render_process_id,\n'
     '      URLLoaderFactoryType type,\n'
     '      const url::Origin& request_initiator,\n'
     '      absl::optional<int64_t> navigation_id,\n'
     '      ukm::SourceIdObj ukm_source_id,\n'
     '      mojo::PendingReceiver<network::mojom::URLLoaderFactory>*\n'
     '          factory_receiver,\n'
     '      mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>*\n'
     '          header_client,\n'
     '      bool* bypass_redirect_checks,\n'
     '      bool* disable_secure_dns,\n'
     '      network::mojom::URLLoaderFactoryOverridePtr* factory_override)\n'
     '      override;\n'
     '#endif\n',
     "browser client declarations")

BCC = "content/shell/browser/shell_content_browser_client.cc"
edit(BCC,
     '#include "content/shell/browser/shell_content_browser_client.h"\n',
     '#include "content/shell/browser/shell_content_browser_client.h"\n'
     '\n'
     '#if BUILDFLAG(IS_HAIKU)\n'
     '#include "content/shell/browser/haiku_extensions.h"\n'
     '#endif\n',
     "browser client include")
edit(BCC,
     'bool ShellContentBrowserClient::IsHandledURL(const GURL& url) {\n'
     '  if (!url.is_valid())\n'
     '    return false;\n',
     'bool ShellContentBrowserClient::IsHandledURL(const GURL& url) {\n'
     '  if (!url.is_valid())\n'
     '    return false;\n'
     '#if BUILDFLAG(IS_HAIKU)\n'
     '  if (url.SchemeIs("chrome-extension"))\n'
     '    return true;\n'
     '#endif\n',
     "browser client IsHandledURL")
edit(BCC,
     'void ShellContentBrowserClient::AppendExtraCommandLineSwitches(\n'
     '    base::CommandLine* command_line,\n'
     '    int child_process_id) {\n',
     'void ShellContentBrowserClient::AppendExtraCommandLineSwitches(\n'
     '    base::CommandLine* command_line,\n'
     '    int child_process_id) {\n'
     '#if BUILDFLAG(IS_HAIKU)\n'
     '  haiku_extensions::AppendExtraCommandLineSwitches(command_line,\n'
     '                                                  child_process_id);\n'
     '#endif\n',
     "browser client switches")
edit(BCC,
     '  performance_manager::PerformanceManagerRegistry::GetInstance()\n'
     '      ->CreateProcessNodeAndExposeInterfacesToRendererProcess(\n'
     '          registry, render_process_host);\n',
     '  performance_manager::PerformanceManagerRegistry::GetInstance()\n'
     '      ->CreateProcessNodeAndExposeInterfacesToRendererProcess(\n'
     '          registry, render_process_host);\n'
     '#if BUILDFLAG(IS_HAIKU)\n'
     '  haiku_extensions::ExposeInterfacesToRenderer(\n'
     '      registry, associated_registry, render_process_host);\n'
     '#endif\n',
     "browser client interfaces")
edit(BCC,
     '  map->Add<network_hints::mojom::NetworkHintsHandler>(\n'
     '      base::BindRepeating(&BindNetworkHintsHandler));\n',
     '  map->Add<network_hints::mojom::NetworkHintsHandler>(\n'
     '      base::BindRepeating(&BindNetworkHintsHandler));\n'
     '#if BUILDFLAG(IS_HAIKU)\n'
     '  haiku_extensions::RegisterBrowserInterfaceBindersForFrame(\n'
     '      render_frame_host, map);\n'
     '#endif\n',
     "browser client frame binders")
edit(BCC,
     '  std::vector<std::unique_ptr<NavigationThrottle>> empty_throttles;\n'
     '  if (create_throttles_for_navigation_callback_)\n'
     '    return create_throttles_for_navigation_callback_.Run(navigation_handle);\n'
     '  return empty_throttles;\n',
     '  std::vector<std::unique_ptr<NavigationThrottle>> empty_throttles;\n'
     '  if (create_throttles_for_navigation_callback_)\n'
     '    return create_throttles_for_navigation_callback_.Run(navigation_handle);\n'
     '#if BUILDFLAG(IS_HAIKU)\n'
     '  haiku_extensions::AddNavigationThrottles(navigation_handle,\n'
     '                                          &empty_throttles);\n'
     '#endif\n'
     '  return empty_throttles;\n',
     "browser client throttles")
edit(BCC,
     '  if (url_loader_factory_params_callback_) {\n'
     '    url_loader_factory_params_callback_.Run(factory_params, origin,\n'
     '                                            is_for_isolated_world);\n'
     '  }\n',
     '  if (url_loader_factory_params_callback_) {\n'
     '    url_loader_factory_params_callback_.Run(factory_params, origin,\n'
     '                                            is_for_isolated_world);\n'
     '  }\n'
     '#if BUILDFLAG(IS_HAIKU)\n'
     '  haiku_extensions::OverrideURLLoaderFactoryParams(\n'
     '      browser_context, origin, is_for_isolated_world, factory_params);\n'
     '#endif\n',
     "browser client factory params")
edit(BCC,
     '// static\n'
     'const std::vector<ShellContentBrowserClient*>&\n'
     'ShellContentBrowserClient::GetShellContentBrowserClientInstances() {\n',
     '#if BUILDFLAG(IS_HAIKU)\n'
     'void ShellContentBrowserClient::RenderProcessWillLaunch(\n'
     '    RenderProcessHost* host) {\n'
     '  haiku_extensions::RenderProcessWillLaunch(host);\n'
     '}\n'
     '\n'
     'void ShellContentBrowserClient::SiteInstanceGotProcess(\n'
     '    SiteInstance* site_instance) {\n'
     '  haiku_extensions::SiteInstanceGotProcess(site_instance);\n'
     '}\n'
     '\n'
     'void ShellContentBrowserClient::SiteInstanceDeleting(\n'
     '    SiteInstance* site_instance) {\n'
     '  haiku_extensions::SiteInstanceDeleting(site_instance);\n'
     '}\n'
     '\n'
     'void ShellContentBrowserClient::GetAdditionalAllowedSchemesForFileSystem(\n'
     '    std::vector<std::string>* additional_schemes) {\n'
     '  ContentBrowserClient::GetAdditionalAllowedSchemesForFileSystem(\n'
     '      additional_schemes);\n'
     '  additional_schemes->push_back("chrome-extension");\n'
     '}\n'
     '\n'
     'void ShellContentBrowserClient::\n'
     '    RegisterAssociatedInterfaceBindersForRenderFrameHost(\n'
     '        RenderFrameHost& render_frame_host,\n'
     '        blink::AssociatedInterfaceRegistry& associated_registry) {\n'
     '  haiku_extensions::RegisterAssociatedInterfaceBindersForRenderFrameHost(\n'
     '      render_frame_host, associated_registry);\n'
     '}\n'
     '\n'
     'std::unique_ptr<NavigationUIData>\n'
     'ShellContentBrowserClient::GetNavigationUIData(\n'
     '    NavigationHandle* navigation_handle) {\n'
     '  return haiku_extensions::GetNavigationUIData(navigation_handle);\n'
     '}\n'
     '\n'
     'void ShellContentBrowserClient::RegisterNonNetworkNavigationURLLoaderFactories(\n'
     '    int frame_tree_node_id,\n'
     '    ukm::SourceIdObj ukm_source_id,\n'
     '    NonNetworkURLLoaderFactoryMap* factories) {\n'
     '  haiku_extensions::RegisterNonNetworkNavigationURLLoaderFactories(\n'
     '      frame_tree_node_id, ukm_source_id, factories);\n'
     '}\n'
     '\n'
     'void ShellContentBrowserClient::\n'
     '    RegisterNonNetworkWorkerMainResourceURLLoaderFactories(\n'
     '        BrowserContext* browser_context,\n'
     '        NonNetworkURLLoaderFactoryMap* factories) {\n'
     '  haiku_extensions::RegisterNonNetworkWorkerMainResourceURLLoaderFactories(\n'
     '      browser_context, factories);\n'
     '}\n'
     '\n'
     'void ShellContentBrowserClient::\n'
     '    RegisterNonNetworkServiceWorkerUpdateURLLoaderFactories(\n'
     '        BrowserContext* browser_context,\n'
     '        NonNetworkURLLoaderFactoryMap* factories) {\n'
     '  haiku_extensions::RegisterNonNetworkServiceWorkerUpdateURLLoaderFactories(\n'
     '      browser_context, factories);\n'
     '}\n'
     '\n'
     'void ShellContentBrowserClient::RegisterNonNetworkSubresourceURLLoaderFactories(\n'
     '    int render_process_id,\n'
     '    int render_frame_id,\n'
     '    const absl::optional<url::Origin>& request_initiator_origin,\n'
     '    NonNetworkURLLoaderFactoryMap* factories) {\n'
     '  haiku_extensions::RegisterNonNetworkSubresourceURLLoaderFactories(\n'
     '      render_process_id, render_frame_id, factories);\n'
     '}\n'
     '\n'
     'bool ShellContentBrowserClient::WillCreateURLLoaderFactory(\n'
     '    BrowserContext* browser_context,\n'
     '    RenderFrameHost* frame,\n'
     '    int render_process_id,\n'
     '    URLLoaderFactoryType type,\n'
     '    const url::Origin& request_initiator,\n'
     '    absl::optional<int64_t> navigation_id,\n'
     '    ukm::SourceIdObj ukm_source_id,\n'
     '    mojo::PendingReceiver<network::mojom::URLLoaderFactory>* factory_receiver,\n'
     '    mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>*\n'
     '        header_client,\n'
     '    bool* bypass_redirect_checks,\n'
     '    bool* disable_secure_dns,\n'
     '    network::mojom::URLLoaderFactoryOverridePtr* factory_override) {\n'
     '  return haiku_extensions::WillCreateURLLoaderFactory(\n'
     '      browser_context, frame, render_process_id, type,\n'
     '      std::move(navigation_id), ukm_source_id, factory_receiver,\n'
     '      header_client, bypass_redirect_checks);\n'
     '}\n'
     '#endif  // BUILDFLAG(IS_HAIKU)\n'
     '\n'
     '// static\n'
     'const std::vector<ShellContentBrowserClient*>&\n'
     'ShellContentBrowserClient::GetShellContentBrowserClientInstances() {\n',
     "browser client definitions")

RCH = "content/shell/renderer/shell_content_renderer_client.h"
edit(RCH,
     '  void DidInitializeWorkerContextOnWorkerThread(\n'
     '      v8::Local<v8::Context> context) override;\n',
     '  void DidInitializeWorkerContextOnWorkerThread(\n'
     '      v8::Local<v8::Context> context) override;\n'
     '#if BUILDFLAG(IS_HAIKU)\n'
     '  // Extensions (haiku_extensions_renderer.h).\n'
     '  void RunScriptsAtDocumentStart(RenderFrame* render_frame) override;\n'
     '  void RunScriptsAtDocumentEnd(RenderFrame* render_frame) override;\n'
     '  void RunScriptsAtDocumentIdle(RenderFrame* render_frame) override;\n'
     '  void DidInitializeServiceWorkerContextOnWorkerThread(\n'
     '      blink::WebServiceWorkerContextProxy* context_proxy,\n'
     '      const GURL& service_worker_scope,\n'
     '      const GURL& script_url) override;\n'
     '  void WillEvaluateServiceWorkerOnWorkerThread(\n'
     '      blink::WebServiceWorkerContextProxy* context_proxy,\n'
     '      v8::Local<v8::Context> v8_context,\n'
     '      int64_t service_worker_version_id,\n'
     '      const GURL& service_worker_scope,\n'
     '      const GURL& script_url) override;\n'
     '  void DidStartServiceWorkerContextOnWorkerThread(\n'
     '      int64_t service_worker_version_id,\n'
     '      const GURL& service_worker_scope,\n'
     '      const GURL& script_url) override;\n'
     '  void WillDestroyServiceWorkerContextOnWorkerThread(\n'
     '      v8::Local<v8::Context> context,\n'
     '      int64_t service_worker_version_id,\n'
     '      const GURL& service_worker_scope,\n'
     '      const GURL& script_url) override;\n'
     '#endif\n',
     "renderer client declarations")

RCC = "content/shell/renderer/shell_content_renderer_client.cc"
edit(RCC,
     '#include "content/shell/renderer/shell_content_renderer_client.h"\n',
     '#include "content/shell/renderer/shell_content_renderer_client.h"\n'
     '\n'
     '#if BUILDFLAG(IS_HAIKU)\n'
     '#include "content/shell/renderer/haiku_extensions_renderer.h"\n'
     '#endif\n',
     "renderer client include")
edit(RCC,
     'void ShellContentRendererClient::RenderThreadStarted() {\n'
     '  web_cache_impl_ = std::make_unique<web_cache::WebCacheImpl>();\n',
     'void ShellContentRendererClient::RenderThreadStarted() {\n'
     '  web_cache_impl_ = std::make_unique<web_cache::WebCacheImpl>();\n'
     '#if BUILDFLAG(IS_HAIKU)\n'
     '  haiku_extensions::RenderThreadStarted();\n'
     '#endif\n',
     "renderer client thread start")
edit(RCC,
     '  new ShellRenderFrameObserver(render_frame);\n'
     '}\n',
     '  new ShellRenderFrameObserver(render_frame);\n'
     '#if BUILDFLAG(IS_HAIKU)\n'
     '  haiku_extensions::RenderFrameCreated(render_frame);\n'
     '#endif\n'
     '}\n'
     '\n'
     '#if BUILDFLAG(IS_HAIKU)\n'
     'void ShellContentRendererClient::RunScriptsAtDocumentStart(\n'
     '    RenderFrame* render_frame) {\n'
     '  haiku_extensions::RunScriptsAtDocumentStart(render_frame);\n'
     '}\n'
     '\n'
     'void ShellContentRendererClient::RunScriptsAtDocumentEnd(\n'
     '    RenderFrame* render_frame) {\n'
     '  haiku_extensions::RunScriptsAtDocumentEnd(render_frame);\n'
     '}\n'
     '\n'
     'void ShellContentRendererClient::RunScriptsAtDocumentIdle(\n'
     '    RenderFrame* render_frame) {\n'
     '  haiku_extensions::RunScriptsAtDocumentIdle(render_frame);\n'
     '}\n'
     '\n'
     'void ShellContentRendererClient::DidInitializeServiceWorkerContextOnWorkerThread(\n'
     '    blink::WebServiceWorkerContextProxy* context_proxy,\n'
     '    const GURL& service_worker_scope,\n'
     '    const GURL& script_url) {\n'
     '  haiku_extensions::DidInitializeServiceWorkerContextOnWorkerThread(\n'
     '      context_proxy, service_worker_scope, script_url);\n'
     '}\n'
     '\n'
     'void ShellContentRendererClient::WillEvaluateServiceWorkerOnWorkerThread(\n'
     '    blink::WebServiceWorkerContextProxy* context_proxy,\n'
     '    v8::Local<v8::Context> v8_context,\n'
     '    int64_t service_worker_version_id,\n'
     '    const GURL& service_worker_scope,\n'
     '    const GURL& script_url) {\n'
     '  haiku_extensions::WillEvaluateServiceWorkerOnWorkerThread(\n'
     '      context_proxy, v8_context, service_worker_version_id,\n'
     '      service_worker_scope, script_url);\n'
     '}\n'
     '\n'
     'void ShellContentRendererClient::DidStartServiceWorkerContextOnWorkerThread(\n'
     '    int64_t service_worker_version_id,\n'
     '    const GURL& service_worker_scope,\n'
     '    const GURL& script_url) {\n'
     '  haiku_extensions::DidStartServiceWorkerContextOnWorkerThread(\n'
     '      service_worker_version_id, service_worker_scope, script_url);\n'
     '}\n'
     '\n'
     'void ShellContentRendererClient::WillDestroyServiceWorkerContextOnWorkerThread(\n'
     '    v8::Local<v8::Context> context,\n'
     '    int64_t service_worker_version_id,\n'
     '    const GURL& service_worker_scope,\n'
     '    const GURL& script_url) {\n'
     '  haiku_extensions::WillDestroyServiceWorkerContextOnWorkerThread(\n'
     '      context, service_worker_version_id, service_worker_scope, script_url);\n'
     '}\n'
     '#endif  // BUILDFLAG(IS_HAIKU)\n',
     "renderer client definitions")

# ---- gn ------------------------------------------------------------------

BG = "content/shell/BUILD.gn"
edit(BG,
     '    deps = [\n'
     '      ":content_shell_app",\n'
     '      ":pak",\n'
     '      "//build/win:default_exe_manifest",\n',
     '    deps = [\n'
     '      ":content_shell_app",\n'
     '      ":pak",\n'
     '      "//build/win:default_exe_manifest",\n'
     '    ]\n'
     '    if (is_haiku) {\n'
     '      deps += [ ":haiku_extensions" ]\n'
     '    }\n'
     '    deps += [\n',
     "content_shell links :haiku_extensions")
edit(BG,
     '  if (!is_android) {\n'
     '    deps += [ "//content/browser/tracing:resources" ]\n',
     '  if (is_haiku) {\n'
     '    # The extension system\'s own resources: its JS bindings, the\n'
     '    # manifest and API strings, app_shell\'s pieces.\n'
     '    sources += [\n'
     '      "$root_gen_dir/extensions/extensions_browser_resources_100_percent.pak",\n'
     '      "$root_gen_dir/extensions/extensions_renderer_resources.pak",\n'
     '      "$root_gen_dir/extensions/extensions_resources.pak",\n'
     '      "$root_gen_dir/extensions/shell/app_shell_resources.pak",\n'
     '      "$root_gen_dir/extensions/strings/extensions_strings_en-US.pak",\n'
     '    ]\n'
     '    deps += [\n'
     '      "//extensions:extensions_resources",\n'
     '      "//extensions/shell:resources",\n'
     '      "//extensions/strings",\n'
     '    ]\n'
     '  }\n'
     '\n'
     '  if (!is_android) {\n'
     '    deps += [ "//content/browser/tracing:resources" ]\n',
     "pak extension resources")


def append(rel, marker, text, label):
    path = os.path.join(root, rel)
    s = open(path).read()
    if marker in s:
        print("  %s: already applied" % label)
        return
    open(path, "a").write(text)
    print("  %s: applied" % label)


append(BG, 'source_set("haiku_extensions")', '''
if (is_haiku) {
  # The extension embedder content_shell_lib calls through
  # browser/haiku_extensions.h and renderer/haiku_extensions_renderer.h.
  # app_shell_lib depends on content_shell_lib, so this cannot be part of it;
  # the content_shell executable links both.
  source_set("haiku_extensions") {
    testonly = true
    sources = [
      "browser/extensions/shell_action_api.cc",
      "browser/extensions/shell_action_api.h",
      "browser/extensions/shell_extensions_browser_api_provider.cc",
      "browser/extensions/shell_extensions_browser_api_provider.h",
      "browser/extensions/shell_tabs.cc",
      "browser/extensions/shell_tabs.h",
      "browser/extensions/shell_tabs_api.cc",
      "browser/extensions/shell_tabs_api.h",
      "browser/haiku_extensions.cc",
      "browser/haiku_extensions.h",
      "common/extensions/rch_extensions_client.h",
      "common/extensions/shell_extensions_api_provider.cc",
      "common/extensions/shell_extensions_api_provider.h",
      "renderer/haiku_extensions_renderer.cc",
      "renderer/haiku_extensions_renderer.h",
    ]
    deps = [
      ":content_shell_lib",
      "//content/shell/common/extensions/api",
      "//haiku_port/ozone",
      "//skia",
      "//ui/gfx",
      "//apps",
      "//base",
      "//components/guest_view/common:mojom",
      "//components/keyed_service/content",
      "//components/prefs",
      "//components/sessions",
      "//components/user_prefs",
      "//content/public/browser",
      "//content/public/renderer",
      "//extensions/browser",
      "//extensions/common",
      "//extensions/common:mojom",
      "//extensions/renderer",
      "//extensions/shell:app_shell_lib",
      "//third_party/blink/public:blink",
    ]
  }
}
''', "haiku_extensions source_set")

# The component test targets assert that nothing they depend on links
# //extensions, and they reach content_shell's pak through
# //content/test:test_support. Those tests are never built here; the arm64
# port does the same in 154.
edit("components/BUILD.gn",
     '    "//extensions/strings/*",\n'
     '  ]\n',
     '    "//extensions/strings/*",\n'
     '  ]\n'
     '\n'
     '  # R Chromium (extensions): content_shell links //extensions.\n'
     '  if (is_haiku) {\n'
     '    disallowed_extension_deps_ = []\n'
     '  }\n',
     "components extension deps")

# chrome.system.cpu: Haiku's per-CPU busy time (files/).
edit("extensions/browser/api/system_cpu/BUILD.gn",
     '  if (is_win) {\n'
     '    sources += [ "cpu_info_provider_win.cc" ]\n'
     '  }\n',
     '  if (is_win) {\n'
     '    sources += [ "cpu_info_provider_win.cc" ]\n'
     '  }\n'
     '\n'
     '  if (is_haiku) {\n'
     '    sources += [ "cpu_info_provider_haiku.cc" ]\n'
     '  }\n',
     "system_cpu haiku provider")



# The schemas of the //chrome APIs content_shell implements, from this
# checkout (content/shell/common/extensions/api/BUILD.gn compiles them).
import shutil
for name in ("action", "cookies", "extension", "tab_groups", "tabs",
             "web_navigation", "windows"):
    shutil.copy(os.path.join(root, "chrome/common/extensions/api", name + ".json"),
                os.path.join(root, "content/shell/common/extensions/api"))
print("  chrome API schemas: copied")

# chrome.cookies is not ported to 114: its partition keys and cookie change
# causes are 154's. A tree that got the first source list drops it.
_bg = os.path.join(root, BG)
_s = open(_bg).read()
_cookies = ('      "browser/extensions/shell_cookies_api.cc",\n'
            '      "browser/extensions/shell_cookies_api.h",\n')
if _cookies in _s:
    open(_bg, "w").write(_s.replace(_cookies, ""))
    print("  haiku_extensions drops cookies: applied")
else:
    print("  haiku_extensions drops cookies: already applied")
