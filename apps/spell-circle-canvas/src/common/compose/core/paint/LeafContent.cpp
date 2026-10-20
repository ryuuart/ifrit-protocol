/** @file
 * paintLeafContent: what a leaf draws of its own inside its box — a text
 * leaf's passage with its echoes and the readings beside it, an image
 * leaf's picture placed by its fit, a custom leaf's program.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkPaint.h>
#include <include/core/SkRect.h>
#include <include/core/SkSamplingOptions.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilmedia/advanced/Device.h>
#include <sigilmedia/advanced/Skia.h>
#include <sigilmedia/core/PixelSource.h>
#include <src/core/SkScopeExit.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <optional>

#include "PaintInternal.h"
#include "PenOnCanvas.h"
#include "runtime/ComposeRuntime.h"

namespace sigil::compose {

using namespace detail;

namespace {

/** WHERE A PICTURE MEETS ITS LEAF'S BOX: the part of the source drawn and
 *  the rect it is drawn into. `Cover` draws the centred crop of the source
 *  that covers the box, `Contain` draws the whole source at the one scale
 *  that fits, centred with an equal margin on either side, and `Stretch`
 *  and `Native` draw the whole source into the whole box. A box within half
 *  a unit of the source's shape is that shape, which is what the layout
 *  gives a free leaf, and takes the whole source into the whole box. */
struct ImagePlacement {
  SkRect source;
  SkRect destination;
};

ImagePlacement placeImage(const SkRect& source, const SkRect& box,
                          material::Fit fit) {
  const ImagePlacement whole{source, box};
  if (fit != material::Fit::Cover && fit != material::Fit::Contain)
    return whole;
  if (source.isEmpty() || box.isEmpty()) return whole;
  const float scaleAcross = box.width() / source.width();
  const float scaleDown = box.height() / source.height();
  const float scale = fit == material::Fit::Cover
                          ? std::max(scaleAcross, scaleDown)
                          : std::min(scaleAcross, scaleDown);
  const float fittedWidth = source.width() * scale;
  const float fittedHeight = source.height() * scale;
  if (std::abs(fittedWidth - box.width()) < 0.5f &&
      std::abs(fittedHeight - box.height()) < 0.5f)
    return whole;
  if (fit == material::Fit::Contain)
    return {source,
            SkRect::MakeXYWH(box.x() + (box.width() - fittedWidth) * 0.5f,
                             box.y() + (box.height() - fittedHeight) * 0.5f,
                             fittedWidth, fittedHeight)};
  const float croppedWidth = box.width() / scale;
  const float croppedHeight = box.height() / scale;
  return {
      SkRect::MakeXYWH(source.x() + (source.width() - croppedWidth) * 0.5f,
                       source.y() + (source.height() - croppedHeight) * 0.5f,
                       croppedWidth, croppedHeight),
      box};
}

}  // namespace

void paintLeafContent(Composer::Impl& impl, Instance& inst, SkCanvas& canvas,
                      const PaintContext& paintContext, const SkRect& bounds) {
  const ElementNode& node = inst.painted();
  switch (node.kind) {
    case Kind::Text:
      if (inst.paragraph) {
        const bool onPathRun = node.textData && node.textData->onPath;
        const TextPath* onPath = nullptr;
        if (onPathRun) {
          const std::optional<TextPath>& path = node.textData->onPath;
          if (path.has_value()) onPath = &path.value();
        }
        // ink(paint)/textStroke() resolve to ONE glyph-paint override,
        // and every draw that takes one takes the SAME one: a letter in
        // flight, and a letter on a curve, are painted exactly as a
        // resting letter is.
        const detail::TextInk ink = impl.textInkOf(inst, paintContext);
        // One draw for both: the baseline places the glyph and the tracks
        // deviate from that placement. Neither wins over the other.
        // Dressed type draws through the painter its description carries;
        // a description that dresses its text without one (built from the
        // data blocks directly, never through a text verb) draws at rest.
        const TextPainterOperations* painter =
            Composer::Impl::textPainterOf(inst);
        const TextPainterOperations* engine =
            painter ? painter : detail::registeredTextEngine();
        // Misprint echoes of the TEXT, under the real pass and placed as
        // it is: along the path for a run laid on one, level otherwise.
        // textFx() text draws its own buckets — echoes skip it by
        // contract.
        if (!echoesOf(node).empty() && !hasTextFx(inst)) {
          for (const Echo& e : echoesOf(node)) {
            detail::TextInk stamp;
            stamp.passage.emplace();
            stamp.passage->foreground.setColor4f(
                material::skia::toSkColor(e.color), nullptr);
            canvas.save();
            canvas.translate(e.offset.x, e.offset.y);
            if (onPath && painter)
              painter->paint(inst, canvas, stamp, onPath,
                             {bounds.width(), bounds.height()}, paintContext);
            else
              inst.textLayout.drawBatched(&canvas, *inst.paragraph,
                                          stamp.override());
            canvas.restore();
          }
        }
        if ((hasTextFx(inst) || onPath) && painter) {
          painter->paint(inst, canvas, ink, onPath,
                         {bounds.width(), bounds.height()}, paintContext);
        } else if (ink.hasGlyphStyles() && engine) {
          // Span sources bind for this destination; restarting inks
          // also map onto each unit. Both use the glyph-style seam.
          static thread_local detail::GlyphInk glyphs;
          const SkScopeExit releaseGlyphs([&] { glyphs.clear(); });
          engine->glyphInk(inst, ink, glyphs);
          inst.textLayout.drawBatched(
              &canvas, *inst.paragraph,
              sigil::weave::ParagraphLayout::GlyphStyles{glyphs.styleOfGlyph,
                                                         glyphs.styles},
              ink.override());
        } else {
          inst.textLayout.drawBatched(&canvas, *inst.paragraph, ink.override());
        }
        // The readings beside the type. They stand AT REST while the
        // letters move, as a mark and a band do: a reading that travelled
        // with a cascade would be reading a letter that had left.
        for (const Instance::PlacedAnnotation& reading : inst.textAnnotations)
          if (reading.paragraph)
            reading.layout.drawBatched(&canvas, *reading.paragraph);
      }
      break;
    case Kind::Image:
      if (const sigil::media::PixelSource* source = imageSourceOf(node)) {
        // A frame standing on a device is bound for the destination
        // recorder, including when this canvas records a picture.
        const sk_sp<SkImage> picture = sigil::media::deviceImage(
            source->frameAt(std::chrono::duration<double>(impl.elapsed())),
            paintContext.recorder);
        if (picture) {
          const SkSamplingOptions sampling =
              inst.sampling.value_or(SkSamplingOptions{SkFilterMode::kLinear});
          // A stated region is the source the fit is applied to, and
          // sampling stays inside it; a crop of the whole picture may
          // sample the picture beyond the crop, as its own neighbours.
          const std::optional<SkRect>& region = node.imageData->region;
          const ImagePlacement placed =
              placeImage(region.value_or(SkRect::Make(picture->dimensions())),
                         bounds, node.imageData->fit);
          if (region)
            canvas.drawImageRect(picture, placed.source, placed.destination,
                                 sampling, nullptr,
                                 SkCanvas::kStrict_SrcRectConstraint);
          else if (placed.source == SkRect::Make(picture->dimensions()))
            canvas.drawImageRect(picture, placed.destination, sampling);
          else
            canvas.drawImageRect(picture, placed.source, placed.destination,
                                 sampling, nullptr,
                                 SkCanvas::kFast_SrcRectConstraint);
        }
      }
      break;
    case Kind::Custom:
      if (node.customData && node.customData->program) {
        detail::PenOnCanvas programPen(canvas, paintContext);
        node.customData->program(programPen.pen(), paintContext);
      }
      break;
    case Kind::Box:
    case Kind::Stack:
    case Kind::Slot:
      break;
  }
}

}  // namespace sigil::compose
