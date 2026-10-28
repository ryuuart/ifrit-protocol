/** @file
 * The font and the ink — the partial every passage under a node is set
 * in, its longhands, and the colour or paint text and unnamed marks are
 * painted in.
 */

#include <include/core/SkBlurTypes.h>
#include <include/core/SkMaskFilter.h>
#include <sigilcompose/core/StyleSheet.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/filter/Filter.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilmaterial/skia/SurfaceMaps.h>
#include <sigilweave/style/PaintLayer.h>
#include <sigilweave/style/Type.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "description/ComposeInternal.h"
#include "paint/MaterialEffects.h"
#include "paint/SurfaceMap.h"
#include "runtime/Warning.h"

namespace sigil::compose {

namespace detail {

void warnInkTextUnitNeedsAPassage() {
  static thread_local bool warned = false;
  if (warned) return;
  warned = true;
  sigil::compose::detail::reportWarning(
      "ink() names a text unit (Glyph, Cluster, Word, Line or "
      "Sentence) on a node that is no passage, whether by its own verb or "
      "a rule landing on it — the unit was dropped and the paint is "
      "stretched over PaintBox::Element. State the unit on the text leaf, a "
      "span of it, or a rule that lands on the leaf. (warned once)");
}

}  // namespace detail

namespace {

/** The once-per-process diagnostic behind an ink handed `Padding` or
 *  `Content`, which are rectangles of a fill's box: an ink is laid on
 *  the text it reaches, never on a node's padding. */
void warnInkTakesNoFillBox() {
  static thread_local bool warned = false;
  if (warned) return;
  warned = true;
  sigil::compose::detail::reportWarning(
      "ink() was given PaintBox::Padding or PaintBox::Content, "
      "which place a fill inside its box — the paint is stretched over "
      "PaintBox::Element. (warned once)");
}

/** THE BLUR A GLYPH'S SHADOW IS SOFTENED BY, one object per sigma: a
 *  pass compares its mask filter by identity, so a partial described
 *  again with the same shadow must hold the same filter to compare equal
 *  and leave the passage painted as it was. */
sk_sp<SkMaskFilter> shadowBlurOf(float sigma) {
  static std::mutex guard;
  static std::map<float, sk_sp<SkMaskFilter>> held;
  // A sketch states a handful of softnesses; past this many, one more is
  // made fresh rather than held for the life of the process.
  constexpr size_t kHeldSoftnesses = 256;
  const std::lock_guard<std::mutex> lock(guard);
  if (const auto found = held.find(sigma); found != held.end())
    return found->second;
  sk_sp<SkMaskFilter> made = SkMaskFilter::MakeBlur(kNormal_SkBlurStyle, sigma);
  if (held.size() < kHeldSoftnesses) held.emplace(sigma, made);
  return made;
}

/** The stand-ins, one per property, in both directions. */
struct ShadowPropertyMarkers {
  std::mutex guard;
  std::map<uint32_t, std::shared_ptr<const material::Material>> byProperty;
  std::map<const material::Material*, uint32_t> byMarker;
};

ShadowPropertyMarkers& shadowPropertyMarkers() {
  static ShadowPropertyMarkers markers;
  return markers;
}

}  // namespace

std::shared_ptr<const material::Material> detail::shadowPropertyMarker(
    VarRef property) {
  ShadowPropertyMarkers& markers = shadowPropertyMarkers();
  const std::lock_guard<std::mutex> lock(markers.guard);
  std::shared_ptr<const material::Material>& held =
      markers.byProperty[property.id];
  if (!held) {
    // Clear, so a stand-in the cascade never reached paints nothing.
    held =
        std::make_shared<const material::Material>(material::Color{0, 0, 0, 0});
    markers.byMarker[held.get()] = property.id;
  }
  return held;
}

std::optional<VarRef> detail::shadowPropertyOf(
    const std::shared_ptr<const material::Material>& material) {
  if (!material) return std::nullopt;
  ShadowPropertyMarkers& markers = shadowPropertyMarkers();
  const std::lock_guard<std::mutex> lock(markers.guard);
  const auto found = markers.byMarker.find(material.get());
  if (found == markers.byMarker.end()) return std::nullopt;
  return VarRef{found->second};
}

template <class Derived>
Derived& FontVerbs<Derived>::font(sigil::weave::Type partial) {
  detail::CascadeData& cascade = declarations()->font();
  if (!cascade.font) cascade.font.emplace();
  // Later wins field by field: the partial written last replaces what it
  // names and leaves the rest as an earlier call or a class left it.
  sigil::weave::merge(*cascade.font, partial);
  // A face written here stands over a family named before it, and a lean
  // is a style of its own, which is no italic.
  if (partial.face || partial.keywords.find(sigil::weave::TypeField::Face)) {
    cascade.fontFamily.reset();
    cascade.fontStyleName.reset();
  }
  if (partial.slant)
    cascade.italic = false;
  else if (partial.keywords.find(sigil::weave::TypeField::Slant))
    cascade.italic.reset();
  // A colour written here IS the ink — one lane, one bit — so it states
  // the ink as `ink()` does: a keyword stated about the ink before ends,
  // and a property the ink was read from and a paint no longer stand.
  if (partial.ink) {
    declarations()->ink();
    cascade.inkVar.reset();
    cascade.inkPaint.reset();
    cascade.inkSurfaced.reset();
    cascade.inkBox = PaintBox::Element;
    cascade.statesInk = true;
  }
  return self();
}

// The font's longhands: each is `font()` with one field, so the two
// spellings fold into one partial and the later statement wins.

template <class Derived>
Derived& FontVerbs<Derived>::fontFamily(std::string family) {
  // An empty name is the default family, which is a face the partial can
  // state; any other name waits for the cascade, where a context is.
  if (family.empty()) return font({.face = sigil::weave::defaultFace()});
  detail::CascadeData& cascade = declarations()->font();
  if (cascade.font) {
    cascade.font->face.reset();
    cascade.font->keywords.clear(sigil::weave::TypeField::Face);
    if (cascade.font->empty()) cascade.font.reset();
  }
  cascade.fontFamily = std::move(family);
  cascade.fontStyleName.reset();
  return self();
}

template <class Derived>
Derived& FontVerbs<Derived>::fontFamily(std::string family,
                                        std::string styleName) {
  if (family.empty() || styleName.empty()) return fontFamily(std::move(family));
  fontFamily(std::move(family));
  // The named face carries its own weight and style, so a weight, a lean
  // or an italic written before it in this statement gives way to it; one
  // written after it overrides the face's traits instead.
  detail::CascadeData& cascade = declarations()->font();
  if (cascade.font) {
    cascade.font->weight.reset();
    cascade.font->slant.reset();
    cascade.font->keywords.clear(sigil::weave::TypeField::Weight);
    cascade.font->keywords.clear(sigil::weave::TypeField::Slant);
    if (cascade.font->empty()) cascade.font.reset();
  }
  cascade.italic.reset();
  cascade.fontStyleName = std::move(styleName);
  return self();
}

template <class Derived>
Derived& FontVerbs<Derived>::fontFamily(sigil::weave::Face face) {
  // The face field itself: a held face needs no context to resolve, and
  // `font()` already ends a family named before it.
  return font({.face = std::move(face)});
}

namespace {

/** The text with its leading and trailing spaces dropped. */
std::string_view trimmed(std::string_view text) {
  const size_t first = text.find_first_not_of(" \t\n\r");
  if (first == std::string_view::npos) return {};
  const size_t last = text.find_last_not_of(" \t\n\r");
  return text.substr(first, last - first + 1);
}

/** The comma-separated entries of @p text, each trimmed. */
std::vector<std::string_view> entriesOf(std::string_view text) {
  std::vector<std::string_view> entries;
  size_t start = 0;
  while (start <= text.size()) {
    const size_t comma = text.find(',', start);
    const size_t end = comma == std::string_view::npos ? text.size() : comma;
    entries.push_back(trimmed(text.substr(start, end - start)));
    if (comma == std::string_view::npos) break;
    start = comma + 1;
  }
  return entries;
}

/** An OpenType tag at the head of @p entry — four printable ASCII
 *  characters, in single or double quotes as CSS writes it, or bare — with
 *  @p rest left holding what follows it. */
std::optional<std::array<char, 4>> tagAtHead(std::string_view entry,
                                             std::string_view& rest) {
  std::string_view tag;
  if (!entry.empty() && (entry.front() == '"' || entry.front() == '\'')) {
    const size_t close = entry.find(entry.front(), 1);
    if (close == std::string_view::npos) return std::nullopt;
    tag = entry.substr(1, close - 1);
    rest = trimmed(entry.substr(close + 1));
  } else {
    const size_t space = entry.find_first_of(" \t");
    tag = entry.substr(0, space);
    rest = space == std::string_view::npos ? std::string_view{}
                                           : trimmed(entry.substr(space));
  }
  if (tag.size() != 4) return std::nullopt;
  std::array<char, 4> bytes{};
  for (size_t index = 0; index < 4; ++index) {
    if (tag[index] < 0x20 || tag[index] > 0x7e) return std::nullopt;
    bytes[index] = tag[index];
  }
  return bytes;
}

/** A number written in full, and nothing after it. */
std::optional<float> numberOf(std::string_view text) {
  if (text.empty()) return std::nullopt;
  float value = 0;
  const auto [end, error] =
      std::from_chars(text.data(), text.data() + text.size(), value);
  if (error != std::errc{} || end != text.data() + text.size())
    return std::nullopt;
  return value;
}

/** Whether @p text is @p word, ASCII case aside. */
bool isKeyword(std::string_view text, std::string_view word) {
  return text.size() == word.size() &&
         std::equal(text.begin(), text.end(), word.begin(),
                    [](char left, char right) {
                      return std::tolower((unsigned char)left) == right;
                    });
}

}  // namespace

std::optional<std::vector<sigil::weave::FontFeature>> readFontFeatureSettings(
    std::string_view text) {
  text = trimmed(text);
  std::vector<sigil::weave::FontFeature> features;
  if (isKeyword(text, "normal")) return features;
  for (std::string_view entry : entriesOf(text)) {
    std::string_view rest;
    const std::optional<std::array<char, 4>> tag = tagAtHead(entry, rest);
    if (!tag) return std::nullopt;
    uint32_t value = 1;
    if (rest.empty() || isKeyword(rest, "on")) {
      value = 1;
    } else if (isKeyword(rest, "off")) {
      value = 0;
    } else {
      const std::optional<float> number = numberOf(rest);
      if (!number || *number < 0 || *number != (float)(uint32_t)*number)
        return std::nullopt;
      value = (uint32_t)*number;
    }
    sigil::weave::FontFeature feature;
    std::copy(tag->begin(), tag->end(), feature.tag);
    feature.value = value;
    features.push_back(feature);
  }
  return features;
}

std::optional<std::vector<sigil::weave::FontVariation>>
readFontVariationSettings(std::string_view text) {
  text = trimmed(text);
  std::vector<sigil::weave::FontVariation> variations;
  if (isKeyword(text, "normal")) return variations;
  for (std::string_view entry : entriesOf(text)) {
    std::string_view rest;
    const std::optional<std::array<char, 4>> tag = tagAtHead(entry, rest);
    if (!tag) return std::nullopt;
    const std::optional<float> number = numberOf(rest);
    if (!number) return std::nullopt;
    sigil::weave::FontVariation variation;
    std::copy(tag->begin(), tag->end(), variation.tag);
    variation.value = *number;
    variations.push_back(variation);
  }
  return variations;
}

namespace {

/** Says once per verb that it was given a length no font holds, and that
 *  the value in force stands. */
void warnNoFontLength(const char* verb, const Dimension& length) {
  static std::mutex guard;
  static std::map<std::string, bool> warned;
  {
    const std::lock_guard<std::mutex> lock(guard);
    if (!warned.emplace(verb, true).second) return;
  }
  sigil::compose::detail::reportWarning(
      "%s() was given a length in a unit no font holds (unit %d) — "
      "a font is sized in pixels, pt, em, rem, lh, ch or a percentage of "
      "the parent's size — so the value in force stands. (warned once)",
      verb, (int)length.unit);
}

}  // namespace

template <class Derived>
Derived& FontVerbs<Derived>::fontSize(Dimension size) {
  const std::optional<sigil::weave::Length> length = fontLength(size);
  if (!length) {
    warnNoFontLength("fontSize", size);
    return self();
  }
  return font({.size = *length});
}

template <class Derived>
Derived& FontVerbs<Derived>::fontWeight(float weight) {
  return font({.weight = weight});
}

template <class Derived>
Derived& FontVerbs<Derived>::fontStyle(FontStyle style) {
  // CSS leans right with a positive angle; OpenType's slnt axis leans
  // right with a negative one, so the angle is written negated. Normal
  // and Italic state no lean at all.
  font({.slant = style.kind == FontStyle::Kind::Oblique ? 0.0f - style.degrees
                                                        : 0.0f});
  if (style.kind == FontStyle::Kind::Italic)
    declarations()->font().italic = true;
  return self();
}

template <class Derived>
Derived& FontVerbs<Derived>::letterSpacing(Dimension tracking) {
  const std::optional<sigil::weave::Length> length = fontLength(tracking);
  if (!length) {
    warnNoFontLength("letterSpacing", tracking);
    return self();
  }
  return font({.letterSpacing = *length});
}

namespace {

/** Says once per verb that its CSS text did not read, and that the value
 *  in force stands. */
void warnUnreadSettings(const char* verb, std::string_view text) {
  static std::mutex guard;
  static std::map<std::string, bool> warned;
  {
    const std::lock_guard<std::mutex> lock(guard);
    if (!warned.emplace(verb, true).second) return;
  }
  sigil::compose::detail::reportWarning(
      "%s() was given '%.*s', which is no CSS value this reads — the value "
      "in force stands. (warned once)",
      verb, (int)text.size(), text.data());
}

}  // namespace

template <class Derived>
Derived& FontVerbs<Derived>::wordSpacing(Dimension spacing) {
  const std::optional<sigil::weave::Length> length = fontLength(spacing);
  if (!length) {
    warnNoFontLength("wordSpacing", spacing);
    return self();
  }
  return font({.wordSpacing = *length});
}

template <class Derived>
Derived& FontVerbs<Derived>::fontFeatureSettings(
    std::vector<sigil::weave::FontFeature> features) {
  return font({.features = std::move(features)});
}

template <class Derived>
Derived& FontVerbs<Derived>::fontFeatureSettings(std::string_view settings) {
  std::optional<std::vector<sigil::weave::FontFeature>> read =
      readFontFeatureSettings(settings);
  if (!read) {
    warnUnreadSettings("fontFeatureSettings", settings);
    return self();
  }
  return fontFeatureSettings(std::move(*read));
}

template <class Derived>
Derived& FontVerbs<Derived>::fontVariationSettings(
    std::vector<sigil::weave::FontVariation> variations) {
  return font({.variations = std::move(variations)});
}

template <class Derived>
Derived& FontVerbs<Derived>::fontVariationSettings(std::string_view settings) {
  std::optional<std::vector<sigil::weave::FontVariation>> read =
      readFontVariationSettings(settings);
  if (!read) {
    warnUnreadSettings("fontVariationSettings", settings);
    return self();
  }
  return fontVariationSettings(std::move(*read));
}

template <class Derived>
Derived& FontVerbs<Derived>::fontKerning(FontKerning kerning) {
  return font({.opticalKerning = kerning == FontKerning::Optical});
}

template <class Derived>
Derived& FontVerbs<Derived>::textTransform(
    sigil::weave::TextTransform transform) {
  return font({.textTransform = transform});
}

template <class Derived>
Derived& FontVerbs<Derived>::textOrientation(TextOrientation orientation) {
  using sigil::weave::VerticalForm;
  VerticalForm form = VerticalForm::kAuto;
  switch (orientation) {
    case TextOrientation::Mixed:
      form = VerticalForm::kAuto;
      break;
    case TextOrientation::Upright:
      form = VerticalForm::kUpright;
      break;
    case TextOrientation::Sideways:
      form = VerticalForm::kRotated;
      break;
  }
  return font({.verticalForm = form});
}

template <class Derived>
Derived& FontVerbs<Derived>::textCombineUpright(TextCombineUpright combine) {
  return font({.verticalForm = combine == TextCombineUpright::All
                                   ? sigil::weave::VerticalForm::kTateChuYoko
                                   : sigil::weave::VerticalForm::kAuto});
}

template <class Derived>
Derived& FontVerbs<Derived>::baselineShift(Dimension shift) {
  // A point is four pixels to every three, as everywhere a length reads.
  if (shift.unit == Dimension::Unit::Px)
    return font({.baselineShift = shift.value});
  if (shift.unit == Dimension::Unit::Pt)
    return font({.baselineShift = shift.value * 4.0f / 3.0f});
  warnNoFontLength("baselineShift", shift);
  return self();
}

template <class Derived>
Derived& FontVerbs<Derived>::textShadow(std::vector<TextShadow> shadows) {
  // The underlays are drawn back to front and CSS draws its first shadow
  // nearest the glyph, so the list is laid down in reverse. A pass whose
  // colour is transparent is drawn in the colour its glyph is painted in,
  // which is the ink in force; one naming a property carries its stand-in
  // until the cascade reads the property where the shadow is stated.
  std::vector<sigil::weave::PaintLayer> passes;
  passes.reserve(shadows.size());
  for (auto shadow = shadows.rbegin(); shadow != shadows.rend(); ++shadow) {
    const ColorTerm& colour = shadow->color;
    // A shadow stated clear draws nothing, and a clear pass would read as
    // the glyph's own colour.
    if (!colour.references() && colour.a <= 0) continue;
    sigil::weave::PaintLayer pass;
    pass.paint.setColor4f(colour.references()
                              ? SkColor4f{0, 0, 0, 0}
                              : material::skia::toSkColor(colour),
                          nullptr);
    if (colour.ref == Fill::Ref::Var)
      pass.material = detail::shadowPropertyMarker(VarRef{colour.varId});
    // The blur is CSS's radius, the Gaussian's standard deviation doubled.
    if (shadow->blur > 0)
      pass.paint.setMaskFilter(shadowBlurOf(shadow->blur * 0.5f));
    pass.offset = shadow->offset;
    passes.push_back(std::move(pass));
  }
  return font({.underlays = std::move(passes)});
}

template <class Derived>
Derived& FontVerbs<Derived>::textShadow(TextShadow shadow) {
  return textShadow(std::vector<TextShadow>{shadow});
}

template <class Derived>
Derived& FontVerbs<Derived>::ink(material::Color colour) {
  detail::CascadeData& cascade = declarations()->ink();
  if (!cascade.font) cascade.font.emplace();
  cascade.font->ink = colour;
  cascade.inkVar.reset();
  cascade.inkPaint.reset();
  cascade.inkSurfaced.reset();
  cascade.inkBox = PaintBox::Element;
  cascade.statesInk = true;
  return self();
}

template <class Derived>
Derived& FontVerbs<Derived>::ink(VarRef reference) {
  detail::CascadeData& cascade = declarations()->ink();
  cascade.inkVar = reference;
  if (cascade.font) cascade.font->ink.reset();
  cascade.inkPaint.reset();
  cascade.inkSurfaced.reset();
  cascade.inkBox = PaintBox::Element;
  cascade.statesInk = true;
  return self();
}

template <class Derived>
Derived& FontVerbs<Derived>::ink(material::Material material, PaintBox box) {
  // A RULE keeps the effects whole beside its ink, because they belong to
  // the ink and dress a matched element only where this rule's ink is the
  // one standing there; an element's own ink stands, so they are its marks.
  std::optional<material::Filter> effects;
  if (const material::Filter* stated = material.effects()) {
    if constexpr (std::is_same_v<Derived, Rule>)
      effects = *stated;
    else
      detail::applyEffects(*declarations(), *stated);
  }
  if constexpr (std::is_same_v<Derived, SpanStyle>) {
    // A span keeps the full surface until its visible ranges and drawing
    // destination are known. Lowering its base here would read pixels even
    // when a later span replaces all of its ink.
    if (std::optional<material::Material> surfaced =
            detail::surfacedForMaps(material)) {
      if (box == PaintBox::Padding || box == PaintBox::Content) {
        warnInkTakesNoFillBox();
        box = PaintBox::Element;
      }
      detail::CascadeData& cascade = declarations()->ink();
      if (cascade.font) cascade.font->ink.reset();
      cascade.inkVar.reset();
      cascade.inkPaint.reset();
      cascade.inkSurfaced = std::move(*surfaced);
      cascade.inkBox = box;
      cascade.statesInk = true;
      return self();
    }
  }
  ink(Fill::fromMaterial(material), box);
  // A material that states a surface, lit or unlit, keeps it for the
  // surface maps, painted as its colours at each node the ink reaches
  // otherwise.
  if (std::optional<material::Material> surfaced =
          detail::surfacedForMaps(material)) {
    detail::CascadeData& cascade = declarations()->ink();
    cascade.inkSurfaced = std::move(*surfaced);
    // A solid base still has mapped surface channels. Its colour lane
    // must preserve the material's requested text-unit domain.
    if (box == PaintBox::Padding || box == PaintBox::Content) {
      if (!cascade.inkPaint) warnInkTakesNoFillBox();
    } else {
      cascade.inkBox = box;
    }
  }
  if (effects) declarations()->cascadeData->inkEffects = std::move(effects);
  return self();
}

template <class Derived>
Derived& FontVerbs<Derived>::ink(Fill paint, PaintBox box) {
  detail::CascadeData& cascade = declarations()->ink();
  cascade.inkSurfaced.reset();
  // A PLAIN COLOUR is the ink lane, and has no unit square for a box to
  // stretch. A paint that happens to be flat is not one: it overrides the
  // glyphs of a leaf set in a style of its own, which an inherited colour
  // does not reach.
  if (paint.kind == Fill::Kind::Color && !paint.references())
    return ink(paint.colorValue);
  // A box the ink cannot stretch a paint over is read as the element's
  // own, and said so: the padding and the content rectangles place a
  // fill, and a text unit needs a passage to cut. A span always lands on
  // one, and a rule keeps its unit until it lands, where the cascade
  // drops it on a node that is no passage.
  if (box == PaintBox::Padding || box == PaintBox::Content) {
    warnInkTakesNoFillBox();
    box = PaintBox::Element;
  }
  constexpr bool landsOnText =
      std::is_same_v<Derived, Rule> || std::is_same_v<Derived, SpanStyle>;
  if (!landsOnText && detail::textUnitOf(box) &&
      declarations()->kind != detail::Kind::Text) {
    detail::warnInkTextUnitNeedsAPassage();
    box = PaintBox::Element;
  }
  // An empty paint STATES the lane and holds nothing, which clears an
  // ancestor's paint and leaves the colour in force standing.
  if (paint.kind == Fill::Kind::None) {
    cascade.statesInk = true;
    cascade.inkPaint.reset();
    cascade.inkBox = box;
    return self();
  }
  // A reference the slot cannot hold — the ink in force, a custom
  // property — leaves the ink exactly where it was, paint included: a
  // reference to the ink IS the ink. Nothing is written until there is
  // something to write, so a standing paint survives the asking.
  if (paint.kind != Fill::Kind::Paint) return self();
  cascade.statesInk = true;
  cascade.inkPaint = detail::paintOf(paint);
  cascade.inkBox = box;
  return self();
}

template class FontVerbs<Element>;
template class FontVerbs<Text>;
template class FontVerbs<Image>;
template class FontVerbs<Band>;
template class FontVerbs<Rule>;
template class FontVerbs<SpanStyle>;

}  // namespace sigil::compose
