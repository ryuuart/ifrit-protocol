#pragma once

/** @file
 * The glyphs handed to Skia, captured without a device or rasterization.
 * Both text blobs and direct glyph draws reach the same recording hook.
 */

#include <include/utils/SkNoDrawCanvas.h>
#include <src/text/GlyphRun.h>

#include <vector>

namespace sigil::test {

struct GlyphDraw {
  SkGlyphID glyph;
  SkPoint position;
  SkFont font;
};

class GlyphCanvas final : public SkNoDrawCanvas {
 public:
  using SkNoDrawCanvas::SkNoDrawCanvas;
  std::vector<GlyphDraw> glyphs;

 protected:
  void onDrawTextBlob(const SkTextBlob* blob, SkScalar x, SkScalar y,
                      const SkPaint& paint) override {
    SkCanvas::onDrawTextBlob(blob, x, y, paint);
  }

  void onDrawPicture(const SkPicture* picture, const SkMatrix* matrix,
                     const SkPaint* paint) override {
    SkCanvas::onDrawPicture(picture, matrix, paint);
  }

  void onDrawGlyphRunList(const sktext::GlyphRunList& runs,
                          const SkPaint&) override {
    const SkMatrix matrix = getTotalMatrix();
    for (const sktext::GlyphRun& run : runs)
      for (size_t index = 0; index < run.runSize(); ++index)
        glyphs.push_back(
            {run.glyphsIDs()[index],
             matrix.mapPoint(runs.origin() + run.positions()[index]),
             run.font()});
  }
};

}  // namespace sigil::test
