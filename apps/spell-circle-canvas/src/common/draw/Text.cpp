/** @file
 * The pen's text: shaped and laid out by SigilWeave, coloured by the
 * fill, stroked by the stroke once one is set.
 */

#include <include/core/SkFont.h>
#include <include/core/SkFontMetrics.h>
#include <include/core/SkFontMgr.h>
#include <include/core/SkFontStyle.h>
#include <include/core/SkString.h>
#include <sigildraw/Pen.h>
#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/layout/Flow.h>
#include <sigilweave/layout/LayoutOptions.h>
#include <sigilweave/layout/ParagraphLayout.h>
#include <sigilweave/paragraph/Paragraph.h>
#include <sigilweave/style/PaintLayer.h>
#include <sigilweave/style/PaintStyle.h>

#include <charconv>
#include <cmath>
#include <string>
#include <string_view>
#include <vector>

namespace sigil::draw {

namespace {

std::u8string_view utf8(std::string_view text) {
  return {reinterpret_cast<const char8_t*>(text.data()), text.size()};
}

SkFontStyle fontStyleOf(Constant style) {
  switch (style) {
    case BOLD:
      return SkFontStyle::Bold();
    case ITALIC:
      return SkFontStyle::Italic();
    case BOLDITALIC:
      return SkFontStyle::BoldItalic();
    default:
      return SkFontStyle::Normal();
  }
}

std::vector<std::string_view> linesOf(std::string_view text) {
  std::vector<std::string_view> lines;
  size_t start = 0;
  while (true) {
    const size_t at = text.find('\n', start);
    if (at == std::string_view::npos) {
      lines.push_back(text.substr(start));
      return lines;
    }
    lines.push_back(text.substr(start, at - start));
    start = at + 1;
  }
}

}  // namespace

sk_sp<SkTypeface> Pen::face() {
  if (m_style.type.face) return m_style.type.face;
  if (m_style.matched) return m_style.matched;
  if (!m_fonts) return nullptr;
  const sk_sp<SkTypeface>& base = m_fonts->defaultTypeface();
  if (m_style.family.empty() && m_style.textStyle == NORMAL) return base;
  // A named family, or the default one in another weight or slant, is
  // matched through the manager and the answer kept until the family or
  // the style changes.
  std::string family = m_style.family;
  if (family.empty() && base) {
    SkString name;
    base->getFamilyName(&name);
    family = name.c_str();
  }
  sk_sp<SkTypeface> found;
  if (SkFontMgr* manager = m_fonts->fontManager())
    found = manager->matchFamilyStyle(family.c_str(),
                                      fontStyleOf(m_style.textStyle));
  m_style.matched = found ? found : base;
  return m_style.matched;
}

weave::TextStyle Pen::textStyleNow() {
  weave::Type type = m_style.type;
  type.face = face();
  return weave::textStyle(type);
}

void Pen::textSize(float size) {
  m_style.type.size = size;
  m_style.leading = size * 1.25f;
}

void Pen::textFont(std::string_view family) {
  m_style.family.assign(family);
  m_style.type.face = nullptr;
  m_style.matched = nullptr;
}

void Pen::textFont(std::string_view family, float size) {
  textFont(family);
  textSize(size);
}

void Pen::textFont(sk_sp<SkTypeface> typeface) {
  m_style.family.clear();
  m_style.type.face = std::move(typeface);
  m_style.matched = nullptr;
}

void Pen::textFont(const weave::Type& type) {
  m_style.type = type;
  m_style.family.clear();
  m_style.matched = nullptr;
  m_style.leading = type.size * 1.25f;
}

void Pen::textAlign(Constant horizontal) { m_style.textAlignX = horizontal; }

void Pen::textAlign(Constant horizontal, Constant vertical) {
  m_style.textAlignX = horizontal;
  m_style.textAlignY = vertical;
}

void Pen::textLeading(float leading) { m_style.leading = leading; }

float Pen::textLeading() const { return m_style.leading; }

void Pen::textStyle(Constant style) {
  m_style.textStyle = style;
  m_style.matched = nullptr;
}

float Pen::textAscent() {
  sk_sp<SkTypeface> typeface = face();
  if (!typeface) return 0.0f;
  SkFontMetrics metrics;
  SkFont(typeface, m_style.type.size).getMetrics(&metrics);
  return -metrics.fAscent;
}

float Pen::textDescent() {
  sk_sp<SkTypeface> typeface = face();
  if (!typeface) return 0.0f;
  SkFontMetrics metrics;
  SkFont(typeface, m_style.type.size).getMetrics(&metrics);
  return metrics.fDescent;
}

float Pen::textWidth(std::string_view str) {
  if (!m_fonts) return 0.0f;
  float widest = 0.0f;
  const weave::TextStyle style = textStyleNow();
  for (std::string_view line : linesOf(str)) {
    weave::Paragraph paragraph;
    paragraph.appendText(utf8(line), style);
    widest = std::max(widest, paragraph.naturalWidth(*m_fonts));
  }
  return widest;
}

namespace {

/** The paint the glyphs are drawn with, in place of the style's own:
 *  the fill on top, the stroke beneath it as p5 draws them. p5 fills
 *  text black until a fill is set and strokes it only once a stroke is,
 *  so a fresh pen's text is black ink and not a white blob with a black
 *  outline. */
weave::PaintStyle glyphPaint(bool doFill, bool fillSet, const SkPaint* fill,
                             bool strokeSet, const SkPaint* stroke,
                             bool antiAlias) {
  weave::PaintStyle over;
  if (!doFill) {
    over.foreground.setColor4f({0, 0, 0, 0}, nullptr);
  } else if (fillSet && fill) {
    over.foreground = *fill;
  } else {
    over.foreground.setColor(SK_ColorBLACK);
  }
  over.foreground.setAntiAlias(antiAlias);
  if (strokeSet && stroke) over.addUnderlay(weave::PaintLayer(*stroke));
  return over;
}

}  // namespace

void Pen::textLine(std::string_view line, float x, float baseline) {
  weave::Paragraph paragraph;
  paragraph.appendText(utf8(line), textStyleNow());
  float x0 = x;
  if (m_style.textAlignX != LEFT) {
    const float w = paragraph.naturalWidth(*m_fonts);
    x0 = m_style.textAlignX == CENTER ? x - w / 2.0f : x - w;
  }
  const weave::ParagraphLayout layout =
      weave::layoutSingleLine(*m_fonts, paragraph, {x0, baseline});
  weave::PaintStyle over =
      glyphPaint(m_style.doFill, m_style.fillSet, fillPaint(),
                 m_style.strokeSet, strokePaint(), m_style.antiAlias);
  // The colours a fill was never set for are built here rather than
  // copied from a paint, so the blend has to be put on them.
  blendInto(over.foreground);
  layout.draw(m_canvas, paragraph, &over);
}

void Pen::text(std::string_view str, float x, float y) {
  // A glyph adds nothing to a mask being recorded.
  if (!m_canvas || !m_fonts || m_clipRecording) return;
  const std::vector<std::string_view> lines = linesOf(str);
  const float ascent = textAscent();
  const float descent = textDescent();
  const float leading = m_style.leading;
  // A block of several lines is placed by its vertical alignment as one
  // thing, then each line's baseline is seated the way a canvas seats a
  // single one.
  float top = y;
  if (m_style.textAlignY == CENTER)
    top -= (float)(lines.size() - 1) * leading / 2.0f;
  else if (m_style.textAlignY == BOTTOM)
    top -= (float)(lines.size() - 1) * leading;
  float baseline = top;
  switch (m_style.textAlignY) {
    case TOP:
      baseline = top + ascent;
      break;
    case CENTER:
      baseline = top + (ascent - descent) / 2.0f;
      break;
    case BOTTOM:
      baseline = top - descent;
      break;
    default:
      break;
  }
  for (size_t i = 0; i < lines.size(); ++i)
    textLine(lines[i], x, baseline + (float)i * leading);
}

void Pen::text(std::string_view str, float x, float y, float w, float h) {
  if (!m_canvas || !m_fonts || m_clipRecording) return;
  const SkRect box = rectBox(x, y, w, h);
  weave::Paragraph paragraph;
  paragraph.appendText(utf8(str), textStyleNow());
  weave::BlockFlow flow(box);
  weave::ParagraphLayoutOptions options;
  options.alignment =
      m_style.textAlignX == CENTER  ? weave::TextAlignment::kCenter
      : m_style.textAlignX == RIGHT ? weave::TextAlignment::kEnd
                                    : weave::TextAlignment::kStart;
  options.lineMetrics.height = m_style.leading;
  options.frame.distribute =
      m_style.textAlignY == CENTER   ? weave::FrameOptions::Distribute::kCenter
      : m_style.textAlignY == BOTTOM ? weave::FrameOptions::Distribute::kEnd
                                     : weave::FrameOptions::Distribute::kStart;
  // THE BOX IS THE EXTENT. Distributing what the passage left over is a
  // question about how deep the frame is, and the layout cannot see the
  // geometry it was handed — so a distribution over an extent of zero
  // has no room to place and returns at once, seating the middle and the
  // foot exactly where the top would be. The pen was handed the depth.
  options.frame.extent = box.height();
  const weave::ParagraphLayout layout =
      weave::layoutParagraph(*m_fonts, paragraph, flow, options);
  weave::PaintStyle over =
      glyphPaint(m_style.doFill, m_style.fillSet, fillPaint(),
                 m_style.strokeSet, strokePaint(), m_style.antiAlias);
  blendInto(over.foreground);
  layout.draw(m_canvas, paragraph, &over);
}

void Pen::text(double value, float x, float y) {
  // The shortest spelling that reads back as the same number, which is
  // how a script prints one: 3 for 3.0, 0.1 for 0.1.
  char buffer[32];
  const auto [end, error] =
      std::to_chars(buffer, buffer + sizeof buffer, value);
  if (error != std::errc{}) return;
  text(std::string_view(buffer, (size_t)(end - buffer)), x, y);
}

}  // namespace sigil::draw
