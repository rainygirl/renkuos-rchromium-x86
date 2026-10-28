#!/usr/bin/env python3
"""Renderers drop root, as in the arm64 and x86_64 ports.

Until now the Haiku branch of content/renderer built the Fuchsia delegate:
three empty functions and an EnableSandbox() that returns true. The Haiku
delegate (files/content/renderer/renderer_main_platform_delegate_haiku.cc,
the arm64 port's) drops the renderer to uid/gid 65534 before it touches web
content, and on a kernel with restrict_syscalls() gives up every syscall
group but BASIC and FILE_READ. In --single-process there is no renderer
process and EnableSandbox() is never called.
"""
import os
import sys

root = sys.argv[1]
path = os.path.join(root, "content/renderer/BUILD.gn")
s = open(path).read()
old = ('  if (is_haiku) {\n'
       '    sources += [ "renderer_main_platform_delegate_fuchsia.cc" ]\n'
       '  }\n')
new = ('  if (is_haiku) {\n'
       '    sources += [ "renderer_main_platform_delegate_haiku.cc" ]\n'
       '  }\n')
if new in s:
    print("  renderer delegate: already applied")
elif s.count(old) == 1:
    open(path, "w").write(s.replace(old, new))
    print("  renderer delegate: applied")
else:
    print("  renderer delegate: PATTERN NOT FOUND")
