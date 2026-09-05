#pragma once

/** @file
 * Single-span style shorthand and the one-call caption/label draw that
 * every SigilWeave-based tool reinvents for its annotations and HUDs.
 */

#include <include/core/SkCanvas.h>
#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/style/Style.h>
#include <sigilweave/style/Type.h>

#include <string_view>

namespace sigil::weave::kit {

/** Creates a single-span TextStyle: size, flat foreground color, optional
 *  language tag and typeface (null → the context default). */
[[nodiscard]] sigil::weave::TextStyle makeStyle(
    float fontSize, SkColor color, const char* language = "",
    sk_sp<SkTypeface> typeface = nullptr);

/** A STYLE WHOSE TRACKING IS QUOTED IN 1/1000 EM — the unit a reference
 *  from Illustrator, Flash or a type specimen states it in, converted to
 *  the px a text style takes.
 *
 *  The conversion needs the em size, which is why it belongs beside the
 *  style rather than in the number: `30` at 18 px and `30` at 60 px are
 *  different distances, and a reconstruction that copied the px would be
 *  wrong at every other size.
 *
 *      kit::tracked(grotBold(), 18, kNear, 30, 0.96f)
 *
 *  @p condense is horizontal scale, for a face with no `wdth` axis to
 *  ask instead. */
[[nodiscard]] inline TextStyle tracked(const sk_sp<SkTypeface>& face,
                                       float size, SkColor4f color,
                                       float trackPerMille = 0,
                                       float condense = 1.0f) {
  return textStyle({.face = face,
                    .size = size,
                    .color = color,
                    .track = size * trackPerMille / 1000.0f,
                    .condense = condense});
}

/** Appearance of a drawLabel() caption; the defaults suit a small
 *  single-line annotation under a scene. */
struct LabelOptions {
  float fontSize = 12.0f;
  SkColor color = SK_ColorBLACK;
  float width = 520.0f;  ///< wrap measure of the label's block flow
  float height = 32.0f;  ///< block height; two 12px lines by default
  const char* language = "";
  sk_sp<SkTypeface> typeface;  ///< null → the context default
};

/** Draws a short explanatory caption in one call: builds a single-span
 *  paragraph, flows it into a small block at `origin`, and draws it.
 *
 *  This shapes the text on every call — right for annotations drawn a
 *  handful of times per frame. Text hot enough to matter belongs in a
 *  sigil::weave::SingleLineParagraphCache or behind a RebuildGuard instead. */
void drawLabel(SkCanvas* canvas, sigil::weave::FontContext& fontContext,
               std::u8string_view text, SkPoint origin,
               const LabelOptions& options = {});

/** UTF-16 variant: lets UTF-16 sources (QString via sigil::weave::qt::toU16,
 *  std::u16string) feed a label without transcoding. */
void drawLabel(SkCanvas* canvas, sigil::weave::FontContext& fontContext,
               std::u16string_view text, SkPoint origin,
               const LabelOptions& options = {});

}  // namespace sigil::weave::kit
