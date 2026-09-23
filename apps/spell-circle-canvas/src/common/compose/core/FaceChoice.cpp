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

/** The `ital` coordinate @p font carries, replaced where it stands or
 *  appended — or, with @p onlyIfPresent, written only over one already
 *  there. */
void setItalicAxis(sigil::weave::Type& font, float value, bool onlyIfPresent) {
  for (sigil::weave::FontVariation& variation : font.variations)
    if (std::memcmp(variation.tag, "ital", 4) == 0) {
      variation.value = value;
      return;
    }
  if (!onlyIfPresent) font.variations.emplace_back("ital", value);
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
  if (!chosen && inForce) {
    if (isItalicFace(*inForce) != italic) {
      SkString name;
      inForce->getFamilyName(&name);
      chosen = fonts.familyTypeface(name.c_str(), wanted);
    }
    if (!chosen) chosen = inForce;
  }
  if (!chosen) return;
  font.face = chosen;
  if (!italic) {
    setItalicAxis(font, 0.0f, /*onlyIfPresent=*/true);
    return;
  }
  // AN ITALIC IS A FACE, then an axis, then a lean: whichever the family
  // has first, and the italic replaces any lean stated above it.
  if (isItalicFace(*chosen)) {
    font.slant = 0.0f;
  } else if (hasItalicAxis(*chosen)) {
    setItalicAxis(font, 1.0f, /*onlyIfPresent=*/false);
    font.slant = 0.0f;
  } else {
    warnNoItalic(*chosen);
    font.slant = -kObliqueDegrees;
  }
}

}  // namespace sigil::compose::detail
