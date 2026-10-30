#!/usr/bin/env python3
"""Give reCAPTCHA longer than 15 seconds on slow machines.

reCAPTCHA's frames and its worker give each other 10 to 15 seconds to answer.
On the Sony VAIO P's Atom the worker's first answer takes about 20, so the page
gets "reCAPTCHA Timeout" and the checkbox spins for ever (Slack's sign-in, for
one). At the start of every document this wraps setTimeout so that reCAPTCHA's
own timers of that length run six times as long: in its frames (/recaptcha/
paths) all of them, elsewhere only those set from a /recaptcha/ script.
Nothing else is touched.

An extension content script would do the same, but content scripts crash this
build's renderer. Runs in the page's own script world, before its scripts.

Usage: port-recaptcha-time.py <chromium src>. Idempotent.
"""
import sys

src = sys.argv[1]
cc = "%s/content/shell/renderer/shell_content_renderer_client.cc" % src
s = open(cc).read()
if "kRecaptchaTime" in s:
    print("  shell_content_renderer_client.cc: already applied")
    raise SystemExit

old = ("void ShellContentRendererClient::RunScriptsAtDocumentStart(\n"
       "    RenderFrame* render_frame) {\n"
       "  haiku_extensions::RunScriptsAtDocumentStart(render_frame);\n"
       "}\n")
assert s.count(old) == 1, s.count(old)
s = s.replace(old, (
    "// See port-recaptcha-time.py.\n"
    "const char16_t kRecaptchaTime[] =\n"
    "    u\"(() => {\"\n"
    "    u\"  const inRecaptcha = location.pathname.startsWith('/recaptcha/');\"\n"
    "    u\"  const setTimeout = window.setTimeout;\"\n"
    "    u\"  window.setTimeout = function (callback, delay, ...rest) {\"\n"
    "    u\"    if (delay >= 10000 && delay <= 15000 &&\"\n"
    "    u\"        (inRecaptcha || /\\\\/recaptcha\\\\//.test(new Error().stack))) {\"\n"
    "    u\"      delay *= 6;\"\n"
    "    u\"    }\"\n"
    "    u\"    return setTimeout.call(this, callback, delay, ...rest);\"\n"
    "    u\"  };\"\n"
    "    u\"})();\";\n"
    "\n"
    "void ShellContentRendererClient::RunScriptsAtDocumentStart(\n"
    "    RenderFrame* render_frame) {\n"
    "  haiku_extensions::RunScriptsAtDocumentStart(render_frame);\n"
    "  render_frame->ExecuteJavaScript(kRecaptchaTime);\n"
    "}\n"), 1)
open(cc, "w").write(s)
print("  shell_content_renderer_client.cc: reCAPTCHA time")
