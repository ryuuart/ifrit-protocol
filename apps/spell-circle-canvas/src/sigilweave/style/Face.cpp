/** @file
 * A face's sharing: the renderer's typeface is reference-counted, and a
 * `Face` holds one reference for as long as it stands.
 */

#include "sigilweave/style/Face.h"

#include <include/core/SkFontParameters.h>
#include <include/core/SkFontStyle.h>
#include <include/core/SkString.h>

#include "sigilweave/advanced/Skia.h"

namespace sigil::weave {

namespace {

const SkTypeface* typeface(const void* native) {
  return static_cast<const SkTypeface*>(native);
}

}  // namespace

Face::Face(const Face& other) : m_native(other.m_native) {
  SkSafeRef(typeface(m_native));
}

Face::Face(Face&& other) noexcept
    : m_native(std::exchange(other.m_native, nullptr)) {}

Face& Face::operator=(const Face& other) {
  if (this != &other) {
    SkSafeRef(typeface(other.m_native));
    SkSafeUnref(typeface(m_native));
    m_native = other.m_native;
  }
  return *this;
}

Face& Face::operator=(Face&& other) noexcept {
  if (this != &other) {
    SkSafeUnref(typeface(m_native));
    m_native = std::exchange(other.m_native, nullptr);
  }
  return *this;
}

Face::~Face() { SkSafeUnref(typeface(m_native)); }

std::string Face::familyName() const {
  if (!m_native) return {};
  SkString name;
  typeface(m_native)->getFamilyName(&name);
  return {name.c_str(), name.size()};
}

FaceStyle Face::style() const {
  if (!m_native) return {};
  return fromSk(typeface(m_native)->fontStyle());
}

std::vector<VariationAxis> Face::variationAxes() const {
  std::vector<VariationAxis> answered;
  if (!m_native) return answered;
  const int count = typeface(m_native)->getVariationDesignParameters({});
  if (count <= 0) return answered;
  std::vector<SkFontParameters::Variation::Axis> axes((size_t)count);
  typeface(m_native)->getVariationDesignParameters({axes.data(), axes.size()});
  answered.reserve(axes.size());
  for (const SkFontParameters::Variation::Axis& axis : axes) {
    VariationAxis entry;
    entry.tag[0] = (char)((axis.tag >> 24) & 0xFF);
    entry.tag[1] = (char)((axis.tag >> 16) & 0xFF);
    entry.tag[2] = (char)((axis.tag >> 8) & 0xFF);
    entry.tag[3] = (char)(axis.tag & 0xFF);
    entry.minimum = axis.min;
    entry.defaultValue = axis.def;
    entry.maximum = axis.max;
    entry.hidden = axis.isHidden();
    answered.push_back(entry);
  }
  return answered;
}

Face FaceAdapter<sk_sp<SkTypeface>>::wrap(sk_sp<SkTypeface> native) {
  Face face;
  face.m_native = reinterpret_cast<const Face::Handle*>(native.release());
  return face;
}

sk_sp<SkTypeface> FaceAdapter<sk_sp<SkTypeface>>::unwrap(const Face& face) {
  return sk_ref_sp(const_cast<SkTypeface*>(
      reinterpret_cast<const SkTypeface*>(face.m_native)));
}

SkTypeface* FaceAdapter<sk_sp<SkTypeface>>::borrow(const Face& face) {
  return const_cast<SkTypeface*>(
      reinterpret_cast<const SkTypeface*>(face.m_native));
}

SkFontStyle toSk(const FaceStyle& style) {
  const SkFontStyle::Slant slant =
      style.slant == FaceSlant::Italic    ? SkFontStyle::kItalic_Slant
      : style.slant == FaceSlant::Oblique ? SkFontStyle::kOblique_Slant
                                          : SkFontStyle::kUpright_Slant;
  return SkFontStyle(style.weight, style.width, slant);
}

FaceStyle fromSk(const SkFontStyle& style) {
  const FaceSlant slant =
      style.slant() == SkFontStyle::kItalic_Slant    ? FaceSlant::Italic
      : style.slant() == SkFontStyle::kOblique_Slant ? FaceSlant::Oblique
                                                     : FaceSlant::Upright;
  return {style.weight(), style.width(), slant};
}

}  // namespace sigil::weave
