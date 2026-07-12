#pragma once

// Qt ↔ TextFlow/Skia bridging. Lives in its own tiny interface target
// (TextFlowQt) so the core TextFlow library stays Qt-free — link this only
// from Qt applications.
//
// QString and Paragraph both store UTF-16, so text crosses the bridge as a
// zero-copy view — no transcoding, no allocation.

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

// ── Text (zero-copy: QString is UTF-16, and so is Paragraph) ─────────────

// Borrowed view of a QString's storage: valid only while `s` is alive and
// unmodified.
inline std::u16string_view toU16(const QString &s) {
  return {reinterpret_cast<const char16_t *>(s.constData()),
          static_cast<size_t>(s.size())};
}

inline QString toQString(std::u16string_view s) {
  return QString::fromUtf16(s.data(), static_cast<qsizetype>(s.size()));
}

inline void appendText(textflow::Paragraph &para, const QString &text,
                       const textflow::TextStyle &style) {
  para.appendText(toU16(text), style);
}

inline void replaceText(textflow::Paragraph &para, uint32_t start,
                        uint32_t end, const QString &text) {
  // Paragraph::replaceText takes UTF-8; convert once, no extra copies.
  const QByteArray utf8 = text.toUtf8();
  para.replaceText(start, end,
                   std::string_view(utf8.constData(),
                                    static_cast<size_t>(utf8.size())));
}

inline std::vector<textflow::CharRange> findAll(const textflow::Paragraph &para,
                                                const QString &needle) {
  return textflow::findAll(para, toU16(needle));
}

// ── Geometry / color ─────────────────────────────────────────────────────

inline SkColor toSkColor(const QColor &c) {
  return SkColorSetARGB(c.alpha(), c.red(), c.green(), c.blue());
}

inline SkPoint toSkPoint(const QPointF &p) {
  return {static_cast<float>(p.x()), static_cast<float>(p.y())};
}

inline SkRect toSkRect(const QRectF &r) {
  return SkRect::MakeXYWH(static_cast<float>(r.x()),
                          static_cast<float>(r.y()),
                          static_cast<float>(r.width()),
                          static_cast<float>(r.height()));
}

// ── Fonts ────────────────────────────────────────────────────────────────

// QFont::weight() is already on the 1–900 (Thin..Black) scale SkFontStyle
// uses, so it carries straight through.
inline SkFontStyle toSkFontStyle(const QFont &font) {
  SkFontStyle::Slant slant = SkFontStyle::kUpright_Slant;
  if (font.style() == QFont::StyleItalic)
    slant = SkFontStyle::kItalic_Slant;
  else if (font.style() == QFont::StyleOblique)
    slant = SkFontStyle::kOblique_Slant;
  return SkFontStyle(static_cast<int>(font.weight()),
                     SkFontStyle::kNormal_Width, slant);
}

// Resolves the QFont's family + style through the given font manager.
// An empty family yields the platform default. May return null when the
// manager has no match at all — callers should fall back to their default
// typeface (e.g. FontContext::defaultTypeface()).
inline sk_sp<SkTypeface> toSkTypeface(SkFontMgr *mgr, const QFont &font) {
  if (!mgr)
    return nullptr;
  const QByteArray family = font.family().toUtf8();
  return mgr->matchFamilyStyle(family.isEmpty() ? nullptr : family.constData(),
                               toSkFontStyle(font));
}

} // namespace textflowqt
