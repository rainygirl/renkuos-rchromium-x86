// Copyright 2026 The RENKU project
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// What app_shell's ShellBrowserMainParts and ShellContentBrowserClient do for
// extensions, done for content_shell's own browser context and windows.

#include "content/shell/browser/haiku_extensions.h"

#include "apps/browser_context_keyed_service_factories.h"
#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/string_tokenizer.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "components/guest_view/common/guest_view.mojom.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/core/session_id_generator.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_main_runner.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "extensions/browser/api/declarative_net_request/web_contents_helper.h"
#include "extensions/browser/api/messaging/messaging_api_message_filter.h"
#include "extensions/browser/api/web_request/web_request_api.h"
#include "extensions/browser/browser_context_keyed_service_factories.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_action.h"
#include "extensions/browser/extension_action_manager.h"
#include "extensions/browser/extension_icon_image.h"
#include "extensions/browser/extension_message_filter.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/extension_navigation_throttle.h"
#include "extensions/browser/extension_protocols.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_web_contents_observer.h"
#include "extensions/browser/guest_view/extensions_guest_view.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "extensions/browser/process_map.h"
#include "extensions/browser/renderer_startup_helper.h"
#include "extensions/browser/url_loader_factory_manager.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extensions_client.h"
#include "extensions/common/mojom/event_dispatcher.mojom.h"
#include "extensions/common/mojom/event_router.mojom.h"
#include "extensions/common/mojom/guest_view.mojom.h"
#include "extensions/common/mojom/renderer_host.mojom.h"
#include "extensions/common/switches.h"
#include "extensions/shell/browser/shell_browser_context_keyed_service_factories.h"
#include "extensions/shell/browser/shell_extension_system.h"
#include "extensions/shell/browser/shell_extension_web_contents_observer.h"
#include "extensions/shell/browser/shell_extensions_browser_client.h"
#include "extensions/shell/browser/shell_navigation_ui_data.h"
#include "extensions/shell/browser/shell_prefs.h"
#include "extensions/shell/common/shell_extensions_client.h"
#include "content/shell/browser/extensions/shell_action_api.h"
#include "content/shell/browser/extensions/shell_extensions_browser_api_provider.h"
#include "content/shell/browser/extensions/shell_tabs.h"
#include "content/shell/browser/haiku_toolbar.h"
#include "content/shell/browser/shell.h"
#include "content/shell/common/extensions/shell_extensions_api_provider.h"
#include "haiku_port/ozone/haiku_browser_chrome.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"

namespace content {
namespace haiku_extensions {

namespace {

// The same switch Chrome reads: a comma-separated list of unpacked
// extension directories, loaded for this run only.
const char kLoadExtension[] = "load-extension";

// Unpacked extensions that stay, one per directory, in the profile.
const char kExtensionsFolder[] = "Extensions";

// Chrome's TabHelper owns one per tab; here it hangs off the WebContents.
struct DnrHelperHolder : public base::SupportsUserData::Data {
  explicit DnrHelperHolder(WebContents* web_contents) : helper(web_contents) {}
  extensions::declarative_net_request::WebContentsHelper helper;
};
const char kDnrHelperKey[] = "rch_dnr_helper";

struct State {
  std::unique_ptr<extensions::ExtensionsClient> extensions_client;
  std::unique_ptr<extensions::ShellExtensionsBrowserClient> browser_client;
  std::unique_ptr<PrefService> local_state;
  std::unique_ptr<PrefService> user_prefs;
  BrowserContext* browser_context = nullptr;
};

State& state() {
  static State* state = new State;
  return *state;
}

const extensions::Extension* GetExtension(SiteInstance* site_instance) {
  return extensions::ExtensionRegistry::Get(site_instance->GetBrowserContext())
      ->enabled_extensions()
      .GetExtensionOrAppByURL(site_instance->GetSiteURL());
}

void LoadExtensions(extensions::ShellExtensionSystem* system,
                    BrowserContext* browser_context) {
  std::vector<base::FilePath> dirs;

  base::FilePath folder = browser_context->GetPath().Append(kExtensionsFolder);
  base::CreateDirectory(folder);
  base::FileEnumerator entries(folder, false,
                               base::FileEnumerator::DIRECTORIES);
  for (base::FilePath dir = entries.Next(); !dir.empty();
       dir = entries.Next()) {
    dirs.push_back(dir);
  }

  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kLoadExtension)) {
    base::CommandLine::StringType list =
        command_line->GetSwitchValueNative(kLoadExtension);
    base::StringTokenizerT<base::CommandLine::StringType,
                           base::CommandLine::StringType::const_iterator>
        tokenizer(list, FILE_PATH_LITERAL(","));
    while (tokenizer.GetNext()) {
      dirs.push_back(
          base::MakeAbsoluteFilePath(base::FilePath(tokenizer.token_piece())));
    }
  }

  for (const base::FilePath& dir : dirs) {
    if (!system->LoadExtension(dir))
      LOG(WARNING) << "could not load the extension in " << dir;
  }
}

// The extensions' toolbar buttons, in every window that has a toolbar: each
// action's title, icon and badge for that window's tab, and what a click
// does -- the popup, in a small window of its own, or action.onClicked.
class ActionToolbar : public extensions::ShellActions::Observer,
                      public extensions::ExtensionRegistryObserver,
                      public extensions::IconImage::Observer {
 public:
  explicit ActionToolbar(BrowserContext* context) : context_(context) {
    extensions::ShellActions::AddObserver(this);
    registry_observation_.Observe(extensions::ExtensionRegistry::Get(context));
    extensions::ShellTabs::SetTabChangedCallback(base::BindRepeating(
        &ActionToolbar::OnTabChanged, base::Unretained(this)));
  }

  ~ActionToolbar() override {
    extensions::ShellTabs::SetTabChangedCallback({});
    extensions::ShellActions::RemoveObserver(this);
  }

  // Every action into one tab's window.
  void PushTab(WebContents* tab) {
    for (const auto& extension : Registry()->enabled_extensions())
      Push(extension->id(), tab);
  }

  void Clicked(WebContents* tab, const std::string& id) {
    const extensions::Extension* extension =
        Registry()->enabled_extensions().GetByID(id);
    extensions::ExtensionAction* action = Action(extension);
    if (!action)
      return;
    const int tab_id = extensions::ShellTabs::GetTabId(tab);
    if (action->HasPopup(tab_id)) {
      SetNextHaikuWindowWithoutToolbar();
      Shell::CreateNewWindow(context_, action->GetPopupUrl(tab_id), nullptr,
                             gfx::Size(360, 480));
      return;
    }
    extensions::EventRouter* router = extensions::EventRouter::Get(context_);
    if (!router)
      return;
    base::Value::List args;
    args.Append(
        extensions::ShellTabs::CreateTabObject(tab, extension).ToValue());
    router->DispatchEventToExtension(
        id, std::make_unique<extensions::Event>(
                extensions::events::ACTION_ON_CLICKED, "action.onClicked",
                std::move(args), context_, GURL(),
                extensions::EventRouter::USER_GESTURE_ENABLED,
                extensions::mojom::EventFilteringInfoPtr()));
  }

  // extensions::ShellActions::Observer:
  void OnActionUpdated(const extensions::ExtensionId& extension_id,
                       int tab_id) override {
    for (WebContents* tab : extensions::ShellTabs::GetTabs(context_)) {
      if (tab_id == extensions::ExtensionAction::kDefaultTabId ||
          extensions::ShellTabs::GetTabId(tab) == tab_id) {
        Push(extension_id, tab);
      }
    }
  }

  bool OnOpenPopupRequested(
      const extensions::ExtensionId& extension_id) override {
    WebContents* tab = extensions::ShellTabs::GetActiveTab(context_);
    if (!tab)
      return false;
    Clicked(tab, extension_id);
    return true;
  }

  // extensions::ExtensionRegistryObserver:
  void OnExtensionLoaded(BrowserContext* context,
                         const extensions::Extension* extension) override {
    extensions::ExtensionAction* action = Action(extension);
    if (!action)
      return;
    // The default icon loads asynchronously; push again when it is in.
    if (extensions::IconImage* icon = action->default_icon_image())
      icon->AddObserver(this);
    PushAll(extension->id());
  }

  void OnExtensionUnloaded(BrowserContext* context,
                           const extensions::Extension* extension,
                           extensions::UnloadedExtensionReason reason) override {
    for (WebContents* tab : extensions::ShellTabs::GetTabs(context_)) {
      gfx::AcceleratedWidget widget = GetHaikuToolbarWidget(tab);
      if (widget != gfx::kNullAcceleratedWidget) {
        ui::SetBrowserChromeExtensionAction(widget, extension->id(), false,
                                            std::string(), nullptr, 0, 0,
                                            std::string(), 0);
      }
    }
  }

  // extensions::IconImage::Observer:
  void OnExtensionIconImageChanged(extensions::IconImage* image) override {
    for (const auto& extension : Registry()->enabled_extensions()) {
      extensions::ExtensionAction* action = Action(extension.get());
      if (action && action->default_icon_image() == image)
        PushAll(extension->id());
    }
  }

 private:
  extensions::ExtensionRegistry* Registry() {
    return extensions::ExtensionRegistry::Get(context_);
  }

  extensions::ExtensionAction* Action(const extensions::Extension* extension) {
    if (!extension)
      return nullptr;
    return extensions::ExtensionActionManager::Get(context_)
        ->GetExtensionAction(*extension);
  }

  void OnTabChanged(WebContents* tab, bool navigated) {
    if (navigated && tab) {
      // Per-tab values (badge text, title) belong to the page, as in Chrome.
      const int tab_id = extensions::ShellTabs::GetTabId(tab);
      for (const auto& extension : Registry()->enabled_extensions()) {
        if (extensions::ExtensionAction* action = Action(extension.get()))
          action->ClearAllValuesForTab(tab_id);
      }
    }
    if (tab)
      PushTab(tab);
  }

  void PushAll(const extensions::ExtensionId& id) {
    for (WebContents* tab : extensions::ShellTabs::GetTabs(context_))
      Push(id, tab);
  }

  void Push(const extensions::ExtensionId& id, WebContents* tab) {
    gfx::AcceleratedWidget widget = GetHaikuToolbarWidget(tab);
    if (widget == gfx::kNullAcceleratedWidget)
      return;
    extensions::ExtensionAction* action =
        Action(Registry()->enabled_extensions().GetByID(id));
    if (!action)
      return;
    const int tab_id = extensions::ShellTabs::GetTabId(tab);

    gfx::Image image = action->GetExplicitlySetIcon(tab_id);
    if (image.IsEmpty())
      image = action->GetDefaultIconImage();
    std::vector<uint32_t> argb;
    int width = 0, height = 0;
    if (!image.IsEmpty()) {
      const SkBitmap& bitmap =
          image.ToImageSkia()->GetRepresentation(1.0f).GetBitmap();
      width = bitmap.width();
      height = bitmap.height();
      argb.resize(static_cast<size_t>(width) * height);
      for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++)
          argb[static_cast<size_t>(y) * width + x] = bitmap.getColor(x, y);
      }
    }
    ui::SetBrowserChromeExtensionAction(
        widget, id, true, action->GetTitle(tab_id),
        argb.empty() ? nullptr : argb.data(), width, height,
        action->GetDisplayBadgeText(tab_id),
        action->GetBadgeBackgroundColor(tab_id));
  }

  raw_ptr<BrowserContext> context_;
  base::ScopedObservation<extensions::ExtensionRegistry,
                          extensions::ExtensionRegistryObserver>
      registry_observation_{this};
};

std::unique_ptr<ActionToolbar>& Toolbar() {
  static std::unique_ptr<ActionToolbar>* toolbar =
      new std::unique_ptr<ActionToolbar>;
  return *toolbar;
}

}  // namespace

void OnActionClicked(WebContents* web_contents, const std::string& id) {
  if (Toolbar())
    Toolbar()->Clicked(web_contents, id);
}

void PreCreateThreads() {
  ChildProcessSecurityPolicy::GetInstance()->RegisterWebSafeScheme(
      extensions::kExtensionScheme);
}

void Init(BrowserContext* browser_context) {
  State& s = state();
  s.browser_context = browser_context;

  s.extensions_client = std::make_unique<extensions::ShellExtensionsClient>();
  // tabs, action, webNavigation, cookies and extension: //chrome's APIs,
  // which this port implements itself (browser/extensions/).
  s.extensions_client->AddAPIProvider(
      std::make_unique<extensions::RChExtensionsAPIProvider>());
  extensions::ExtensionsClient::Set(s.extensions_client.get());

  s.browser_client = std::make_unique<extensions::ShellExtensionsBrowserClient>();
  s.browser_client->AddAPIProvider(
      std::make_unique<extensions::RChExtensionsBrowserAPIProvider>());
  extensions::ExtensionsBrowserClient::Set(s.browser_client.get());

  apps::EnsureBrowserContextKeyedServiceFactoriesBuilt();
  extensions::EnsureBrowserContextKeyedServiceFactoriesBuilt();
  extensions::shell::EnsureBrowserContextKeyedServiceFactoriesBuilt();

  s.local_state =
      extensions::shell_prefs::CreateLocalState(browser_context->GetPath());
  sessions::SessionIdGenerator::GetInstance()->Init(s.local_state.get());
  s.user_prefs =
      extensions::shell_prefs::CreateUserPrefService(browser_context);
  s.browser_client->InitWithBrowserContext(browser_context,
                                           s.user_prefs.get());

  BrowserContextDependencyManager::GetInstance()->CreateBrowserContextServices(
      browser_context);

  auto* system = static_cast<extensions::ShellExtensionSystem*>(
      extensions::ExtensionSystem::Get(browser_context));
  system->InitForRegularProfile(true /* extensions_enabled */);
  Toolbar() = std::make_unique<ActionToolbar>(browser_context);
  LoadExtensions(system, browser_context);
  system->FinishInitialization();
}

void Shutdown(BrowserContext* browser_context) {
  State& s = state();
  if (!s.browser_context)
    return;
  Toolbar().reset();
  BrowserContextDependencyManager::GetInstance()->DestroyBrowserContextServices(
      browser_context);
  sessions::SessionIdGenerator::GetInstance()->Shutdown();
  s.user_prefs->CommitPendingWrite();
  s.user_prefs.reset();
  s.local_state->CommitPendingWrite();
  s.local_state.reset();
  s.browser_context = nullptr;
}

void PostDestroyThreads() {
  extensions::ExtensionsBrowserClient::Set(nullptr);
  state().browser_client.reset();
}

void WebContentsCreated(WebContents* web_contents) {
  if (!state().browser_context)
    return;
  extensions::ShellExtensionWebContentsObserver::CreateForWebContents(
      web_contents);
  // Chrome's TabHelper gives every tab one. Without it declarativeNetRequest
  // never learns which frames committed where, and allowAllRequests rules
  // do nothing.
  web_contents->SetUserData(kDnrHelperKey,
                            std::make_unique<DnrHelperHolder>(web_contents));
  // A tab for chrome.tabs, with its events; its toolbar is filled once the
  // window exists, which is after this.
  extensions::ShellTabs::AttachTab(web_contents);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](base::WeakPtr<WebContents> tab) {
                       if (tab && Toolbar())
                         Toolbar()->PushTab(tab.get());
                     },
                     web_contents->GetWeakPtr()));
}

void RenderProcessWillLaunch(RenderProcessHost* host) {
  BrowserContext* context = host->GetBrowserContext();
  host->AddFilter(new extensions::ExtensionMessageFilter(host->GetID(), context));
  host->AddFilter(
      new extensions::MessagingAPIMessageFilter(host->GetID(), context));
}

void SiteInstanceGotProcess(SiteInstance* site_instance) {
  const extensions::Extension* extension = GetExtension(site_instance);
  if (!extension)
    return;
  extensions::ProcessMap::Get(site_instance->GetBrowserContext())
      ->Insert(extension->id(), site_instance->GetProcess()->GetID(),
               site_instance->GetId());
}

void SiteInstanceDeleting(SiteInstance* site_instance) {
  if (BrowserMainRunner::ExitedMainMessageLoop())
    return;
  const extensions::Extension* extension = GetExtension(site_instance);
  if (!extension)
    return;
  extensions::ProcessMap::Get(site_instance->GetBrowserContext())
      ->Remove(extension->id(), site_instance->GetProcess()->GetID(),
               site_instance->GetId());
}

void AppendExtraCommandLineSwitches(base::CommandLine* command_line,
                                    int child_process_id) {
  if (command_line->GetSwitchValueASCII(::switches::kProcessType) !=
      ::switches::kRendererProcess) {
    return;
  }
  static const char* const kSwitchNames[] = {
      extensions::switches::kAllowlistedExtensionID,
      extensions::switches::kExtensionProcess,
  };
  command_line->CopySwitchesFrom(*base::CommandLine::ForCurrentProcess(),
                                 kSwitchNames, std::size(kSwitchNames));
}

void ExposeInterfacesToRenderer(
    service_manager::BinderRegistry* registry,
    blink::AssociatedInterfaceRegistry* associated_registry,
    RenderProcessHost* render_process_host) {
  int id = render_process_host->GetID();
  associated_registry->AddInterface<extensions::mojom::EventRouter>(
      base::BindRepeating(&extensions::EventRouter::BindForRenderer, id));
  associated_registry->AddInterface<guest_view::mojom::GuestViewHost>(
      base::BindRepeating(&extensions::ExtensionsGuestView::CreateForComponents,
                          id));
  associated_registry->AddInterface<extensions::mojom::GuestView>(
      base::BindRepeating(&extensions::ExtensionsGuestView::CreateForExtensions,
                          id));
  associated_registry->AddInterface<extensions::mojom::RendererHost>(
      base::BindRepeating(&extensions::RendererStartupHelper::BindForRenderer,
                          id));
}

void RegisterAssociatedInterfaceBindersForRenderFrameHost(
    RenderFrameHost& render_frame_host,
    blink::AssociatedInterfaceRegistry& associated_registry) {
  associated_registry.AddInterface<extensions::mojom::LocalFrameHost>(
      base::BindRepeating(
          [](RenderFrameHost* render_frame_host,
             mojo::PendingAssociatedReceiver<extensions::mojom::LocalFrameHost>
                 receiver) {
            extensions::ExtensionWebContentsObserver::BindLocalFrameHost(
                std::move(receiver), render_frame_host);
          },
          &render_frame_host));
}

void RegisterBrowserInterfaceBindersForFrame(
    RenderFrameHost* render_frame_host,
    mojo::BinderMapWithContext<RenderFrameHost*>* map) {
  const GURL& site = render_frame_host->GetSiteInstance()->GetSiteURL();
  if (!site.SchemeIs(extensions::kExtensionScheme))
    return;
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(
          render_frame_host->GetProcess()->GetBrowserContext())
          ->enabled_extensions()
          .GetByID(site.host());
  if (!extension)
    return;
  extensions::ExtensionsBrowserClient::Get()
      ->RegisterBrowserInterfaceBindersForFrame(map, render_frame_host,
                                                extension);
}

void AddNavigationThrottles(
    NavigationHandle* navigation_handle,
    std::vector<std::unique_ptr<NavigationThrottle>>* throttles) {
  throttles->push_back(
      std::make_unique<extensions::ExtensionNavigationThrottle>(
          navigation_handle));
}

std::unique_ptr<NavigationUIData> GetNavigationUIData(
    NavigationHandle* navigation_handle) {
  return std::make_unique<extensions::ShellNavigationUIData>(navigation_handle);
}

void RegisterNonNetworkNavigationURLLoaderFactories(
    int frame_tree_node_id,
    ukm::SourceIdObj ukm_source_id,
    NonNetworkURLLoaderFactoryMap* factories) {
  WebContents* web_contents =
      WebContents::FromFrameTreeNodeId(frame_tree_node_id);
  factories->emplace(
      extensions::kExtensionScheme,
      extensions::CreateExtensionNavigationURLLoaderFactory(
          web_contents->GetBrowserContext(), ukm_source_id,
          !!extensions::WebViewGuest::FromWebContents(web_contents)));
}

void RegisterNonNetworkWorkerMainResourceURLLoaderFactories(
    BrowserContext* browser_context,
    NonNetworkURLLoaderFactoryMap* factories) {
  factories->emplace(
      extensions::kExtensionScheme,
      extensions::CreateExtensionWorkerMainResourceURLLoaderFactory(
          browser_context));
}

void RegisterNonNetworkServiceWorkerUpdateURLLoaderFactories(
    BrowserContext* browser_context,
    NonNetworkURLLoaderFactoryMap* factories) {
  factories->emplace(
      extensions::kExtensionScheme,
      extensions::CreateExtensionServiceWorkerScriptURLLoaderFactory(
          browser_context));
}

void RegisterNonNetworkSubresourceURLLoaderFactories(
    int render_process_id,
    int render_frame_id,
    NonNetworkURLLoaderFactoryMap* factories) {
  factories->emplace(extensions::kExtensionScheme,
                     extensions::CreateExtensionURLLoaderFactory(
                         render_process_id, render_frame_id));
}

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
    bool* bypass_redirect_checks) {
  // The webRequest proxy, which is also where declarativeNetRequest applies
  // its rules.
  auto* web_request_api =
      extensions::BrowserContextKeyedAPIFactory<extensions::WebRequestAPI>::Get(
          browser_context);
  bool use_proxy = web_request_api->MaybeProxyURLLoaderFactory(
      browser_context, frame, render_process_id, type, std::move(navigation_id),
      ukm_source_id, factory_receiver, header_client);
  if (bypass_redirect_checks)
    *bypass_redirect_checks = use_proxy;
  return use_proxy;
}

void OverrideURLLoaderFactoryParams(
    BrowserContext* browser_context,
    const url::Origin& origin,
    bool is_for_isolated_world,
    network::mojom::URLLoaderFactoryParams* factory_params) {
  extensions::URLLoaderFactoryManager::OverrideURLLoaderFactoryParams(
      browser_context, origin, is_for_isolated_world, factory_params);
}

}  // namespace haiku_extensions
}  // namespace content
