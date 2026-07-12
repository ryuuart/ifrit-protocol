#include "SkiaSceneBackend.h"
#include "SkiaGraphiteContext.h"
#include "SkiaOffscreenSurface.h"
#include "SpellCircleRenderer.h"

#include <include/core/SkBlendMode.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>
#include <include/core/SkFont.h>
#include <include/core/SkFontMetrics.h>
#include <include/core/SkFontMgr.h>
#include <include/core/SkFontStyle.h>
#include <include/core/SkPaint.h>
#include <include/core/SkRect.h>
#include <include/ports/SkFontMgr_mac_ct.h>

#include <textflow/TextFlow.h>

#include <QByteArray>
#include <QColor>
#include <QHash>
#include <QPointF>
#include <QString>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <memory>

namespace {

constexpr float kPi = 3.14159265358979323846f;

SkColor toSkColor(const QColor &color) {
  return SkColorSetARGB(color.alpha(), color.red(), color.green(),
                        color.blue());
}

// Vertical offset from a target y to the baseline y that centers a glyph's
// [ascent, descent] span on that target — mirrors QCanvasPainter's
// TextBaseline::Middle.
float middleBaselineOffset(const SkFontMetrics &metrics) {
  return -(metrics.fAscent + metrics.fDescent) * 0.5f;
}

// Paints `text` tangent to a circle of `radius` centered at `center`, one
// glyph at a time — the same idea as TextPathPainter (see
// TextPathPainter.cpp) but specialised to a circle instead of an arbitrary
// QPainterPath: a circle has constant curvature, so the arc-length parameter
// t maps directly to angle (phi = 2*pi*t) with no need to flatten/sample the
// curve first.
//
//   - pathOffset [0, 1]: fraction of the (offset) circle's circumference used
//     as the text run's anchor, center-aligned around it (matching
//     TextPathPainter's default PathAlignment::Center).
//   - perpOffset: signed outward radial displacement — glyphs are placed on
//     a circle of radius (radius + perpOffset), not `radius` itself, so
//     spacing stays uniform on the curve the text actually sits on.
void drawTextAlongCircle(SkCanvas *canvas, const SkFont &font, SkColor color,
                         const QPointF &center, float radius, float pathOffset,
                         float perpOffset, const QString &text) {
  if (text.isEmpty())
    return;

  const float rArc = radius + perpOffset;
  const float arcLen = 2.0f * kPi * rArc;
  if (arcLen <= 0.0f)
    return;

  QHash<QChar, float> glyphCache;
  auto glyphAdvance = [&](QChar ch) -> float {
    auto it = glyphCache.constFind(ch);
    if (it != glyphCache.constEnd())
      return it.value();
    const QByteArray utf8 = QString(ch).toUtf8();
    const float advance =
        font.measureText(utf8.constData(), utf8.size(), SkTextEncoding::kUTF8);
    glyphCache.insert(ch, advance);
    return advance;
  };

  float totalWidth = 0.0f;
  for (QChar ch : text)
    totalWidth += glyphAdvance(ch);

  const float anchor = pathOffset * arcLen;
  float arcPos = anchor - totalWidth * 0.5f; // PathAlignment::Center

  SkFontMetrics metrics;
  font.getMetrics(&metrics);
  const float baselineShift = middleBaselineOffset(metrics);

  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setColor(color);

  const float cx = static_cast<float>(center.x());
  const float cy = static_cast<float>(center.y());

  for (QChar ch : text) {
    const float advance = glyphAdvance(ch);
    const float charCenter = arcPos + advance * 0.5f;
    arcPos += advance;

    if (charCenter < 0.0f || charCenter > arcLen)
      continue;

    const float t = charCenter / arcLen;
    const float phi = 2.0f * kPi * t;
    const float px = cx + rArc * std::cos(phi);
    const float py = cy + rArc * std::sin(phi);
    // Tangent direction at phi (travel direction as t increases) is
    // (-sin phi, cos phi); rotating the glyph's local +x axis to align with
    // it means rotating by (phi + 90deg).
    const float angleDeg = (phi + kPi * 0.5f) * 180.0f / kPi;

    canvas->save();
    canvas->translate(px, py);
    canvas->rotate(angleDeg);
    const QByteArray utf8 = QString(ch).toUtf8();
    canvas->drawString(utf8.constData(), -advance * 0.5f, baselineShift, font,
                       paint);
    canvas->restore();
  }
}

} // namespace

// SkiaSceneBackendImpl deliberately lives at global scope (not in an
// anonymous namespace) so that SpellCircleRenderer's
// `friend class SkiaSceneBackendImpl;` declaration names this exact type —
// a class in an anonymous namespace gets internal linkage under a
// compiler-generated unique name, and the friend declaration would instead
// silently forward-declare an unrelated, inaccessible ::SkiaSceneBackendImpl.
class SkiaSceneBackendImpl final : public CanvasSceneBackend {
public:
  explicit SkiaSceneBackendImpl(std::unique_ptr<SkiaGraphiteContext> context)
      : m_context(std::move(context)) {
    // Live text layer (TextFlow): on by default, SPELLCIRCLE_TEXTFLOW=0
    // disables it.
    const char *env = std::getenv("SPELLCIRCLE_TEXTFLOW");
    m_textFlowEnabled = !(env && env[0] == '0');
  }

  QCanvasImage drawScene(SpellCircleRenderer &renderer, QCanvasPainter *painter,
                         QCanvasOffscreenCanvas &canvas,
                         QSize pixelSize) override {
    // All coordinates are in 0..canvasWidth / 0..canvasHeight scene space,
    // same as QCanvasPainterSceneBackend — pixelSize always matches that
    // scene size (see SpellCircleRenderer::prePaint), so the resolved
    // geometry in renderer.m_circles/m_edges/m_boxes/m_pointLabels can be
    // drawn directly with no extra scaling here.
    SkiaOffscreenSurface surface(*m_context, canvas.texture(), pixelSize);
    SkCanvas *skCanvas = surface.canvas();
    if (!skCanvas)
      return {};

    skCanvas->clear(SK_ColorTRANSPARENT);

    const SkColor accentColor = toSkColor(renderer.m_accentColor);
    const float strokeWidth =
        static_cast<float>(renderer.m_strokeWidth * renderer.m_scale);
    const float boxWidth =
        static_cast<float>(renderer.m_boxWidth * renderer.m_scale);
    const float boxHeight =
        static_cast<float>(renderer.m_boxHeight * renderer.m_scale);
    const float boxPadding =
        static_cast<float>(renderer.m_boxPadding * renderer.m_scale);
    const float boxDistance =
        static_cast<float>(renderer.m_boxDistance * renderer.m_scale);
    const float pointDistance =
        static_cast<float>(renderer.m_pointDistance * renderer.m_scale);
    const float labelOffset =
        static_cast<float>(renderer.m_labelOffset * renderer.m_scale);

    const float fontSize =
        static_cast<float>(renderer.m_font.pointSizeF() * renderer.m_scale);
    const QByteArray familyUtf8 = renderer.m_font.family().toUtf8();
    // QFont::weight() is already on the same 1-900 (Thin..Black) scale as
    // SkFontStyle's weight, so it carries through directly — the settings
    // dialog's "Font Style" picker (e.g. "Light", "Semibold Italic") sets it
    // via QFontDatabase::font(), not just a bold/normal bool (see
    // FontDatabase::font() in FontDatabase.cpp).
    const int fontWeight = static_cast<int>(renderer.m_font.weight());
    SkFontStyle::Slant fontSlant = SkFontStyle::kUpright_Slant;
    if (renderer.m_font.style() == QFont::StyleItalic)
      fontSlant = SkFontStyle::kItalic_Slant;
    else if (renderer.m_font.style() == QFont::StyleOblique)
      fontSlant = SkFontStyle::kOblique_Slant;
    const SkFontStyle fontStyle(fontWeight, SkFontStyle::kNormal_Width,
                                fontSlant);
    // Recreating a CoreText SkFontMgr is expensive (it enumerates system
    // fonts), so it's cached for the render thread's lifetime rather than
    // rebuilt every drawScene() call.
    static const sk_sp<SkFontMgr> fontMgr = SkFontMgr_New_CoreText(nullptr);
    sk_sp<SkTypeface> typeface = fontMgr->matchFamilyStyle(
        familyUtf8.isEmpty() ? nullptr : familyUtf8.constData(), fontStyle);
    SkFont font(typeface, fontSize);
    font.setSubpixel(true);
    font.setEdging(SkFont::Edging::kAntiAlias);

    SkFontMetrics fontMetrics;
    font.getMetrics(&fontMetrics);

    // ── Edges (drawn underneath everything) ─────────────────────────────────
    SkPaint edgePaint;
    edgePaint.setAntiAlias(true);
    edgePaint.setStyle(SkPaint::kStroke_Style);
    edgePaint.setStrokeWidth(strokeWidth);
    edgePaint.setColor(accentColor);
    for (const auto &edge : renderer.m_edges) {
      skCanvas->drawLine(static_cast<float>(edge.first.x()),
                         static_cast<float>(edge.first.y()),
                         static_cast<float>(edge.second.x()),
                         static_cast<float>(edge.second.y()), edgePaint);
    }

    // ── Circles ──────────────────────────────────────────────────────────────
    for (const auto &circle : renderer.m_circles) {
      // A radius-0 circle is an invisible anchor (see isAnchorOnlyCircle in
      // SpellCircleRenderer.cpp) used only to position points/boxes/edges —
      // nothing to draw for it.
      if (circle.radius <= 0.0f)
        continue;

      const float r = circle.radius;
      const float cx = static_cast<float>(circle.center.x());
      const float cy = static_cast<float>(circle.center.y());

      SkPaint strokePaint;
      strokePaint.setAntiAlias(true);
      strokePaint.setStyle(SkPaint::kStroke_Style);
      strokePaint.setStrokeWidth(strokeWidth);
      strokePaint.setColor(accentColor);
      skCanvas->drawCircle(cx, cy, r, strokePaint);

      if (circle.active > 0.0f) {
        SkPaint fillPaint;
        fillPaint.setAntiAlias(true);
        fillPaint.setStyle(SkPaint::kFill_Style);
        fillPaint.setColor(accentColor);
        fillPaint.setAlphaf(circle.active);
        skCanvas->drawCircle(cx, cy, r, fillPaint);
      }

      if (!circle.name.isEmpty()) {
        drawTextAlongCircle(skCanvas, font, accentColor, circle.center, r,
                            circle.textStart, labelOffset, circle.name);
      }
    }

    // ── Point value labels ───────────────────────────────────────────────────
    // Positioned the same way as a box (anchor pushed outward by boxDistance
    // along the ray from the canvas center), but a point isn't a box: just
    // the text, no rect/stroke/fill around it.
    for (const auto &label : renderer.m_pointLabels) {
      const QPointF center = label.anchor + label.direction * pointDistance;
      const QByteArray utf8 = label.value.toUtf8();
      const float textW = font.measureText(utf8.constData(), utf8.size(),
                                           SkTextEncoding::kUTF8);

      SkPaint paint;
      paint.setAntiAlias(true);
      paint.setColor(accentColor);
      skCanvas->drawString(
          utf8.constData(), static_cast<float>(center.x()) - textW * 0.5f,
          static_cast<float>(center.y()) + middleBaselineOffset(fontMetrics),
          font, paint);
    }

    // ── Boxes ────────────────────────────────────────────────────────────────
    // The resolved box rects double as exclusion shapes for the TextFlow
    // layer below, so the flowing paragraph makes room for them too.
    std::vector<SkRect> boxRects;
    auto drawBox = [&](const SpellCircleRenderer::ResolvedBox &box) {
      const QByteArray utf8 = box.value.toUtf8();
      const float textW = font.measureText(utf8.constData(), utf8.size(),
                                           SkTextEncoding::kUTF8);
      const float bw = std::max(textW + boxPadding * 2.0f, boxWidth);
      const float bh = boxHeight;

      // The box's center sits on the ray from the canvas center through the
      // point, pushed outward by boxDistance beyond the point — nothing more
      // (see the equivalent comment in QCanvasPainterSceneBackend::drawScene
      // for why edge-dependent anchoring was rejected).
      const QPointF boxCenter = box.anchor + box.direction * boxDistance;
      const float hw = bw / 2.0f;
      const float hh = bh / 2.0f;
      const float bx = static_cast<float>(boxCenter.x()) - hw;
      const float by = static_cast<float>(boxCenter.y()) - hh;

      const SkRect rect = SkRect::MakeXYWH(bx, by, bw, bh);
      boxRects.push_back(rect);

      // Punch a transparent hole first so whatever was drawn underneath
      // (edges, circles) doesn't show through the box — equivalent to
      // QCanvasPainter::clearRect().
      SkPaint clearPaint;
      clearPaint.setBlendMode(SkBlendMode::kClear);
      skCanvas->drawRect(rect, clearPaint);

      if (box.active > 0.0f) {
        SkPaint fillPaint;
        fillPaint.setAntiAlias(true);
        fillPaint.setStyle(SkPaint::kFill_Style);
        fillPaint.setColor(accentColor);
        fillPaint.setAlphaf(box.active);
        skCanvas->drawRect(rect, fillPaint);
      }

      SkPaint strokePaint;
      strokePaint.setAntiAlias(true);
      strokePaint.setStyle(SkPaint::kStroke_Style);
      strokePaint.setStrokeWidth(strokeWidth);
      strokePaint.setColor(accentColor);
      skCanvas->drawRect(rect, strokePaint);

      // TextBaseline::Top: the top of the glyphs (not the alphabetic
      // baseline) sits at (bx + boxPadding, by + boxPadding).
      const float baselineY = by + boxPadding - fontMetrics.fAscent;

      SkPaint textPaint;
      textPaint.setAntiAlias(true);
      if (box.active > 0.0f) {
        // Punches the text out of the box fill instead of drawing it on top
        // — matches QCanvasPainter::CompositeOperation::DestinationOut.
        textPaint.setBlendMode(SkBlendMode::kDstOut);
        textPaint.setColor(SK_ColorBLACK);
      } else {
        textPaint.setColor(accentColor);
      }
      skCanvas->drawString(utf8.constData(), bx + boxPadding, baselineY, font,
                           textPaint);
    };
    for (const auto &box : renderer.m_boxes)
      drawBox(box);

    // ── Live TextFlow layer ──────────────────────────────────────────────
    // A rich, mixed-font, mixed-script paragraph that reflows around the
    // spell circles every frame. Words restyle and rewrite themselves
    // periodically to exercise the incremental (shape-cache) path; the
    // reflow itself is pure placement arithmetic.
    if (m_textFlowEnabled)
      drawTextFlowLayer(skCanvas, renderer, pixelSize, accentColor, boxRects);

    surface.submit();

    // The offscreen canvas's blended output is premultiplied-alpha; without
    // this flag drawImage() re-applies alpha on top of already alpha-baked-in
    // color, darkening every translucent fill.
    return painter->addImage(canvas,
                             QCanvasPainter::ImageFlag::GenerateMipmaps |
                                 QCanvasPainter::ImageFlag::Premultiplied);
  }

private:
  void buildParagraph(float em, SkColor ink) {
    using namespace textflow;
    SkFontMgr *mgr = m_textCtx->fontMgr();

    auto make = [&](const char *family, float size, SkColor color,
                    const char *lang = "") {
      TextStyle style;
      style.shaping.typeface =
          family ? mgr->matchFamilyStyle(family, SkFontStyle()) : nullptr;
      style.shaping.size = size;
      style.shaping.language = lang;
      style.paint.color = color;
      return style;
    };

    const SkColor dim = SkColorSetA(ink, 0xB4);
    const TextStyle serif = make("Georgia", em, ink);
    const TextStyle sans = make("Avenir Next", em * 0.92f, dim);
    const TextStyle mono = make("Menlo", em * 0.78f, dim);
    TextStyle emphatic = make("Georgia", em * 1.18f, ink);
    emphatic.paint.shadows.push_back(
        {SkColorSetA(SK_ColorBLACK, 0x60), {em * 0.06f, em * 0.08f},
         em * 0.06f});

    m_paragraph.clear();
    m_paragraph.appendText("The circle remembers every sigil cast into it. ",
                           serif);
    m_paragraph.appendText("Glyphs flow around the living geometry, ",
                           emphatic);
    m_paragraph.appendText(
        "reflowing each frame as the rings drift and breathe. ", serif);
    m_paragraph.appendText("術式は円環の周りを流れ、", make(nullptr, em, ink, "ja"));
    m_paragraph.appendText("빛의 문자가 따라 흐른다. ",
                           make(nullptr, em, dim, "ko"));
    m_paragraph.appendText("每個符文都繞行於法陣之間。 ",
                           make(nullptr, em, ink, "zh"));
    m_paragraph.appendText("No word is ever reshaped for the motion — ", sans);
    m_paragraph.appendText("shape once, reposition forever. ", mono);
    m_paragraph.appendText(
        "The paragraph itself mutates while it moves: colors flare, words "
        "swap, and the cache absorbs it all. ",
        serif);
    // Enough body to fill the canvas so lines visibly split around the
    // circles wherever they wander.
    m_paragraph.appendText(
        "Where a ring crosses a line, the line parts around it and each "
        "fragment justifies itself against its own edges; where the ring "
        "leaves, the fragments close ranks again. ",
        sans);
    m_paragraph.appendText("記憶された文様は決して崩れず、", make(nullptr, em, dim, "ja"));
    m_paragraph.appendText("원이 지나가도 문장은 이어진다. ",
                           make(nullptr, em, ink, "ko"));
    m_paragraph.appendText(
        "Every glyph you see was shaped exactly once; every frame since has "
        "been nothing but arithmetic over cached advances — the same trick "
        "Chromium plays, minus the browser. ",
        serif);
    m_paragraph.appendText(
        "The sidebar feeds the model, the model feeds the renderer, and the "
        "renderer hands this paragraph a fresh set of obstacles sixty times "
        "a second. ",
        sans);
    m_paragraph.appendText("それでもレイアウトは一ミリ秒の遥か手前で終わる。 ",
                           make(nullptr, em, ink, "ja"));
    m_paragraph.appendText("mixed scripts, mixed fonts, one shape cache. ",
                           mono);
    m_paragraph.appendText(
        "Circles drift, boxes anchor, edges hum — and the text simply makes "
        "room, line by line, exactly where the geometry asks it to.",
        serif);
  }

  void drawTextFlowLayer(SkCanvas *canvas, SpellCircleRenderer &renderer,
                         QSize pixelSize, SkColor ink,
                         const std::vector<SkRect> &boxRects) {
    using namespace textflow;
    using Clock = std::chrono::steady_clock;

    // Created on first use so everything lives on the render thread.
    if (!m_textCtx)
      m_textCtx = std::make_unique<FontContext>(SkFontMgr_New_CoreText(nullptr));

    const float em = pixelSize.width() / 46.0f;
    if (!m_paragraphBuilt || m_paragraphEm != em || m_paragraphInk != ink) {
      buildParagraph(em, ink);
      m_paragraphBuilt = true;
      m_paragraphEm = em;
      m_paragraphInk = ink;
    }

    // Periodic rich-text churn (~every 2s / 4s at 60 fps).
    if (m_frame > 0 && m_frame % 120 == 0) {
      static const SkColor flares[] = {0xFFE4572E, 0xFF4C86C6, 0xFFF3A712};
      PaintStyle flare;
      flare.color = flares[(m_frame / 120) % 3];
      const std::u16string &text = m_paragraph.text();
      const size_t at = text.find(u"Glyphs");
      if (at != std::u16string::npos)
        m_paragraph.setPaint(static_cast<uint32_t>(at),
                             static_cast<uint32_t>(at + 11),
                             flare); // "Glyphs flow"
    }
    if (m_frame > 0 && m_frame % 240 == 0) {
      static const char *verbs[] = {"drift", "spin ", "pulse", "churn"};
      const size_t at = m_paragraph.text().find(u"drift");
      size_t found = at;
      if (found == std::u16string::npos)
        for (const char *v : {"spin ", "pulse", "churn"})
          if ((found = m_paragraph.text().find(
                   std::u16string(v, v + 5))) != std::u16string::npos)
            break;
      if (found != std::u16string::npos)
        m_paragraph.replaceText(static_cast<uint32_t>(found),
                                static_cast<uint32_t>(found + 5),
                                verbs[(m_frame / 240) % 4]);
    }

    // The live scene *is* the flow geometry: every resolved circle becomes
    // an exclusion shape.
    const float margin = pixelSize.width() * 0.04f;
    ExclusionFlow flow(SkRect::MakeXYWH(
        margin, margin, pixelSize.width() - 2 * margin,
        pixelSize.height() - 2 * margin));
    const float pad = em * 0.6f;
    for (const auto &circle : renderer.m_circles) {
      if (circle.radius <= 0.0f)
        continue;
      const float r = circle.radius + pad;
      flow.shapes().push_back(
          {ExclusionFlow::Shape::kCircle,
           SkRect::MakeXYWH(static_cast<float>(circle.center.x()) - r,
                            static_cast<float>(circle.center.y()) - r, 2 * r,
                            2 * r),
           0});
    }
    for (const SkRect &rect : boxRects)
      flow.shapes().push_back({ExclusionFlow::Shape::kRect, rect, pad});
    flow.setMinIntervalWidth(em * 3);

    LayoutOptions opts;
    opts.alignment = Alignment::kJustify;
    opts.lineHeight = em * 1.75f;

    const auto t0 = Clock::now();
    Layout layout = layoutParagraph(*m_textCtx, m_paragraph, flow, opts);
    const auto t1 = Clock::now();
    layout.draw(canvas, m_paragraph);

    const double us =
        std::chrono::duration<double, std::micro>(t1 - t0).count();
    m_layoutUsEma = m_layoutUsEma == 0 ? us : m_layoutUsEma * 0.98 + us * 0.02;
    if (m_frame % 600 == 0)
      spdlog::info("TextFlow layer: layout {:.1f} us (ema {:.1f} us), {} runs, "
                   "{} circles as exclusions",
                   us, m_layoutUsEma, layout.runs.size(),
                   flow.shapes().size());
    m_frame++;
  }

  std::unique_ptr<SkiaGraphiteContext> m_context;

  bool m_textFlowEnabled = true;
  std::unique_ptr<textflow::FontContext> m_textCtx; // render-thread only
  textflow::Paragraph m_paragraph;
  bool m_paragraphBuilt = false;
  float m_paragraphEm = 0;
  SkColor m_paragraphInk = 0;
  uint64_t m_frame = 0;
  double m_layoutUsEma = 0;
};

std::unique_ptr<CanvasSceneBackend> createSkiaSceneBackend(QRhi *rhi) {
  std::unique_ptr<SkiaGraphiteContext> context =
      SkiaGraphiteContext::create(rhi);
  if (!context)
    return nullptr;
  return std::make_unique<SkiaSceneBackendImpl>(std::move(context));
}
