// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/common/extensions/shell_extensions_api_provider.h"

#include "content/shell/common/extensions/api/api_features.h"
#include "content/shell/common/extensions/api/generated_schemas.h"
#include "content/shell/common/extensions/api/manifest_features.h"
#include "content/shell/common/extensions/api/permission_features.h"
#include "extensions/common/alias.h"
#include "extensions/common/mojom/api_permission_id.mojom-shared.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_info.h"

namespace extensions {

namespace {

using mojom::APIPermissionID;

// As chrome/common/extensions/permissions/chrome_api_permissions.cc
// registers them.
constexpr APIPermissionInfo::InitInfo kPermissionsToRegister[] = {
    {APIPermissionID::kCookie, "cookies"},
    {APIPermissionID::kScripting, "scripting",
     APIPermissionInfo::kFlagRequiresManagementUIWarning},
    {APIPermissionID::kTab, "tabs",
     APIPermissionInfo::kFlagRequiresManagementUIWarning},
    {APIPermissionID::kWebNavigation, "webNavigation",
     APIPermissionInfo::kFlagRequiresManagementUIWarning},
};

constexpr Alias kPermissionAliases[] = {Alias("windows", "tabs")};

}  // namespace

RChExtensionsAPIProvider::RChExtensionsAPIProvider() = default;
RChExtensionsAPIProvider::~RChExtensionsAPIProvider() = default;

void RChExtensionsAPIProvider::AddAPIFeatures(FeatureProvider* provider) {
  AddRChAPIFeatures(provider);
}

void RChExtensionsAPIProvider::AddManifestFeatures(
    FeatureProvider* provider) {
  // "action": core has its handler but, in 114, not its feature.
  AddRChManifestFeatures(provider);
}

void RChExtensionsAPIProvider::AddPermissionFeatures(
    FeatureProvider* provider) {
  AddRChPermissionFeatures(provider);
}

void RChExtensionsAPIProvider::AddBehaviorFeatures(
    FeatureProvider* provider) {}

void RChExtensionsAPIProvider::AddAPIJSONSources(
    JSONFeatureProviderSource* json_source) {
  // Only ExtensionsClient::CreateAPIFeatureSource reads these, and nothing
  // calls it; the compiled features above are what is used.
}

bool RChExtensionsAPIProvider::IsAPISchemaGenerated(
    const std::string& name) {
  return api::RChGeneratedSchemas::IsGenerated(name);
}

base::StringPiece RChExtensionsAPIProvider::GetAPISchema(
    const std::string& name) {
  return api::RChGeneratedSchemas::Get(name);
}

void RChExtensionsAPIProvider::RegisterPermissions(
    PermissionsInfo* permissions_info) {
  permissions_info->RegisterPermissions(kPermissionsToRegister,
                                        kPermissionAliases);
}

void RChExtensionsAPIProvider::RegisterManifestHandlers() {
  // The "action" handler (ExtensionActionHandler) is registered by core.
}

}  // namespace extensions
