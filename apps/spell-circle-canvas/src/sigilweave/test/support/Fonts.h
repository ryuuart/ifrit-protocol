#pragma once

/** @file
 * The one font context every weave test binary shapes with, and the two
 * lookups a case makes before it can ask a question about a particular
 * face. Building a system font manager enumerates the installed font set,
 * so each process constructs it exactly once and every test shares the
 * result.
 */

#include <include/core/SkFontMgr.h>
#include <include/core/SkFontStyle.h>
#include <include/core/SkTypeface.h>
#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/ports/SystemFontManager.h>

namespace sigil::weave::test {

/// The process-wide FontContext over the system font manager.
inline FontContext& sharedContext() {
  // systemFontManager() shares one enumerated font set process-wide.
  static auto* fontContext = new FontContext(ports::systemFontManager());
  return *fontContext;
}

/// The installed face for `family`, or null when this machine has none.
/// A null answer is the caller's cue to skip and say which family it
/// wanted: GTEST_SKIP returns from the function it is written in, so only
/// the case itself can skip the case.
inline sk_sp<SkTypeface> installedFace(const char* family) {
  return sharedContext().fontManager()->matchFamilyStyle(family,
                                                         SkFontStyle::Normal());
}

/// The installed face for `family` when it carries variation axes, else
/// null — the question a case about axes has to settle before it can ask
/// anything else, since a static face answers every coordinate the same.
inline sk_sp<SkTypeface> installedVariableFace(const char* family) {
  sk_sp<SkTypeface> face = installedFace(family);
  if (!face || face->getVariationDesignPosition({}) < 1) return nullptr;
  return face;
}

}  // namespace sigil::weave::test
