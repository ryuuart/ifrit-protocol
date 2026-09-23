/** @file
 * The face a family name and an italic choose: the family found through
 * the font context, its italic face or axis, and the oblique that stands
 * in where the family has neither.
 */

#include "FaceChoice.h"

#include <include/core/SkFontParameters.h>
#include <include/core/SkFontStyle.h>
#include <include/core/SkString.h>
#include <include/core/SkTypes.h>  // SkDebugf — the face choice's diagnostics

#include <algorithm>
#include <boost/unordered/unordered_flat_set.hpp>
#include <cmath>
#include <cstring>
#include <vector>

namespace sigil::compose::detail {

namespace {

/** The lean CSS gives an oblique that names no angle, and the one an
 *  italic stands in with where the family has no italic of any kind. */
constexpr float kObliqueDegrees = 14.0f;

void warnNoSuchFamily(const std::string& family) {
  static thread_local boost::unordered_flat_set<std::string> warned;
  if (!warned.insert(family).second) return;
  SkDebugf(
      "[compose] fontFamily(\"%s\") names a family the font context cannot "
      "find — the face in force was left standing. Name a family installed "
      "where the composer runs. (warned once)\n",
      family.c_str());
}

void warnNoItalic(const SkTypeface& face) {
  SkString name;
  face.getFamilyName(&name);
  static thread_local boost::unordered_flat_set<std::string> warned;
  if (!warned.insert(name.c_str()).second) return;
  SkDebugf(
      "[compose] fontStyle(Italic) over \"%s\", which has no italic face and "
      "no ital axis — it is set as an oblique of 14 degrees instead, which "
      "leans only a face with a slnt axis. (warned once)\n",
      name.c_str());
}

bool isItalicFace(const SkTypeface& face) {
  return face.fontStyle().slant() != SkFontStyle::kUpright_Slant;
}

bool hasItalicAxis(const SkTypeface& face) {
  const int count = face.getVariationDesignParameters({});
  if (count <= 0) return false;
  std::vector<SkFontParameters::Variation::Axis> axes(
      static_cast<size_t>(count));
  if (face.getVariationDesignParameters({axes.data(), axes.size()}) != count)
    return false;
  return std::any_of(axes.begin(), axes.end(), [](const auto& axis) {
    return axis.tag == SkSetFourByteTag('i', 't', 'a', 'l');
  });
}

/** The coordinate @p font carries on the axis @p tag, replaced where it
 *  stands or appended — or, with @p onlyIfPresent, written only over one
 *  already there. */
void setAxis(sigil::weave::Type& font, const char (&tag)[5], float value,
             bool onlyIfPresent) {
  for (sigil::weave::FontVariation& variation : font.variations)
    if (std::memcmp(variation.tag, tag, 4) == 0) {
      variation.value = value;
      return;
    }
  if (!onlyIfPresent) font.variations.emplace_back(tag, value);
}

/** Whether @p face is the one its own family's name finds at its style —
 *  an installed face — rather than a file's that happens to share an
 *  installed family's name. */
bool isFamilyMember(sigil::weave::FontContext& fonts, const SkTypeface& face,
                    const SkString& family) {
  const sk_sp<SkTypeface> member =
      fonts.familyTypeface(family.c_str(), face.fontStyle());
  if (!member) return false;
  if (member.get() == &face) return true;
  SkString own, found;
  return face.getPostScriptName(&own) && member->getPostScriptName(&found) &&
         own == found;
}

/** The coordinate @p style sets on the axis @p tag, if it sets one. */
std::optional<float> axisOf(const sigil::weave::TextStyle& style,
                            const char (&tag)[5]) {
  for (const sigil::weave::FontVariation& variation : style.shaping.variations)
    if (std::memcmp(variation.tag, tag, 4) == 0) return variation.value;
  return std::nullopt;
}

}  // namespace

void chooseFace(sigil::weave::FontContext& fonts, sigil::weave::Type& font,
                const std::string* family, bool italic) {
  const sk_sp<SkTypeface> inForce =
      font.face && *font.face ? *font.face : fonts.defaultTypeface();
  const SkFontStyle own =
      inForce ? inForce->fontStyle() : SkFontStyle::Normal();
  const int weight = font.weight && *font.weight > 0.0f
                         ? static_cast<int>(std::lround(*font.weight))
                         : own.weight();
  const SkFontStyle wanted(
      weight, own.width(),
      italic ? SkFontStyle::kItalic_Slant : SkFontStyle::kUpright_Slant);
  sk_sp<SkTypeface> chosen;
  if (family != nullptr) {
    chosen = fonts.familyTypeface(*family, wanted);
    if (!chosen) warnNoSuchFamily(*family);
  }
  // No family found, or none named: the face in force stands unless it
  // leans the wrong way, when its own family's other face is asked for.
  // A face that is not its family's own — a file's — has no other face.
  if (!chosen && inForce) {
    if (isItalicFace(*inForce) != italic) {
      SkString name;
      inForce->getFamilyName(&name);
      if (isFamilyMember(fonts, *inForce, name))
        chosen = fonts.familyTypeface(name.c_str(), wanted);
    }
    if (!chosen) chosen = inForce;
  }
  if (!chosen) return;
  font.face = chosen;
  if (!italic) {
    // An italic axis set above is set back, and one the range's style
    // carries is overridden by the partial that says so.
    setAxis(font, "ital", 0.0f, /*onlyIfPresent=*/!hasItalicAxis(*chosen));
    return;
  }
  // AN ITALIC IS A FACE, then an axis, then a lean: whichever the family
  // has first, and the italic replaces any lean stated above it.
  if (isItalicFace(*chosen)) {
    font.slant = 0.0f;
  } else if (hasItalicAxis(*chosen)) {
    setAxis(font, "ital", 1.0f, /*onlyIfPresent=*/false);
    font.slant = 0.0f;
  } else {
    warnNoItalic(*chosen);
    font.slant = -kObliqueDegrees;
  }
}

void chooseRangeFace(sigil::weave::FontContext& fonts,
                     sigil::weave::Type& partial,
                     const sigil::weave::TextStyle& base,
                     const std::string* familyInForce, bool italicInForce,
                     const std::string* family, std::optional<bool> italic) {
  const bool italicNow = italic.value_or(italicInForce);
  // A face the partial states stands over the leaf's family; a family it
  // names stands over both.
  const std::string* named =
      family != nullptr ? family : (partial.face ? nullptr : familyInForce);
  if (family == nullptr && italicNow == italicInForce &&
      !(partial.face && italicNow) && !(named != nullptr && partial.weight))
    return;
  sigil::weave::Type chosen = partial;
  if (!chosen.face) chosen.face = base.shaping.typeface;
  if (!chosen.weight) chosen.weight = axisOf(base, "wght");
  chooseFace(fonts, chosen, named, italicNow);
  // A style's lean of zero writes no axis over the one the range is set
  // at, so an italic replacing a lean there says the zero itself.
  if (chosen.slant && *chosen.slant == 0.0f &&
      axisOf(base, "slnt").value_or(0.0f) != 0.0f)
    setAxis(chosen, "slnt", 0.0f, /*onlyIfPresent=*/false);
  partial.face = chosen.face;
  partial.slant = chosen.slant;
  partial.variations = chosen.variations;
}

}  // namespace sigil::compose::detail
