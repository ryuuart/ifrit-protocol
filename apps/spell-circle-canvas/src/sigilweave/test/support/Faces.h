#pragma once

/** @file
 * The face this library commits for itself. The instruments every library
 * shares live in <Fonts.h> under `sigil::test::instrument`, and so does the
 * font context, which is the whole test tree's rather than this library's.
 */

#include <Fonts.h>
#include <include/core/SkTypeface.h>
#include <sigilweave/ports/SystemFontManager.h>

namespace sigil::weave::test {

/// The face committed under this library's test/assets/. Latin-only, and
/// carrying one vertical OpenType feature per visible consequence, which no
/// real CJK face offers: there the same substitution hangs off several tags
/// at once, so a shaped run cannot name the feature that ran. Null only
/// when the asset failed to load.
inline sk_sp<SkTypeface> verticalFeaturesFace() {
  static sk_sp<SkTypeface> face = ports::systemFontManager()->makeFromFile(
      SIGIL_TEST_ASSET_DIR "/VerticalFeatures.ttf");
  return face;
}

}  // namespace sigil::weave::test
