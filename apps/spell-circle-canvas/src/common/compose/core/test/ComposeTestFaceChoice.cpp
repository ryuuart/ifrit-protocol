// The face an italic chooses over the face in force: where that face is
// its family's own, the family is asked again by the name the font service
// answers for the face, and its italic face answers. The case asks the
// machine's Georgia, so its suite carries the `fonts` label.

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkFontStyle.h>
#include <include/core/SkSurface.h>
#include <sigilcompose/core/FontStyle.h>
#include <sigilweave/fonts/FamilyName.h>

#include <vector>

#include "sigilweave/advanced/Skia.h"
#include "support/CoreTestSupport.h"

namespace {

using sigil::compose::FontStyle;

/** @p page rendered on a host of its own and read back. */
std::vector<uint32_t> render(const Element& page) {
  Host host(320, 80);
  host.composer.render(page);
  host.frame();
  SkBitmap bitmap;
  bitmap.allocPixels(host.surface->imageInfo());
  host.surface->readPixels(bitmap.pixmap(), 0, 0);
  const auto* first = static_cast<const uint32_t*>(bitmap.getPixels());
  return {first, first + bitmap.width() * bitmap.height()};
}

/** White words with a figure of every height, so two faces never draw
 *  them alike. */
Text words() {
  return text(u8"Hamburgefonstiv").fontSize(28).ink({1, 1, 1, 1}).key("t");
}

TEST(ComposeFontStyle, AnItalicOverAFamilysOwnFaceIsTheFamilysItalic) {
  const sk_sp<SkTypeface> upright = fonts().familyTypeface(
      "Georgia", sigil::weave::fromSk(SkFontStyle::Normal()));
  ASSERT_TRUE(upright);
  // The family the italic is asked of is the one the font service names.
  const sk_sp<SkTypeface> italicFace =
      fonts().familyTypeface(sigil::weave::fonts::familyName(*upright),
                             sigil::weave::fromSk(SkFontStyle::Italic()));
  ASSERT_TRUE(italicFace);
  ASSERT_NE(italicFace->fontStyle().slant(), SkFontStyle::kUpright_Slant);
  const auto italic = render(box()
                                 .font({.face = upright})
                                 .fontStyle(FontStyle::Italic)
                                 .children({words()}));
  EXPECT_EQ(italic,
            render(box().font({.face = italicFace}).children({words()})));
  EXPECT_NE(italic, render(box().font({.face = upright}).children({words()})));
}

}  // namespace
