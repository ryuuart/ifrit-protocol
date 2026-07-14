#pragma once

/** @file
 * Qt ↔ TextFlow/Skia bridging. Lives in its own tiny interface target
 * (TextFlowQt) so the core TextFlow library stays Qt-free — link this only
 * from Qt applications.
 *
 * QString and Paragraph both store UTF-16, so text crosses the bridge as a
 * zero-copy view — no transcoding, no allocation.
 */

#include <textflow/Paragraph.h>
#include <textflow/Query.h>

#include <QColor>
#include <QFont>
#include <QPointF>
#include <QRectF>
#include <QString>

#include <include/core/SkColor.h>
#include <include/core/SkFontMgr.h>
#include <include/core/SkFontStyle.h>
#include <include/core/SkPoint.h>
#include <include/core/SkRect.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkTypeface.h>

#include <string_view>

namespace textflowqt {

/** @name Text (zero-copy: QString is UTF-16, and so is Paragraph)
 *  @{
 */

/** Returns a borrowed view of a QString's storage: valid only while `text`
 * is alive and unmodified.
 */
inline std::u16string_view toU16(const QString &text) {
  return {reinterpret_cast<const char16_t *>(text.constData()),
          static_cast<size_t>(text.size())};
}

/** Copies a UTF-16 view into a QString. */
inline QString toQString(std::u16string_view text) {
  return QString::fromUtf16(text.data(), static_cast<qsizetype>(text.size()));
}

/** Appends QString text without transcoding its UTF-16 storage. */
inline void appendText(textflow::Paragraph &paragraph, const QString &text,
                       const textflow::TextStyle &style) {
  paragraph.appendText(toU16(text), style);
}

/** Replaces a paragraph range with QString text. */
inline void replaceText(textflow::Paragraph &paragraph, uint32_t start,
                        uint32_t end, const QString &text) {
  // Paragraph::replaceText takes UTF-8; convert once, no extra copies.
  const QByteArray utf8 = text.toUtf8();
  paragraph.replaceText(
      start, end,
      std::u8string_view(reinterpret_cast<const char8_t *>(utf8.constData()),
                         static_cast<size_t>(utf8.size())));
}

/** Finds every occurrence of `needle` without transcoding it. */
inline std::vector<textflow::CharRange>
findAllOccurrences(const textflow::Paragraph &paragraph,
                   const QString &needle) {
  return textflow::findAllOccurrences(paragraph, toU16(needle));
}

/** @} */

/** @name Geometry / color
 *  @{
 */

/** Converts a QColor to its unpremultiplied Skia color value. */
inline SkColor toSkColor(const QColor &color) {
  return SkColorSetARGB(color.alpha(), color.red(), color.green(),
                        color.blue());
}

/** Converts a QPointF to a float SkPoint. */
inline SkPoint toSkPoint(const QPointF &point) {
  return {static_cast<float>(point.x()), static_cast<float>(point.y())};
}

/** Converts a QRectF to a float SkRect. */
inline SkRect toSkRect(const QRectF &rectangle) {
  return SkRect::MakeXYWH(static_cast<float>(rectangle.x()),
                          static_cast<float>(rectangle.y()),
                          static_cast<float>(rectangle.width()),
                          static_cast<float>(rectangle.height()));
}

/** @} */

/** @name Fonts
 *  @{
 */

/** Converts a QFont's weight/slant to an SkFontStyle. QFont::weight() is
 * already on the 1–900 (Thin..Black) scale SkFontStyle uses, so it carries
 * straight through.
 */
inline SkFontStyle toSkFontStyle(const QFont &font) {
  SkFontStyle::Slant slant = SkFontStyle::kUpright_Slant;
  if (font.style() == QFont::StyleItalic)
    slant = SkFontStyle::kItalic_Slant;
  else if (font.style() == QFont::StyleOblique)
    slant = SkFontStyle::kOblique_Slant;
  return SkFontStyle(static_cast<int>(font.weight()),
                     SkFontStyle::kNormal_Width, slant);
}

/** Resolves the QFont's family + style through the given font manager.
 *
 * An empty family yields the platform default. May return null when the
 * manager has no match at all — callers should fall back to their default
 * typeface (e.g. FontContext::defaultTypeface()).
 */
inline sk_sp<SkTypeface> toSkTypeface(SkFontMgr *fontManager,
                                      const QFont &font) {
  if (!fontManager)
    return nullptr;
  const QByteArray family = font.family().toUtf8();
  return fontManager->matchFamilyStyle(
      family.isEmpty() ? nullptr : family.constData(), toSkFontStyle(font));
}

/** @} */

} // namespace textflowqt
