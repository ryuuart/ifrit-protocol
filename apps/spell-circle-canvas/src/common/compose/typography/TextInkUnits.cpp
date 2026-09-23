/** @file
 * THE INK RESTARTED ON EACH UNIT. An ink that names a unit of its
 * passage — a glyph, a cluster, a word, a line, a sentence — paints every
 * such unit afresh, its unit square on the text-metric box of that unit:
 * across the unit's advances, from the cap top down to the baseline. One
 * body for both draws that ask: the kernel's draw at rest and the dressed
 * painter's, each handing over the layout it draws and the poses it places
 * the glyphs by, so a letter on a curve gets the box it stands in.
 */

#include <include/core/SkFontMetrics.h>
#include <include/core/SkMatrix.h>
#include <sigilweave/choreograph/Choreograph.h>  // forEachPlacedGlyph
#include <sigilweave/fonts/Shaper.h>             // makeFont — the cap height

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

#include "TextEngine.h"
#include "TextPose.h"

namespace sigil::compose {

namespace {

/** The height of a face's capitals above its baseline, where the face
 *  reports one, else its ascent. Memoized per (face, size) across a walk,
 *  as the glyph band is. */
float capHeightOf(const sigil::weave::ShapedWord* shaped,
                  std::vector<std::pair<BandKey, float>>& memo) {
  if (!shaped || !shaped->typeface) return 0.0f;
  const BandKey key{shaped->typeface.get(), shaped->fontSize};
  for (const auto& [seen, height] : memo)
    if (seen == key) return height;
  SkFontMetrics metrics;
  sigil::weave::makeFont(shaped->typeface, shaped->fontSize)
      .getMetrics(&metrics);
  const float height =
      metrics.fCapHeight > 0 ? metrics.fCapHeight : -metrics.fAscent;
  memo.emplace_back(key, height);
  return height;
}

/** One glyph's text-metric box as its pose places it, as an axis-aligned
 *  bound: across its advance and from the cap top down to the baseline,
 *  turned with the glyph where the layout turned it. An upright glyph in
 *  a column has no cap band across it: it is one em across the column and
 *  its advance down it. */
SkRect inkBoxOf(const sigil::weave::PlacedGlyph& placed, const RestPose& pose,
                float capHeight) {
  const bool upright = placed.shaped && placed.shaped->vertical;
  const float size = placed.shaped ? placed.shaped->fontSize : 0.0f;
  const float half = std::abs(placed.advance) * 0.5f;
  const float x0 = upright ? -size * 0.5f : -half;
  const float x1 = upright ? size * 0.5f : half;
  const float y0 = upright ? -half : -capHeight;
  const float y1 = upright ? half : 0.0f;
  SkRect box = SkRect::MakeEmpty();
  bool first = true;
  for (const SkPoint corner :
       {SkPoint{x0, y0}, SkPoint{x1, y0}, SkPoint{x1, y1}, SkPoint{x0, y1}}) {
    const SkPoint at{
        pose.centre.x() + corner.x() * pose.cosine - corner.y() * pose.sine,
        pose.centre.y() + corner.x() * pose.sine + corner.y() * pose.cosine};
    if (first) {
      box = SkRect::MakeLTRB(at.x(), at.y(), at.x(), at.y());
      first = false;
    } else {
      box.fLeft = std::min(box.fLeft, at.x());
      box.fTop = std::min(box.fTop, at.y());
      box.fRight = std::max(box.fRight, at.x());
      box.fBottom = std::max(box.fBottom, at.y());
    }
  }
  return box;
}

/** One run of glyphs one ink paints afresh: the style it starts from, the
 *  paint on the unit square, and the box the glyphs cover. */
struct UnitGroup {
  const sigil::weave::PaintStyle* base = nullptr;
  sk_sp<SkShader> unitSquare;
  SkRect box = SkRect::MakeEmpty();
  bool placed = false;
};

}  // namespace

void detail::inkByUnit(const sigil::weave::ParagraphLayout& layout,
                       const Instance& inst, const GlyphStructure& structure,
                       const PoseContext& poses, const TextInk& ink,
                       GlyphInk& glyphs) {
  glyphs.styles.clear();
  glyphs.styleOfGlyph.assign(structure.glyphs.size(), GlyphInk::kOwnPaint);
  if (!inst.paragraph) return;
  const std::vector<SpanRestyle>* spans =
      inst.description && inst.description->textData
          ? &inst.description->textData->spanRestyles
          : nullptr;
  static thread_local std::vector<UnitGroup> groups;
  static thread_local std::vector<std::pair<BandKey, float>> caps;
  groups.clear();
  caps.clear();

  // A unit is a run of glyphs one ink restarts on, so a glyph opens a new
  // one where the ink painting it changes or its unit does. The leaf's
  // own ink reaches every glyph; a span's reaches the glyphs whose paint
  // is still its shader, which is exactly the text a later span left it.
  constexpr uint32_t kNone = ~0u;
  uint32_t openInk = kNone, openUnit = kNone;
  sigil::weave::forEachPlacedGlyph(
      layout, *inst.paragraph, [&](const sigil::weave::PlacedGlyph& placed) {
        const uint32_t ordinal = placed.ordinal;
        if (ordinal >= structure.glyphs.size()) return;
        uint32_t whose = kNone;
        const sigil::weave::PaintStyle* base = nullptr;
        const sk_sp<SkShader>* unitSquare = nullptr;
        sigil::weave::Unit unit = ink.unit;
        if (ink.unitSquare && ink.passage) {
          whose = 0;
          base = &*ink.passage;
          unitSquare = &ink.unitSquare;
        } else if (ink.spanUnits && spans) {
          const SkShader* painted = placed.paint->foreground.getShader();
          for (size_t index = spans->size(); painted && index-- > 0;) {
            const SpanRestyle& span = (*spans)[index];
            const std::optional<sigil::weave::Unit> spanUnit =
                textUnitOf(span.inkBox);
            if (!spanUnit || !span.inkShader ||
                span.inkShader->shaderValue.get() != painted)
              continue;
            whose = (uint32_t)index + 1;
            base = placed.paint;
            unitSquare = &span.inkShader->shaderValue;
            unit = *spanUnit;
            break;
          }
        }
        if (!base || (size_t)unit >= GlyphStructure::kUnits) {
          openInk = kNone;
          return;
        }
        const uint32_t unitIndex = structure.unitOf[(size_t)unit][ordinal];
        if (whose != openInk || unitIndex != openUnit) {
          groups.push_back({base, *unitSquare});
          openInk = whose;
          openUnit = unitIndex;
        }
        UnitGroup& group = groups.back();
        RestPose pose;
        if (restPoseOf(poses, placed, pose)) {
          const SkRect box =
              inkBoxOf(placed, pose, capHeightOf(placed.shaped, caps));
          if (group.placed)
            group.box.join(box);
          else
            group.box = box;
          group.placed = true;
        }
        glyphs.styleOfGlyph[ordinal] = (uint32_t)groups.size() - 1;
      });

  glyphs.styles.reserve(groups.size());
  for (const UnitGroup& group : groups) {
    sigil::weave::PaintStyle style = *group.base;
    const SkRect box = group.placed ? group.box : SkRect::MakeWH(1, 1);
    SkMatrix map = SkMatrix::Translate(box.left(), box.top());
    map.preScale(std::max(box.width(), 1.0f), std::max(box.height(), 1.0f));
    style.foreground.setShader(group.unitSquare->makeWithLocalMatrix(map));
    glyphs.styles.push_back(std::move(style));
  }
}

void detail::inkAtRestByUnit(Instance& inst, const TextInk& ink,
                             GlyphInk& glyphs) {
  glyphs.styles.clear();
  glyphs.styleOfGlyph.clear();
  if (!inst.paragraph) return;
  static thread_local GlyphStructure structure;
  structure.build(inst.textLayout, *inst.paragraph, scopeOf(inst));
  const PoseContext poses{.inst = &inst, .layout = &inst.textLayout};
  inkByUnit(inst.textLayout, inst, structure, poses, ink, glyphs);
}

}  // namespace sigil::compose
