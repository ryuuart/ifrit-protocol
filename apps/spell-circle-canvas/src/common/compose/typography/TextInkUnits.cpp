/** @file
 * THE INK EACH GLYPH DRAWS WITH. A span keeps its resolved foreground;
 * an ink that names a unit of its
 * passage — a glyph, a cluster, a word, a line, a sentence — paints every
 * such unit afresh, its unit square on the text-metric box of that unit:
 * across the unit's advances, from the cap top down to the baseline. One
 * body for both draws that ask: the kernel's draw at rest and the dressed
 * painter's, each handing over the layout it draws and the poses it places
 * the glyphs by, so a letter on a curve gets the box it stands in.
 */

#include <include/core/SkFontMetrics.h>
#include <include/core/SkMatrix.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilweave/choreograph/Choreograph.h>  // forEachPlacedGlyph
#include <sigilweave/fonts/Shaper.h>             // makeFont — the cap height
#include <src/core/SkScopeExit.h>

#include <algorithm>
#include <cmath>
#include <optional>
#include <utility>
#include <vector>

#include "TextEngine.h"
#include "TextPose.h"
#include "paint/SurfaceMap.h"
#include "sigilweave/advanced/Skia.h"

namespace sigil::compose {

namespace {

/** The height of a face's capitals above its baseline, where the face
 *  reports one, else its ascent. Memoized per (face, size) across a walk,
 *  as the glyph band is. */
float capHeightOf(const sigil::weave::ShapedWord* shaped,
                  std::vector<std::pair<BandKey, float>>& memo) {
  if (!shaped || !shaped->typeface) return 0.0f;
  const BandKey key{shaped->typeface.identity(), shaped->fontSize};
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
        pose.centre.x + corner.x() * pose.cosine - corner.y() * pose.sine,
        pose.centre.y + corner.x() * pose.sine + corner.y() * pose.cosine};
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

/** One run of glyphs one ink paints afresh. Its mapping spans all styles
 *  inside the unit, including font changes that split a word. */
struct UnitGroup {
  sk_sp<SkShader> shader;
  std::optional<material::Color> color;
  bool mapped = false;
  SkRect box = SkRect::MakeEmpty();
  bool placed = false;
  const detail::TextInk::Surface* surface = nullptr;
  /** The group's paint already is a surface map of its material. */
  bool surfaceMap = false;
};

struct GroupStyle {
  const sigil::weave::PaintStyle* base;
  uint32_t group;
};

}  // namespace

void detail::resolveGlyphInk(const sigil::weave::ParagraphLayout& layout,
                             const Instance& inst,
                             const GlyphStructure& structure,
                             const PoseContext& poses, const TextInk& ink,
                             GlyphInk& glyphs) {
  glyphs.styles.clear();
  glyphs.styleOfGlyph.assign(structure.glyphs.size(), GlyphInk::kOwnPaint);
  if (!inst.paragraph) return;
  static thread_local std::vector<UnitGroup> groups;
  static thread_local std::vector<GroupStyle> styles;
  static thread_local std::vector<std::pair<BandKey, float>> caps;
  const SkScopeExit releaseGroups([&] {
    groups.clear();
    styles.clear();
  });
  groups.clear();
  styles.clear();
  caps.clear();

  // A unit is a run of glyphs one ink restarts on, so a glyph opens a new
  // one where the ink painting it changes or its unit does. The leaf's
  // own ink reaches every glyph; a span's reaches the glyphs whose style
  // still carries its material owner after later restyles.
  constexpr uint32_t kNone = ~0u;
  uint32_t openInk = kNone, openUnit = kNone;
  sigil::weave::forEachPlacedGlyph(
      layout, *inst.paragraph, [&](const sigil::weave::PlacedGlyph& placed) {
        const uint32_t ordinal = placed.ordinal;
        if (ordinal >= structure.glyphs.size()) return;
        uint32_t whose = kNone;
        const sigil::weave::PaintStyle* base = nullptr;
        sk_sp<SkShader> shader;
        std::optional<material::Color> color;
        std::optional<sigil::weave::Unit> unit;
        const TextInk::Surface* surface = nullptr;
        bool surfaceMap = false;
        if ((ink.unitSquare || ink.unitSurface) && ink.passage) {
          whose = 0;
          base = &*ink.passage;
          shader = ink.unitSquare;
          unit = ink.unit;
          if (ink.unitSurface) surface = &*ink.unitSurface;
          surfaceMap = surface && surface->map;
        } else if (placed.paint->foregroundMaterial) {
          for (size_t index = ink.spans.size(); index-- > 0;) {
            const TextInk::Span& span = ink.spans[index];
            if (span.source != placed.paint->foregroundMaterial) continue;
            whose = (uint32_t)index + 1;
            base = placed.paint;
            shader = span.shader;
            color = span.color;
            unit = span.unit;
            if (span.surface) surface = &*span.surface;
            surfaceMap = span.surfaceMap;
            break;
          }
        }
        if (!base || (unit && (size_t)*unit >= GlyphStructure::kUnits)) {
          openInk = kNone;
          return;
        }
        const uint32_t unitIndex =
            unit ? structure.unitOf[(size_t)*unit][ordinal] : 0;
        if (whose != openInk || unitIndex != openUnit || groups.empty()) {
          groups.push_back({std::move(shader), color, unit.has_value()});
          groups.back().surface = surface;
          groups.back().surfaceMap = surfaceMap;
          openInk = whose;
          openUnit = unitIndex;
        }
        UnitGroup& group = groups.back();
        RestPose pose;
        if (unit && restPoseOf(poses, placed, pose)) {
          const SkRect box =
              inkBoxOf(placed, pose, capHeightOf(placed.shaped, caps));
          if (group.placed)
            group.box.join(box);
          else
            group.box = box;
          group.placed = true;
        }
        const uint32_t groupIndex = (uint32_t)groups.size() - 1;
        if (styles.empty() || styles.back().base != base ||
            styles.back().group != groupIndex)
          styles.push_back({base, groupIndex});
        glyphs.styleOfGlyph[ordinal] = (uint32_t)styles.size() - 1;
      });

  for (UnitGroup& group : groups) {
    if (group.mapped && (group.shader || group.surface)) {
      const SkRect box = group.placed ? group.box : SkRect::MakeWH(1, 1);
      SkMatrix map = SkMatrix::Translate(box.left(), box.top());
      map.preScale(std::max(box.width(), 1.0f), std::max(box.height(), 1.0f));
      if (group.surface)
        group.shader =
            group.surface->shader(ink.frame, material::skia::toMatrix(map));
      else
        group.shader = group.shader->makeWithLocalMatrix(map);
    }
  }
  // An ink that resolved to neither a shader nor a colour paints nothing
  // of its own, so its glyphs keep the paint they would have drawn with
  // anyway: the range keeps its own ink rather than vanishing. Every style
  // that does paint is renumbered past the ones dropped.
  static thread_local std::vector<uint32_t> renumbered;
  renumbered.assign(styles.size(), GlyphInk::kOwnPaint);
  glyphs.styles.reserve(styles.size());
  for (size_t index = 0; index < styles.size(); ++index) {
    const GroupStyle& entry = styles[index];
    const UnitGroup& group = groups[entry.group];
    if (!group.shader && !group.color) continue;
    sigil::weave::PaintStyle style = *entry.base;
    style.foregroundMaterial.reset();
    style.foreground.setShader(group.shader);
    if (!group.shader)
      style.foreground.setColor4f(material::skia::toSkColor(*group.color),
                                  nullptr);
    if (group.surfaceMap) detail::markSurfaceMap(style.foreground);
    renumbered[index] = (uint32_t)glyphs.styles.size();
    glyphs.styles.push_back(std::move(style));
  }
  for (uint32_t& named : glyphs.styleOfGlyph)
    if (named != GlyphInk::kOwnPaint) named = renumbered[named];
}

void detail::glyphInkAtRest(Instance& inst, const TextInk& ink,
                            GlyphInk& glyphs) {
  glyphs.styles.clear();
  glyphs.styleOfGlyph.clear();
  if (!inst.paragraph) return;
  static thread_local GlyphStructure structure;
  structure.build(inst.textLayout, *inst.paragraph, scopeOf(inst));
  const PoseContext poses{.inst = &inst, .layout = &inst.textLayout};
  resolveGlyphInk(inst.textLayout, inst, structure, poses, ink, glyphs);
}

}  // namespace sigil::compose
