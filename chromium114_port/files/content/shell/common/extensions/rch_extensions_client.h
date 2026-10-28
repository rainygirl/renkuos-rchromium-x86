// Copyright 2026 The RENKU project
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_COMMON_EXTENSIONS_RCH_EXTENSIONS_CLIENT_H_
#define CONTENT_SHELL_COMMON_EXTENSIONS_RCH_EXTENSIONS_CLIENT_H_

#include <memory>

#include "content/shell/common/extensions/shell_extensions_api_provider.h"
#include "extensions/shell/common/shell_extensions_client.h"
#include "url/gurl.h"

namespace extensions {

// app_shell's ExtensionsClient, with the //chrome APIs this port implements,
// and one answer changed: app_shell calls every URL a blocklist update URL
// ("for now we accept any URL ... because we don't really care"), and 114's
// WebRequestPermissions::HideRequest() hides those from webRequest and
// declarativeNetRequest -- which, with every URL one, is every request.
class RChExtensionsClient : public ShellExtensionsClient {
 public:
  RChExtensionsClient() {
    AddAPIProvider(std::make_unique<RChExtensionsAPIProvider>());
  }

  bool IsBlocklistUpdateURL(const GURL& url) const override { return false; }
};

}  // namespace extensions

#endif  // CONTENT_SHELL_COMMON_EXTENSIONS_RCH_EXTENSIONS_CLIENT_H_
