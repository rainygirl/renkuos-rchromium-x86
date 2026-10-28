// Copyright 2026 The RENKU project
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// What the extension code (haiku_extensions.cc) needs from the platform
// delegate's toolbar: which native window a tab is in, and a way to open the
// next window without a toolbar, which is what an extension's popup is.

#ifndef CONTENT_SHELL_BROWSER_HAIKU_TOOLBAR_H_
#define CONTENT_SHELL_BROWSER_HAIKU_TOOLBAR_H_

#include "ui/gfx/native_widget_types.h"

namespace content {

class WebContents;

// The widget of the window showing `web_contents` with a toolbar, or
// kNullAcceleratedWidget.
gfx::AcceleratedWidget GetHaikuToolbarWidget(WebContents* web_contents);

// The next Shell window opens without a toolbar.
void SetNextHaikuWindowWithoutToolbar();

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_HAIKU_TOOLBAR_H_
