#!/usr/bin/env python3
"""--user-agent=<string> for content_shell 114, as Chrome has it.

content_shell 114 always answers its own user agent (Chrome/114.0.5735.199,
see out/haiku-x86/args.gn) and ignores --user-agent. Slack's server refuses
anything older than Chrome 138 with "Please update your browser", so R Slack
runs its window with a newer version in the user agent. Only the window that
asks for it is affected; the browser itself still tells the truth.

Usage: port-shell-ua-switch.py <chromium src>. Idempotent.
"""
import sys

src = sys.argv[1]
cc = "%s/content/shell/browser/shell_content_browser_client.cc" % src
s = open(cc).read()
if "CommandLineUserAgent()" in s:
    print("  shell_content_browser_client.cc: already applied")
    raise SystemExit

for name, helper in (("GetUserAgent", "GetShellUserAgent"),
                     ("GetFullUserAgent", "GetShellFullUserAgent"),
                     ("GetReducedUserAgent", "GetShellReducedUserAgent")):
    old = ("std::string ShellContentBrowserClient::%s() {\n"
           "  return %s();\n"
           "}\n" % (name, helper))
    assert s.count(old) == 1, (name, s.count(old))
    s = s.replace(old, (
        "std::string ShellContentBrowserClient::%s() {\n"
        "#if BUILDFLAG(IS_HAIKU)\n"
        "  const std::string custom = CommandLineUserAgent();\n"
        "  if (!custom.empty()) {\n"
        "    return custom;\n"
        "  }\n"
        "#endif\n"
        "  return %s();\n"
        "}\n" % (name, helper)), 1)

anchor = "std::string ShellContentBrowserClient::GetUserAgent() {\n"
s = s.replace(anchor, (
    "#if BUILDFLAG(IS_HAIKU)\n"
    "// --user-agent=<string>, as Chrome takes it; empty when absent or when the\n"
    "// value could not be a header (a line break would end it early).\n"
    "static std::string CommandLineUserAgent() {\n"
    "  const std::string ua =\n"
    "      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(\n"
    "          \"user-agent\");\n"
    "  if (ua.find_first_of(\"\\r\\n\") != std::string::npos) {\n"
    "    return std::string();\n"
    "  }\n"
    "  return ua;\n"
    "}\n"
    "#endif\n"
    "\n") + anchor, 1)
open(cc, "w").write(s)
print("  shell_content_browser_client.cc: --user-agent")
