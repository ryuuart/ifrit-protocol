#pragma once

/** @file
 * Platform system-font-manager factory — the one place TextFlow's tools,
 * tests, and consumers obtain an SkFontMgr wired to the host operating
 * system. Every platform port hides behind the same call, so adding
 * DirectWrite (Windows) or Fontconfig (Linux) later touches only
 * SystemFontManager.cpp, never a call site.
 */

#include <include/core/SkRefCnt.h>

class SkFontMgr;

namespace textflow::ports {

/**
 * Returns the process-wide system font manager (CoreText on macOS).
 *
 * Construction enumerates the installed font set, which is far too slow to
 * repeat per call site, so one shared instance is created lazily and reused
 * for the life of the process. The returned manager is immutable and safe
 * to hand to any number of FontContexts on any thread.
 */
sk_sp<SkFontMgr> systemFontManager();

} // namespace textflow::ports
