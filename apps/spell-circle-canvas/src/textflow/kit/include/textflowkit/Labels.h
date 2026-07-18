#pragma once

/** @file
 * Single-span style shorthand and the one-call caption/label draw that
 * every TextFlow-based tool reinvents for its annotations and HUDs.
 */

#include <textflow/FontContext.h>
#include <textflow/Style.h>

#include <include/core/SkCanvas.h>

#include <string_view>

namespace textflowkit {

/** Creates a single-span TextStyle: size, flat foreground color, optional
 *  language tag and typeface (null → the context default). */
[[nodiscard]] textflow::TextStyle makeStyle(float fontSize, SkColor color,
                                            const char *language = "",
                                            sk_sp<SkTypeface> typeface =
                                                nullptr);

/** Appearance of a drawLabel() caption; the defaults suit a small
 *  single-line annotation under a scene. */
struct LabelOptions {
  float fontSize = 12.0f;
  SkColor color = SK_ColorBLACK;
  float width = 520.0f;  ///< wrap measure of the label's block flow
  float height = 32.0f;  ///< block height; two 12px lines by default
  const char *language = "";
  sk_sp<SkTypeface> typeface; ///< null → the context default
};

/** Draws a short explanatory caption in one call: builds a single-span
 *  paragraph, flows it into a small block at `origin`, and draws it.
 *
 *  This shapes the text on every call — right for annotations drawn a
 *  handful of times per frame. Text hot enough to matter belongs in a
 *  textflow::SingleLineParagraphCache or behind a RebuildGuard instead. */
void drawLabel(SkCanvas *canvas, textflow::FontContext &fontContext,
               std::u8string_view text, SkPoint origin,
               const LabelOptions &options = {});

/** UTF-16 variant: lets UTF-16 sources (QString via textflowqt::toU16,
 *  std::u16string) feed a label without transcoding. */
void drawLabel(SkCanvas *canvas, textflow::FontContext &fontContext,
               std::u16string_view text, SkPoint origin,
               const LabelOptions &options = {});

} // namespace textflowkit
