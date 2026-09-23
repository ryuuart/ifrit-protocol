/** @file
 * The text painter — the value a text verb installs on a description, and
 * the verbs themselves. `textFx()`, `textOnPath()`, `textAttach()`, `span()`
 * and `variationDrive()` are declared on the text leaf by the
 * kernel and defined here, so an element that dresses its type links this
 * tier and carries the engine that draws it; the kernel reaches the engine
 * only through the value. The fold that lets a span restyle ride as axis
 * tracks lives here too, beside the axis gate it consults.
 */

#include <include/core/SkTypes.h>  // SkDebugf — the span's paint diagnostic

#include <algorithm>
#include <cstdio>  // std::snprintf — variationDrive's effect key
#include <cstring>

#include "AxisGate.h"
#include "TextEngine.h"

namespace sigil::compose {

using namespace detail;

namespace {

/** The once-per-process diagnostic behind a span stating an ink paint it
 *  has no one shader for, so a range left in its own ink is not mistaken
 *  for one the paint took. */
void warnSpanInkHasNoOneShader() {
  static thread_local bool warned = false;
  if (warned) return;
  warned = true;
  SkDebugf(
      "[compose] span() states an ink paint that animates or is resolved "
      "against a box, and a range has neither a frame nor a box of its own "
      "to resolve it in — the paint was left out and the range keeps the "
      "ink it is set in. State a colour or a static paint, or put the paint "
      "on the leaf's own ink(). (warned once)\n");
}

/** The engine as a TextPainterOperations: every operation forwards to the
 *  engine's own body with the instance's composer. One value for every
 *  text, so two descriptions dressing their type compare equal here — and
 *  the field is excluded from equality regardless. */
struct TextEngine final : TextPainterOperations {
  bool operator==(const TextEngine&) const { return true; }
  void paint(Instance& inst, SkCanvas& canvas, const TextInk& ink,
             const TextPath* onPath, SkSize size,
             const PaintContext& ctx) const override {
    paintTextFx(*inst.owner, inst, canvas, ink, onPath, size, ctx);
  }
  void inkByUnit(Instance& inst, const TextInk& ink,
                 GlyphInk& glyphs) const override {
    inkAtRestByUnit(inst, ink, glyphs);
  }
  void marks(Instance& inst) const override {
    resolveTextMarks(*inst.owner, inst);
  }
  std::vector<sigil::weave::CharRange> ranges(
      const sigil::weave::Selector& selector,
      sigil::weave::Paragraph& paragraph, sigil::weave::FontContext& fonts,
      std::span<const sigil::weave::LineMetrics> lines,
      std::span<const sigil::weave::ColumnMetrics> columns,
      std::span<const NamedRun> named, TextScope scope) const override {
    return resolveTextRanges(selector, paragraph, fonts, lines, columns, named,
                             scope);
  }
  bool foldable(
      Instance& inst, const sigil::weave::TextStyle& style,
      std::span<const sigil::weave::CharRange> ranges,
      const sigil::weave::Paragraph& paragraph,
      std::span<const sigil::weave::CharRange> paintCarried,
      std::vector<std::pair<std::string, float>>& axes) const override {
    return foldableAsAxes(*inst.owner, style, ranges, paragraph, paintCarried,
                          axes);
  }
  std::vector<TextUnit> units(Instance& inst,
                              const sigil::weave::Selector& selector,
                              sigil::weave::Unit unit) const override {
    return unitsOfText(*inst.owner, inst, selector, unit);
  }
  void annotations(Instance& inst) const override {
    resolveTextAnnotations(*inst.owner, inst);
  }
  sigil::weave::ReservedBand reservedBand(
      Instance& inst, std::span<const Annotation> annotations) const override {
    return reservedBandOf(*inst.owner, inst, annotations);
  }
  std::vector<Beat> beats(Instance& inst, size_t trackIndex) const override {
    return beatsOfTrack(inst, trackIndex);
  }
  float cascadeSpanMs(Instance& inst, size_t trackIndex) const override {
    return cascadeSpanOfTrack(inst, trackIndex);
  }
};

/** ONE painter value for the process: a verb copies it (a refcount bump)
 *  rather than minting one per describe. */
const TextPainter& enginePainter() {
  static const TextPainter kPainter{TextEngine{}};
  return kPainter;
}

/** THE ENGINE, ANNOUNCED. A text leaf that dresses nothing carries no
 *  painter, and the read-back queries would then have to answer empty
 *  about a passage that is perfectly well laid out. Linking this tier is
 *  what makes them answer; the registration happens as the process starts
 *  and nothing depends on the order it happens in, because the queries run
 *  long after. */
const bool kEngineRegistered = [] {
  detail::registerTextEngine(enginePainter().get());
  return true;
}();

/** The node's text block with the engine installed — what every verb that
 *  dresses type writes into. */
TextData& dressedText(TextData& text) {
  if (!text.painter) text.painter = enginePainter();
  return text;
}

}  // namespace

// ---------------------------------------------------------------------------
// The verbs

template <class Derived>
Derived& TextContentVerbs<Derived>::textOnPath(TextPath spec) {
  dressedText(declarations()->textData.ensure()).onPath = std::move(spec);
  return self();
}

template <class Derived>
Derived& TextContentVerbs<Derived>::textFx(Track track) {
  dressedText(declarations()->textData.ensure())
      .tracks.push_back(std::move(track));
  return self();
}

template <class Derived>
Derived& TextContentVerbs<Derived>::textAnnotation(Annotation reading) {
  detail::TextData& text = dressedText(declarations()->textData.ensure());
  text.annotations.push_back(std::move(reading));
  return self();
}

template <class Derived>
Derived& TextContentVerbs<Derived>::textAttach(sigil::weave::Selector where,
                                               Element what) {
  detail::ElementNode* node = declarations();
  detail::TextData& text = dressedText(node->textData.ensure());
  // A KEY IS THE ANCHOR'S HANDLE, so a mark that carries none is given one
  // from its declaration order: the layout looks its rect up by key, and
  // the reconciler matches children by key. The generated name is namespaced
  // with a character no author writes, so it cannot collide with a key
  // somebody chose.
  if (what.node()->key.empty())
    what.key("mark#" + std::to_string(text.marks.size()));
  text.marks.push_back({std::move(where), what.node()->key});
  detail::NodeAccess::append(self(), std::move(what));
  return self();
}

template <class Derived>
Derived& TextContentVerbs<Derived>::variationDrive(
    const char (&tag)[5], const choreograph::Output<float>* value) {
  // SUGAR over textFx(): an axis coordinate is a per-glyph deviation like a
  // shove or a fade, so the drive is a whole-text track and composes with
  // whatever other tracks the element carries. A second, parallel text path
  // would be hidden by any track drawn over it, which is why the drive is
  // a track.
  //
  // The effect reads the Output DIRECTLY rather than through the track's
  // progress, because an axis coordinate is a design-space number (GRAD
  // runs to ±100 on the faces that have it) and a progress is a 0→1 ramp
  // the cascade clamps. The progress is bound to the same Output for the
  // one thing it is good for here: declaring the paint volatility, so the
  // node repaints while the drive moves and settles when it stops.
  const sigil::weave::FontVariation coordinate(tag, 0.0f);
  detail::TextData& text = dressedText(declarations()->textData.ensure());
  // The effect's key IS its identity, and a drive is identified by its axis
  // and by its place among the element's tracks — declaration order, the
  // handle a keyless mark takes for the same reason. WHICH Output feeds it
  // is carried by the track's own progress, compared where every bound
  // value in the tree is; the Output's ADDRESS is not identity, because a
  // destroyed Output's address comes back on the next one allocated and a
  // key holding it would prune a live drive onto the dead body.
  char key[32];
  std::snprintf(key, sizeof(key), "variationDrive:%.4s#%zu", tag,
                text.tracks.size());
  Track track;
  track.effect = TextEffect(
      key, {},
      [coordinate, value](const GlyphInfo&, float, core::noise::Mix64Stream&) {
        GlyphModifier mod;
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
  text.tracks.push_back(std::move(track));
  return self();
}

template <class Derived>
Derived& TextContentVerbs<Derived>::span(sigil::weave::Selector where,
                                         SpanStyle style) {
  detail::SpanRestyle restyle;
  restyle.where = std::move(where);
  // What the span states is read off its style as the cascade reads a
  // node's: the font partial, the ink's colour inside it, and the ink as
  // a property or a paint.
  const detail::ElementNode& stated = *style.node();
  if (stated.cascadeData) {
    const detail::CascadeData& said = *stated.cascadeData;
    if (said.font) restyle.partial = *said.font;
    restyle.fontFamily = said.fontFamily;
    restyle.italic = said.italic;
    restyle.inkVar = said.inkVar;
    // A static paint collapses to one fill: a flat colour is the ink's
    // colour, anything else a shader over the range. A live paint has no
    // one shader to give a range, and nor has a geometry-dependent one —
    // unless it restarts per unit, when its box is the unit square the
    // engine lays on each unit. The ink verb kept a unit only under the
    // own box.
    const bool restarts = said.inkUnit.has_value();
    if (said.inkPaint && (said.inkPaint->isAnimated() ||
                          (said.inkPaint->geometryDependent() && !restarts))) {
      warnSpanInkHasNoOneShader();
    } else if (said.inkPaint) {
      PaintContext unitSquare;
      unitSquare.size = {1.0f, 1.0f};
      const Fill flat = restarts ? resolveFill(*said.inkPaint, unitSquare)
                                 : toFill(*said.inkPaint);
      if (flat.kind == Fill::Kind::Color) {
        restyle.partial.color = flat.colorValue;
      } else if (flat.kind == Fill::Kind::Shader) {
        restyle.inkShader = flat;
        if (restarts) restyle.inkUnit = said.inkUnit;
      }
    }
  }
  dressedText(declarations()->textData.ensure())
      .spanRestyles.push_back(std::move(restyle));
  return self();
}

// The six members of the text-content family this tier defines, named
// one by one: the family's other members are instantiated where they are
// defined, in the kernel.
template Text& TextContentVerbs<Text>::textOnPath(TextPath);
template Text& TextContentVerbs<Text>::textFx(Track);
template Text& TextContentVerbs<Text>::textAnnotation(Annotation);
template Text& TextContentVerbs<Text>::textAttach(sigil::weave::Selector,
                                                  Element);
template Text& TextContentVerbs<Text>::variationDrive(
    const char (&)[5], const choreograph::Output<float>*);
template Text& TextContentVerbs<Text>::span(sigil::weave::Selector, SpanStyle);

// ---------------------------------------------------------------------------
// The fold

bool detail::foldableAsAxes(
    Composer::Impl& impl, const sigil::weave::TextStyle& style,
    std::span<const sigil::weave::CharRange> ranges,
    const sigil::weave::Paragraph& paragraph,
    std::span<const sigil::weave::CharRange> paintCarried,
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
    // …except the PAINT of a span an earlier declaration has coloured,
    // which that declaration owns. A fold writes no paint, so the colour
    // standing on such a span is the one meant to stand and cannot be the
    // reason to re-shape. WHOLLY inside, because a span only half covered
    // would keep the earlier colour across its other half too.
    for (const sigil::weave::CharRange& kept : paintCarried)
      if (kept.start <= span.start && span.end <= kept.end) {
        probe.paint = style.paint;
        break;
      }
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
