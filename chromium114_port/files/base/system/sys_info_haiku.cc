// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Haiku's answer to sysctl and /proc/meminfo is get_system_info(), which
// reports page counts for the whole machine in one call.

#include "base/system/sys_info.h"

#include <OS.h>

#include "base/cpu.h"
#include "base/notreached.h"

namespace base {

namespace {

uint64_t AmountOfMemory(bool available) {
  system_info info;
  if (get_system_info(&info) != B_OK) {
    NOTREACHED();
    return 0;
  }
  const uint64_t pages =
      available ? (info.max_pages - info.used_pages) : info.max_pages;
  return pages * static_cast<uint64_t>(B_PAGE_SIZE);
}

}  // namespace

// static
uint64_t SysInfo::AmountOfPhysicalMemoryImpl() {
  return AmountOfMemory(/*available=*/false);
}

// static
uint64_t SysInfo::AmountOfAvailablePhysicalMemoryImpl() {
  // Haiku reports pages in use rather than a reclaimable figure, so this is
  // the conservative reading: what is free now, not what could be freed.
  return AmountOfMemory(/*available=*/true);
}

// static
std::string SysInfo::CPUModelName() {
  // CPUID's brand string, which is what the Linux branch reads out of
  // /proc/cpuinfo: "Intel(R) Atom(TM) CPU Z520 @ 1.33GHz" on the VAIO P.
  return CPU().cpu_brand();
}

}  // namespace base
