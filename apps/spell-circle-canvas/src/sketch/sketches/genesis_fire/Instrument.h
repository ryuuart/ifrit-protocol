#pragma once
// The type chassis an INSTRUMENT PANEL is set in — the sidebar of numbers
// a study puts beside its picture.
//
// Two faces and their bold cuts: a terminal face for everything measured,
// and the system grotesque for everything named. A panel of readings is
// set in exactly these registers whatever it is measuring, so the plate
// beside a fire, a rig or an exposure fit reads as one instrument rather
// than as three that happen to look alike.
//
// The registers only. Where a panel STANDS, what it is grounded in and
// what it says are each study's own, and none of that belongs here.

#include <include/core/SkColor.h>
#include <include/core/SkFontStyle.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkTypeface.h>
#include <sigilcompose/core/Element.h>
#include <sigilcompose/core/Factories.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

#include <string_view>
#include <utility>

namespace instrument {

/** The measured face, and its bold cut. */
inline sk_sp<SkTypeface> monoFace() {
  return sigil::weave::ports::face({"Menlo"}, SkFontStyle::Normal());
}
inline sk_sp<SkTypeface> monoBoldFace() {
  return sigil::weave::ports::face({"Menlo"}, SkFontStyle::Bold());
}
/** The named face, its bold cut, and the black cut a masthead takes. */
inline sk_sp<SkTypeface> uiFace() {
  return sigil::weave::ports::face({"Helvetica Neue"}, SkFontStyle::Normal());
}
inline sk_sp<SkTypeface> uiBoldFace() {
  return sigil::weave::ports::face({"Helvetica Neue"}, SkFontStyle::Bold());
}
inline sk_sp<SkTypeface> heavyFace() {
  return sigil::weave::ports::face({"Helvetica Neue", "Helvetica"},
                                   SkFontStyle::kBlack_Weight);
}

/** A positional shorthand over the library's designated-init `textStyle`,
 *  for the one display line that names its own face. */
inline sigil::weave::TextStyle faced(sk_sp<SkTypeface> face, float size,
                                     SkColor4f color, float track = 0.0f) {
  return sigil::weave::textStyle(
      {.face = std::move(face), .size = size, .color = color, .track = track});
}

/** The registers a panel is set in. */
inline sigil::weave::TextStyle mono(float size, SkColor4f color,
                                    float track = 0.0f) {
  return faced(monoFace(), size, color, track);
}
inline sigil::weave::TextStyle monoB(float size, SkColor4f color,
                                     float track = 0.0f) {
  return faced(monoBoldFace(), size, color, track);
}
inline sigil::weave::TextStyle ui(float size, SkColor4f color,
                                  float track = 0.0f) {
  return faced(uiFace(), size, color, track);
}
inline sigil::weave::TextStyle uiB(float size, SkColor4f color,
                                   float track = 0.0f) {
  return faced(uiBoldFace(), size, color, track);
}

/** One line of the panel. */
inline sigil::compose::Element t(std::string_view line,
                                 sigil::weave::TextStyle style) {
  return sigil::compose::text(sigil::compose::toUtf8(line), std::move(style));
}

/** The same register as a PEN's type, for the study that draws its panel
 *  rather than describing it: a pen carries one type and one fill, so a
 *  register is set on it rather than handed to a node. */
inline sigil::weave::Type penType(const sk_sp<SkTypeface>& face, float size,
                                  float track = 0.0f) {
  return sigil::weave::Type{.face = face, .size = size, .track = track};
}

}  // namespace instrument
