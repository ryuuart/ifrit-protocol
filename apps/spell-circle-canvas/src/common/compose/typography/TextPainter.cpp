/** @file
 * The text painter — the value a text verb installs on a description, and
 * the verbs themselves. `fx()`, `onPath()`, `mark()`, `spanStyle()`,
 * `spanPaint()` and `variationDrive()` are declared on Element by the
 * kernel and defined here, so an element that dresses its type links this
 * tier and carries the engine that draws it; the kernel reaches the engine
 * only through the value. The fold that lets a span restyle ride as axis
 * tracks lives here too, beside the axis gate it consults.
 */

#include <algorithm>
#include <cstdio>  // std::snprintf — variationDrive's effect key
#include <cstring>

#include "AxisGate.h"
#include "TextEngine.h"

namespace sigil::compose {

using namespace detail;

namespace {

/** The engine as a TextPainterOps: every operation forwards to the
 *  engine's own body with the instance's composer. One value for every
 *  text, so two descriptions dressing their type compare equal here — and
 *  the field is excluded from equality regardless. */
struct TextEngine final : TextPainterOps {
  bool operator==(const TextEngine&) const { return true; }
  void paint(Instance& inst, SkCanvas& canvas,
             const sigil::weave::PaintStyle* override, const TextPath* onPath,
             SkSize size, const PaintContext& ctx) const override {
    paintTextFx(*inst.owner, inst, canvas, override, onPath, size, ctx);
  }
  void marks(Instance& inst) const override {
    resolveTextMarks(*inst.owner, inst);
  }
  std::vector<sigil::weave::CharRange> ranges(
      const Selector& selector, sigil::weave::Paragraph& paragraph,
      sigil::weave::FontContext& fonts,
      std::span<const sigil::weave::LineMetrics> lines,
      std::span<const sigil::weave::ColumnMetrics> columns,
      std::span<const NamedRun> named) const override {
    return resolveTextRanges(selector, paragraph, fonts, lines, columns, named);
  }
  bool foldable(
      Instance& inst, const sigil::weave::TextStyle& style,
      std::span<const sigil::weave::CharRange> ranges,
      const sigil::weave::Paragraph& paragraph,
      std::vector<std::pair<std::string, float>>& axes) const override {
    return foldableAsAxes(*inst.owner, style, ranges, paragraph, axes);
  }
  std::vector<Beat> beats(Instance& inst, size_t trackIndex) const override {
    return beatsOfTrack(*inst.owner, inst, trackIndex);
  }
  float cascadeSpanMs(Instance& inst, size_t trackIndex) const override {
    return cascadeSpanOfTrack(*inst.owner, inst, trackIndex);
  }
};

/** ONE painter value for the process: a verb copies it (a refcount bump)
 *  rather than minting one per describe. */
const TextPainter& enginePainter() {
  static const TextPainter kPainter{TextEngine{}};
  return kPainter;
}

/** The node's text block with the engine installed — what every verb that
 *  dresses type writes into. */
TextData& dressedText(TextData& text) {
  if (!text.painter) text.painter = enginePainter();
  return text;
}

}  // namespace

// ---------------------------------------------------------------------------
// The verbs

Element& Element::onPath(TextPath spec) {
  dressedText(m_node->textData.ensure()).onPath = std::move(spec);
  return *this;
}

Element& Element::fx(Track track) {
  dressedText(m_node->textData.ensure()).tracks.push_back(std::move(track));
  return *this;
}

Element& Element::mark(Selector where, Element what) {
  detail::TextData& text = dressedText(m_node->textData.ensure());
  // A KEY IS THE ANCHOR'S HANDLE, so a mark that carries none is given one
  // from its declaration order: the layout looks its rect up by key, and
  // the reconciler matches children by key. The generated name is namespaced
  // with a character no author writes, so it cannot collide with a key
  // somebody chose.
  if (what.node()->key.empty())
    what.key("mark#" + std::to_string(text.marks.size()));
  text.marks.push_back({std::move(where), what.node()->key});
  return child(std::move(what));
}

Element& Element::variationDrive(const char (&tag)[5],
                                 const choreograph::Output<float>* value) {
  // SUGAR over fx(): an axis coordinate is a per-glyph deviation like a
  // shove or a fade, so the drive is a whole-text track and composes with
  // whatever other tracks the element carries. A second, parallel text path
  // is what it used to be, and a track drawn over it hid it completely.
  //
  // The effect reads the Output DIRECTLY rather than through the track's
  // progress, because an axis coordinate is a design-space number (GRAD
  // runs to ±100 on the faces that have it) and a progress is a 0→1 ramp
  // the cascade clamps. The progress is bound to the same Output for the
  // one thing it is good for here: declaring the paint volatility, so the
  // node repaints while the drive moves and settles when it stops.
  const sigil::weave::FontVariation coordinate(tag, 0.0f);
  // The effect's key IS its identity, and a drive is identified by its axis
  // and by WHICH Output feeds it — the binding identity every bound value
  // in the tree is compared by. Two drives of one axis from two Outputs
  // must not prune onto each other.
  char key[64];
  std::snprintf(key, sizeof(key), "variationDrive:%.4s@%p", tag,
                (const void*)value);
  Track track;
  track.effect = TextEffect(
      key, {},
      [coordinate, value](const GlyphInfo&, float, Rng&) {
        GlyphMod mod;
        if (!value) return mod;
        sigil::weave::FontVariation driven = coordinate;
        driven.value = value->value();
        mod.axis = driven;
        return mod;
      },
      // Only an ADVANCE-INVARIANT axis is honoured, which is precisely the
      // condition that the glyphs keep the pen positions shaping gave them:
      // a drive re-cuts outlines where they already stand, so a run under a
      // sweeping grade is type at rest and keeps its whole-pixel origins.
      /*reach=*/0.0f, /*curves=*/{}, /*displaces=*/false);
  track.progress = value;
  dressedText(m_node->textData.ensure()).tracks.push_back(std::move(track));
  return *this;
}

Element& Element::spanPaint(Selector where, sigil::weave::PaintStyle paint) {
  detail::SpanRestyle restyle;
  restyle.where = std::move(where);
  restyle.style.paint = std::move(paint);
  restyle.paintOnly = true;
  dressedText(m_node->textData.ensure())
      .spanRestyles.push_back(std::move(restyle));
  return *this;
}

Element& Element::spanStyle(Selector where, sigil::weave::TextStyle style) {
  detail::SpanRestyle restyle;
  restyle.where = std::move(where);
  restyle.style = std::move(style);
  dressedText(m_node->textData.ensure())
      .spanRestyles.push_back(std::move(restyle));
  return *this;
}

// ---------------------------------------------------------------------------
// The fold

bool detail::foldableAsAxes(Composer::Impl& impl,
                            const sigil::weave::TextStyle& style,
                            std::span<const sigil::weave::CharRange> ranges,
                            const sigil::weave::Paragraph& paragraph,
                            std::vector<std::pair<std::string, float>>& axes) {
  axes.clear();
  const auto sameTag = [](const sigil::weave::FontVariation& a,
                          const sigil::weave::FontVariation& b) {
    return std::memcmp(a.tag, b.tag, sizeof a.tag) == 0;
  };
  const std::vector<sigil::weave::FontVariation>& wanted =
      style.shaping.variations;
  for (const sigil::weave::StyleSpan& span : paragraph.spans()) {
    bool covered = false;
    for (const sigil::weave::CharRange& range : ranges)
      covered |= range.start < span.end && span.start < range.end;
    if (!covered) continue;
    // The covered text's own style, with the wanted axes written over it,
    // must BE the wanted style: any other difference is a reshape.
    sigil::weave::TextStyle probe = span.style;
    probe.shaping.variations = wanted;
    if (!(probe == style)) return false;
    // An axis the text was shaped with and the restyle leaves out is a
    // reset to the face's default, which is a reshape.
    for (const sigil::weave::FontVariation& have :
         span.style.shaping.variations)
      if (std::ranges::none_of(wanted,
                               [&](const auto& w) { return sameTag(w, have); }))
        return false;
    const sk_sp<SkTypeface> face = span.style.shaping.typeface
                                       ? span.style.shaping.typeface
                                       : impl.fonts.defaultTypeface();
    for (const sigil::weave::FontVariation& want : wanted) {
      const auto have =
          std::ranges::find_if(span.style.shaping.variations,
                               [&](const auto& h) { return sameTag(h, want); });
      if (have != span.style.shaping.variations.end() &&
          have->value == want.value)
        continue;  // already shaped there: nothing to hold
      const char tag[5] = {want.tag[0], want.tag[1], want.tag[2], want.tag[3],
                           '\0'};
      if (!axisGateProbe(impl.fonts, face, tag).allowed) return false;
      const std::string name(tag, 4);
      if (std::ranges::none_of(axes,
                               [&](const auto& a) { return a.first == name; }))
        axes.emplace_back(name, want.value);
    }
  }
  return true;
}

}  // namespace sigil::compose
