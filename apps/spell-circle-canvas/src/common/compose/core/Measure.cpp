/** @file
 * The one-shot verbs of Measure.h: what a tree or a run answers without a
 * live composer — the element-tree-as-a-brush bake, the intrinsic size, a
 * face's metrics, a run's advances and pens, and the two that solve a
 * style BACKWARDS from a size the drawing states.
 */

#include <include/core/SkBBHFactory.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkFont.h>
#include <include/core/SkFontMetrics.h>
#include <include/core/SkPicture.h>
#include <include/core/SkPictureRecorder.h>
#include <sigilweave/choreograph/Choreograph.h>  // forEachPlacedGlyph
#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/layout/ParagraphLayout.h>

#include <algorithm>
#include <cmath>

#include "ComposeRuntime.h"

namespace sigil::compose {

using namespace detail;

sk_sp<SkPicture> snapshot(const Element& root, sigil::weave::FontContext& fonts,
                          SkSize maxSize) {
  motion::Ticker ticker;  // inert: nothing steps it, transitions can't run
  Composer composer(ticker, fonts);
  Composer::Impl& impl = *composer.m_impl;
  impl.liveOnly = true;  // one-shot: per-node caches would be pure waste
  composer.render(root);
  if (!impl.root) return nullptr;
  if (!maxSize.isEmpty()) {
    if (maxSize.width() > 0)
      YGNodeStyleSetMaxWidth(impl.root->yoga, maxSize.width());
    if (maxSize.height() > 0)
      YGNodeStyleSetMaxHeight(impl.root->yoga, maxSize.height());
  }
  impl.ensureLayout();
  const SkRect rect = impl.instanceRect(*impl.root);
  if (rect.isEmpty()) return nullptr;
  SkPictureRecorder recorder;
  SkCanvas* canvas =
      recorder.beginRecording(SkRect::MakeWH(rect.width(), rect.height()));
  impl.paint(*impl.root, *canvas);
  return recorder.finishRecordingAsPicture();
}
TextMetrics metrics(const sigil::weave::TextStyle& style,
                    sigil::weave::FontContext& fonts) {
  SkFont font(
      style.shaping.typeface ? style.shaping.typeface : fonts.defaultTypeface(),
      style.shaping.fontSize);
  SkFontMetrics fm;
  font.getMetrics(&fm);
  TextMetrics out;
  out.ascent = -fm.fAscent;  // Skia reports ascent negative (above baseline)
  out.descent = fm.fDescent;
  out.leading = fm.fLeading > 0 ? fm.fLeading : 0.0f;
  // Some faces report zero for these; fall back to the conventional
  // fractions of the ascent rather than handing back a zero that reads as
  // a measurement.
  out.capHeight = fm.fCapHeight > 0 ? fm.fCapHeight : out.ascent * 0.72f;
  out.xHeight = fm.fXHeight > 0 ? fm.fXHeight : out.ascent * 0.52f;
  out.lineHeight = out.ascent + out.descent + out.leading;
  return out;
}

sigil::weave::TextStyle atCapHeight(sigil::weave::TextStyle style, float capPx,
                                    sigil::weave::FontContext& fonts) {
  if (!(capPx > 0.0f) || !(style.shaping.fontSize > 0.0f)) return style;
  const TextMetrics m = metrics(style, fonts);
  if (!(m.capHeight > 0.0f)) return style;
  style.shaping.fontSize *= capPx / m.capHeight;
  return style;
}

std::vector<float> measureRun(std::u8string_view utf8,
                              const sigil::weave::TextStyle& style,
                              sigil::weave::FontContext& fonts) {
  std::vector<float> advances;
  if (utf8.empty()) return advances;
  // The exact machinery a text() leaf runs (layoutText, Layout.cpp): one
  // Paragraph, one unconstrained single-line layout, the placed glyphs in
  // order. Only the Element is skipped — which is the point. A caller
  // placing glyphs by hand needs the advances, and the alternative to this
  // is a whole paragraph layout per glyph.
  sigil::weave::Paragraph paragraph;
  paragraph.appendText(utf8, style);
  static const sigil::weave::ParagraphLayoutOptions kOptions;
  sigil::weave::BlockFlow flow(SkRect::MakeWH(1.0e6f, 1.0e6f));
  sigil::weave::ParagraphLayout layout =
      sigil::weave::layoutParagraph(fonts, paragraph, flow, kOptions);
  // An inter-word space is a GAP the flow leaves between positioned runs,
  // not a glyph, so it visits nothing here. Left out, every glyph after a
  // space would be placed short by the accumulated space advances and the
  // error would grow with each word. Whatever the layout left between one
  // glyph's pen end and the next one's origin therefore rides the advance
  // of the glyph it follows, which is what makes the prefix sums reproduce
  // the pen positions the layout used.
  //
  // Two steps are not glue and are not folded: a line change (a '\n' in the
  // run) restarts the pen, and a BACKWARDS step between two words is
  // bidi reordering rather than a gap — visual order runs the other way
  // there and no prefix sum can express it. Inside one word a backwards
  // step is ordinary kerning and counts.
  float pen = 0;
  int lineIndex = -1;
  uint32_t wordIndex = 0;
  sigil::weave::forEachPlacedGlyph(
      layout, paragraph, [&](const sigil::weave::PlacedGlyph& placed) {
        const float step = placed.rest.x() - pen;
        if (placed.lineIndex != lineIndex) {
          lineIndex = placed.lineIndex;
        } else if (!advances.empty() &&
                   (placed.wordIndex == wordIndex || step > 0)) {
          advances.back() += step;
        }
        advances.push_back(placed.advance);
        pen = placed.rest.x() + placed.advance;
        wordIndex = placed.wordIndex;
      });
  return advances;
}

std::vector<float> runPens(std::u8string_view utf8,
                           const sigil::weave::TextStyle& style,
                           sigil::weave::FontContext& fonts) {
  const std::vector<float> advances = measureRun(utf8, style, fonts);
  std::vector<float> pens;
  pens.reserve(advances.size() + 1);
  float pen = 0;
  for (const float advance : advances) {
    pens.push_back(pen);
    pen += advance;
  }
  // The past-the-end entry, which is what makes the last advance readable
  // the same way every other one is and hands back the run's width for
  // free. An empty run reaches here with nothing summed and answers 0,
  // which is its width.
  pens.push_back(pen);
  return pens;
}

sigil::weave::TextStyle fitRun(std::u8string_view utf8,
                               sigil::weave::TextStyle style, float widthPx,
                               sigil::weave::FontContext& fonts,
                               const RunFit& fit) {
  if (utf8.empty() || !(widthPx > 0.0f)) return style;
  const float ceiling =
      fit.maxSize > 0.0f ? fit.maxSize : style.shaping.fontSize;
  if (!(ceiling > 0.0f)) return style;
  const float floorSize = std::clamp(fit.minSize, 0.01f, ceiling);
  const float widest =
      style.shaping.scaleX > 0.0f ? style.shaping.scaleX : 1.0f;
  style.shaping.scaleX = widest;

  auto widthAt = [&](float size) {
    style.shaping.fontSize = size;
    const std::vector<float> pens = runPens(utf8, style, fonts);
    return pens.empty() ? 0.0f : pens.back();
  };

  const float wide = widthAt(ceiling);
  if (!(wide > 0.0f)) return style;
  if (wide <= widthPx) return style;  // it already fits; nothing is resized

  // The width is AFFINE in the size, not proportional: the ink scales and
  // the tracking does not, because tracking is stated in px. So two
  // measurements identify the line and the size is read straight off it,
  // where a single ratio assumes the line passes through the origin and
  // overshoots by exactly the tracking the run carries.
  const float half = ceiling * 0.5f;
  const float narrow = widthAt(half);
  const float perSize = (wide - narrow) / (ceiling - half);
  const float fixed = wide - perSize * ceiling;
  float size = perSize > 0.0f ? (widthPx - fixed) / perSize : floorSize;
  float width = widthAt(std::clamp(size, floorSize, ceiling));
  if (width > widthPx) {
    // The line is the model, and a face whose advances do not scale
    // perfectly leaves a remainder. One correction closes it.
    size = std::clamp(style.shaping.fontSize * widthPx / width, floorSize,
                      ceiling);
    width = widthAt(size);
  }

  // The condense closes what the size floor left over, and only that: a
  // run that fitted by shrinking is never also squeezed.
  if (width > widthPx && fit.minCondense < widest)
    style.shaping.scaleX =
        std::clamp(widest * widthPx / width, fit.minCondense, widest);
  return style;
}

SkSize intrinsicSize(const Element& root, sigil::weave::FontContext& fonts,
                     SkSize maxSize) {
  motion::Ticker ticker;  // inert — same sampling rules as snapshot()
  Composer composer(ticker, fonts);
  Composer::Impl& impl = *composer.m_impl;
  impl.liveOnly = true;
  composer.render(root);
  if (!impl.root) return SkSize::MakeEmpty();
  if (!maxSize.isEmpty()) {
    if (maxSize.width() > 0)
      YGNodeStyleSetMaxWidth(impl.root->yoga, maxSize.width());
    if (maxSize.height() > 0)
      YGNodeStyleSetMaxHeight(impl.root->yoga, maxSize.height());
  }
  impl.ensureLayout();
  const SkRect rect = impl.instanceRect(*impl.root);
  return {rect.width(), rect.height()};
}

}  // namespace sigil::compose
