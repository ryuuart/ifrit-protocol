#pragma once

/** @file
 * The faces a case asks for before it can ask its own question: the two
 * lookups into whatever this machine has installed, and the constructed
 * instrument committed beside the tests. The font context itself is the
 * whole test process's, not this library's.
 */

#include <include/core/SkFontMgr.h>
#include <include/core/SkFontStyle.h>
#include <include/core/SkTypeface.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <Fonts.h>

namespace sigil::weave::test {

/// The installed face for `family`, or null when this machine has none.
/// A null answer is the caller's cue to skip and say which family it
/// wanted: GTEST_SKIP returns from the function it is written in, so only
/// the case itself can skip the case.
inline sk_sp<SkTypeface> installedFace(const char* family) {
  return sigil::test::fonts().fontManager()->matchFamilyStyle(
      family, SkFontStyle::Normal());
}

/// The installed face for `family` when it carries variation axes, else
/// null — the question a case about axes has to settle before it can ask
/// anything else, since a static face answers every coordinate the same.
inline sk_sp<SkTypeface> installedVariableFace(const char* family) {
  sk_sp<SkTypeface> face = installedFace(family);
  if (!face || face->getVariationDesignPosition({}) < 1) return nullptr;
  return face;
}

/// The constructed face committed under test/assets/. It is Latin-only,
/// carries a vertical feature per visible consequence, and is the same
/// face on every machine, so a question asked of it is asked identically
/// everywhere. Null only when the asset failed to load.
inline sk_sp<SkTypeface> instrumentFace() {
  static sk_sp<SkTypeface> face = ports::systemFontManager()->makeFromFile(
      SIGIL_TEST_ASSET_DIR "/VerticalFeatures.ttf");
  return face;
}

}  // namespace sigil::weave::test
