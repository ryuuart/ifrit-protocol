#pragma once
// VerticalSpecimen.h — the chassis three vertical-CJK plates stand on.
//
// tategaki, bousen and mawarikomi study three disjoint paragraphs of the
// vertical writing mode — per-character ORIENTATION, what a column
// CARRIES, and what a column does under CONSTRAINT — so they are three
// sketches and not one. What they have no reason to state three times is
// the chassis underneath: the face the plates are set in, the two style
// registers a specimen page needs, the inks, and the block that puts a
// horizontal caption over a short vertical column.
//
// A caption is set HORIZONTALLY on purpose. A plate about a writing mode
// that labelled itself in the same mode would be showing one thing and
// arguing it in the same breath; setting the labels the other way puts
// the two modes side by side on the page.
//
// Two palettes, because a vertical plate is printed one of two ways and
// the names are the same words in both: ink on unbleached paper, or shell
// white on a sumi ground.

#include <include/core/SkColor.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkSize.h>
#include <include/core/SkTypeface.h>
#include <sigilcompose/core/Factories.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilmaterial/color/Color.h>
#include <sigilweave/layout/StyleSheet.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Style.h>
#include <sigilweave/style/Type.h>

#include <string_view>
#include <utility>

namespace vertical {

/** The canvas all three plates are drawn against. */
constexpr SkSize kSceneSize = {900, 640};

/** INK ON UNBLEACHED PAPER. */
namespace paper {
constexpr sigil::material::Color kKinari{0.937f, 0.918f, 0.878f, 1};  // unbleached paper
constexpr sigil::material::Color kKinariLift{0.961f, 0.945f, 0.909f, 1};
constexpr sigil::material::Color kSumi{0.114f, 0.106f, 0.098f, 1};  // ink
constexpr sigil::material::Color kAka{0.741f, 0.196f, 0.153f, 1};   // vermilion
constexpr sigil::material::Color kAi{0.192f, 0.302f, 0.404f, 1};    // indigo
constexpr sigil::material::Color kUsu{0.612f, 0.588f, 0.545f, 1};   // pale ink
}  // namespace paper

/** SHELL WHITE ON A SUMI GROUND — the same inks, printed the other way
 *  round, so the vermilion and the indigo have to lift off black rather
 *  than sit on paper. */
namespace ink {
constexpr sigil::material::Color kSumi{0.055f, 0.051f, 0.047f, 1};  // ink ground
constexpr sigil::material::Color kSumiLift{0.086f, 0.078f, 0.070f, 1};
constexpr sigil::material::Color kGofun{0.921f, 0.906f, 0.870f, 1};  // shell white
constexpr sigil::material::Color kAi{0.478f, 0.588f, 0.678f, 1};     // indigo
constexpr sigil::material::Color kAka{0.847f, 0.294f, 0.216f, 1};    // vermilion
}  // namespace ink

/** The mincho face the plates are set in, or whatever the platform
 *  offers. Resolved once: the fallback chain walks the system font list,
 *  which is not a per-frame cost anyone should pay. */
inline sk_sp<SkTypeface> mincho() {
  return sigil::weave::ports::face(
      {"Hiragino Mincho ProN", "Yu Mincho", "Songti SC", "Noto Serif CJK JP"});
}

/** THE BODY REGISTER: the passage itself, in the mincho face, tagged `ja`
 *  so the face's own Japanese behaviour is what shapes it. @p form is
 *  what decides per-character orientation and is left on auto unless a
 *  plate is studying that question. */
inline sigil::weave::TextStyle body(
    float size, sigil::material::Color color,
    sigil::weave::VerticalForm form = sigil::weave::VerticalForm::kAuto) {
  sigil::weave::TextStyle s =
      sigil::weave::textStyle({.size = size, .color = sigil::material::skia::toSkColor(color)});
  s.shaping.typeface = mincho();
  s.shaping.languageTag = "ja";
  s.shaping.verticalForm = form;
  return s;
}

/** THE BODY REGISTER AS A PARTIAL — what a reading beside the body, or a
 *  mark over it, is set in over the body's own style. */
inline sigil::weave::Type bodyType(
    float size, sigil::material::Color color,
    sigil::weave::VerticalForm form = sigil::weave::VerticalForm::kAuto) {
  sigil::weave::Type t{.face = mincho(), .size = size, .color = sigil::material::skia::toSkColor(color)};
  t.language = "ja";
  t.verticalForm = form;
  return t;
}
/** The body register as a partial with no colour of its own: set in the
 *  ink in force where the run lands. */
inline sigil::weave::Type bodyType(
    float size,
    sigil::weave::VerticalForm form = sigil::weave::VerticalForm::kAuto) {
  sigil::weave::Type t{.face = mincho(), .size = size};
  t.language = "ja";
  t.verticalForm = form;
  return t;
}

/** THE CAPTION REGISTER: Latin, horizontal, tracked open, in whatever the
 *  default family is — a label is not part of the setting it names. */
inline sigil::weave::TextStyle label(float size, sigil::material::Color color,
                                     float tracking = 0) {
  return sigil::weave::textStyle(
      {.size = size, .color = sigil::material::skia::toSkColor(color), .track = tracking});
}
/** The same register as a partial, for a caption set over what its cell
 *  inherits. */
inline sigil::weave::Type labelType(float size, sigil::material::Color color,
                                    float tracking = 0) {
  return {.size = size, .color = sigil::material::skia::toSkColor(color), .track = tracking};
}
/** The caption register as a partial with no colour of its own: set in
 *  the ink in force where the run lands. */
inline sigil::weave::Type labelType(float size, float tracking = 0) {
  return {.size = size, .track = tracking};
}

/** A CAPTION OVER A SHORT VERTICAL COLUMN.
 *
 *  The column is handed in already built, because what each plate is
 *  showing lives inside that element — a run wearing one decoration, a
 *  run asking for vertical alternates, a run clamped to one column with a
 *  marker at its foot. What the block owns is only the arrangement: the
 *  caption above, the column below, and the measure the caption wraps to
 *  (@p captionWidth of 0 lets it take its own). */
inline sigil::compose::Element specimen(std::string_view caption,
                                        const sigil::weave::Type& style,
                                        sigil::compose::Element column,
                                        float captionWidth = 0.0f,
                                        float gap = 8.0f) {
  // A caption stacked over its body is the compose kit's cell in its
  // type-specimen reading. The cell sets its label in the role
  // `label`, so the register this plate states IS that role on the
  // cell that writes it.
  return sigil::compose::kit::cell(
             {.where = sigil::compose::kit::Caption::Where::Above,
              .gap = gap,
              .labelMeasure = captionWidth},
             caption, {}, std::move(column))
      .styleSheet(sigil::weave::StyleSheet{{"label", style}});
}

}  // namespace vertical
