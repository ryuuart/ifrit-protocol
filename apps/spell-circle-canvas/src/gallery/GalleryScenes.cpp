#include "GalleryScenes.h"

#include <textflow/Query.h>
#include <textflowqt/TextFlowQt.h>

#include <include/core/SkContourMeasure.h>
#include <include/core/SkFontMgr.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPath.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkRSXform.h>

#include <QStringLiteral>

#include <array>
#include <chrono>
#include <cmath>
#include <random>
#include <string>

using namespace textflow;

namespace gallery {

namespace {

using Clock = std::chrono::steady_clock;

constexpr SkColor kInk = 0xFF23252B;
constexpr SkColor kAccent = 0xFFC63D2F;
constexpr SkColor kBlue = 0xFF2B5AA7;
constexpr SkColor kShape = 0x33808A99;
constexpr SkColor kPaper = 0xFFFAF7F0;

double toUs(Clock::duration d) {
  return std::chrono::duration<double, std::micro>(d).count();
}

TextStyle makeStyle(float size, SkColor color, const char *lang = "",
                    sk_sp<SkTypeface> typeface = nullptr) {
  TextStyle s;
  s.shaping.typeface = std::move(typeface);
  s.shaping.size = size;
  s.shaping.language = lang;
  s.paint.color = color;
  return s;
}

// Body paragraph rebuilt only when the live controls actually change.
struct BodyCache {
  Paragraph para;
  QString builtText;
  const SkTypeface *builtFace = nullptr;
  float builtSize = 0;

  // Returns true when the paragraph was rebuilt this call.
  bool ensure(const SceneParams &params, const QString &fallbackText,
              const sk_sp<SkTypeface> &fallbackFace) {
    const QString &text =
        params.text.isEmpty() ? fallbackText : params.text;
    const sk_sp<SkTypeface> &face =
        params.typeface ? params.typeface : fallbackFace;
    if (text == builtText && face.get() == builtFace &&
        params.fontSize == builtSize)
      return false;
    builtText = text;
    builtFace = face.get();
    builtSize = params.fontSize;
    para.clear();
    // Zero-copy: QString and Paragraph both store UTF-16.
    textflowqt::appendText(para, text,
                           makeStyle(params.fontSize, kInk, "", face));
    return true;
  }
};

sk_sp<SkTypeface> defaultSerif(FontContext &ctx) {
  sk_sp<SkTypeface> face =
      ctx.fontMgr()->matchFamilyStyle("Noto Serif", SkFontStyle());
  return face ? face : ctx.defaultTypeface();
}

void drawCaption(SkCanvas *canvas, FontContext &ctx, const char *text,
                 SkPoint at, float width = 520) {
  Paragraph para;
  para.appendText(text, makeStyle(12.0f, kBlue));
  BlockFlow flow(SkRect::MakeXYWH(at.x(), at.y(), width, 32));
  layoutParagraph(ctx, para, flow).draw(canvas, para);
}

// Mixed Latin/CJK filler in alternating color chunks.
Paragraph makeBigParagraph(int wordCount, float size) {
  const char *latin[] = {"the",     "letters", "fall",   "away",    "from",
                         "their",   "lines",   "and",    "return",  "again",
                         "layout",  "engine",  "words",  "measure", "glyph",
                         "cascade", "gentle",  "steady", "rhythm",  "flowing"};
  const char *cjk[] = {"文字", "雨", "波紋", "字形", "빗물", "글자",
                       "물결", "여울", "漣漪", "文雨", "字落", "縦横"};
  const TextStyle styles[3] = {makeStyle(size, kInk), makeStyle(size, kBlue),
                               makeStyle(size, kAccent)};
  std::mt19937 rng(23);
  Paragraph para;
  std::string chunk;
  int chunkStyle = 0;
  for (int i = 0; i < wordCount; ++i) {
    if (i % 5 == 4)
      chunk += cjk[rng() % 12];
    else
      chunk += latin[rng() % 20];
    chunk += ' ';
    if (i % 120 == 119 || i + 1 == wordCount) {
      para.appendText(chunk, styles[chunkStyle++ % 3]);
      chunk.clear();
    }
  }
  return para;
}

// A spiky ring whose points breathe in and out independently, around an
// even-odd hole that stays open to text. Rebuilt every frame: each frame's
// path carries a fresh generation ID, so unlike the drifting donut (cached
// flattening + pathOffset) this shape exercises live re-flattening and the
// layout adapts to the changing silhouette as it morphs.
SkPath spikyRingPath(float t, float r) {
  SkPathBuilder pb;
  constexpr int kSpikes = 9;
  constexpr float kTwoPi = 6.2831853f;
  for (int i = 0; i < kSpikes; ++i) {
    const float base = static_cast<float>(i) * kTwoPi / kSpikes;
    const float tip =
        r * (0.76f + 0.24f * std::sin(t * 1.7f + static_cast<float>(i) * 2.3f));
    const SkPoint pTip = {tip * std::cos(base), tip * std::sin(base)};
    const float aValley = base + kTwoPi / kSpikes * 0.5f;
    const SkPoint pValley = {r * 0.5f * std::cos(aValley),
                             r * 0.5f * std::sin(aValley)};
    if (i == 0)
      pb.moveTo(pTip);
    else
      pb.lineTo(pTip);
    pb.lineTo(pValley);
  }
  pb.close();
  pb.addCircle(0, 0, r * 0.32f);
  pb.setFillType(SkPathFillType::kEvenOdd);
  return pb.detach();
}

// ── Scene 1: exclusions & SkPath shapes ───────────────────────────────────

class ExclusionsScene final : public Scene {
public:
  QString name() const override {
    return QStringLiteral("Exclusions & shapes");
  }
  QString defaultText() const override {
    return QStringLiteral(
        "Typography is the craft of arranging type, and glyphs flow around "
        "obstacles the way water flows around stones. 日本語のテキストも同じ"
        "流れに乗って進み、한국어 단어들도 자연스럽게 흐르고, 中文字符同样围"
        "绕形状排布。Latin and CJK mix freely because every word is shaped "
        "once, cached, and repositioned with pure arithmetic. The orbiting "
        "circle is a classic exclusion; the pulsing spiky ring and the donut "
        "are arbitrary SkPaths — the ring is a brand-new path every frame as "
        "its points breathe in and out, and both holes stay open to text. "
        "When a shape slides across a line the line splits into fragments "
        "and each fragment justifies itself independently; when it moves on, "
        "the fragments knit back together as if nothing happened. No word is "
        "ever reshaped for any of this, frame after frame after frame. "
        "Everything you read here can be rewritten live from the panel on "
        "the left: retype the body, swap the family, drag the size — the "
        "shape cache absorbs each keystroke and the flow simply re-places "
        "the words. 形が動くたびに行は裂け、また元通りに繋がる。글자는 한 번"
        "만 성형되고 계속 재사용됩니다. The donut's hole is part of the same "
        "even-odd path, so with enough text the lines pour straight through "
        "its centre while avoiding the ring around it.");
  }

  FrameStats render(SkCanvas *canvas, SkISize size, double t, int /*frame*/,
                    const SceneParams &params, FontContext &ctx) override {
    if (!m_serif)
      m_serif = defaultSerif(ctx);
    m_body.ensure(params, defaultText(), m_serif);

    const float w = size.width(), h = size.height();
    const float em = params.fontSize;
    ExclusionFlow flow(SkRect::MakeXYWH(28, 24, w - 56, h - 48));

    const float cr = std::min(w, h) * 0.13f;
    const SkPoint c = {w * 0.32f + w * 0.2f * std::sin(static_cast<float>(t) * 0.9f),
                       h * 0.38f + h * 0.24f * std::sin(static_cast<float>(t) * 0.53f)};
    flow.shapes().push_back(ExclusionFlow::Shape::Circle(
        SkRect::MakeXYWH(c.x() - cr, c.y() - cr, 2 * cr, 2 * cr), em * 0.5f));

    if (m_pathSize != size) {
      m_pathSize = size;
      SkPathBuilder donut;
      const float dr = std::min(w, h) * 0.16f;
      donut.addCircle(w * 0.72f, h * 0.68f, dr);
      donut.addCircle(w * 0.72f, h * 0.68f, dr * 0.5f);
      donut.setFillType(SkPathFillType::kEvenOdd);
      m_donut = donut.detach();
    }
    // Morphing exclusion: a brand-new spiky path every frame, points
    // pulsing in and out — the flow re-flattens and relayouts live.
    const SkPath spiky =
        spikyRingPath(static_cast<float>(t), std::min(w, h) * 0.19f);
    ExclusionFlow::Shape star = ExclusionFlow::Shape::Path(spiky, em * 0.4f);
    star.pathOffset = {w * 0.6f + w * 0.18f * std::cos(static_cast<float>(t) * 0.4f),
                       h * 0.3f + h * 0.1f * std::sin(static_cast<float>(t) * 0.7f)};
    flow.shapes().push_back(star);
    // The donut drifts too: moving a path via pathOffset reuses its cached
    // flattening, and every frame is a full live relayout around it.
    ExclusionFlow::Shape donut =
        ExclusionFlow::Shape::Path(m_donut, em * 0.4f);
    donut.pathOffset = {w * 0.05f * std::sin(static_cast<float>(t) * 0.6f),
                        h * 0.06f * std::cos(static_cast<float>(t) * 0.45f)};
    flow.shapes().push_back(donut);
    flow.setMinIntervalWidth(em * 3);

    LayoutOptions opts;
    opts.alignment = params.alignment;
    opts.breaker = params.breaker;
    opts.lineHeight = em * 1.7f;
    opts.kpMinInterval = em * 3;
    opts.ellipsis = u"…"; // paste a novel: the tail is marked, not shaped

    const auto t0 = Clock::now();
    Layout layout = layoutParagraph(ctx, m_body.para, flow, opts);
    const auto t1 = Clock::now();

    canvas->clear(kPaper);
    SkPaint ghost;
    ghost.setAntiAlias(true);
    ghost.setColor(kShape);
    canvas->drawCircle(c.x(), c.y(), cr, ghost);
    canvas->save();
    canvas->translate(star.pathOffset.x(), star.pathOffset.y());
    canvas->drawPath(spiky, ghost);
    canvas->restore();
    canvas->save();
    canvas->translate(donut.pathOffset.x(), donut.pathOffset.y());
    canvas->drawPath(m_donut, ghost);
    canvas->restore();
    layout.drawBatched(canvas, m_body.para);

    return {toUs(t1 - t0), static_cast<int>(layout.runs.size()), 0};
  }

private:
  BodyCache m_body;
  sk_sp<SkTypeface> m_serif;
  SkPath m_donut;
  SkISize m_pathSize = {0, 0};
};

// ── Scene 2: Knuth-Plass vs greedy, hyphenation, last-line modes ──────────

class KnuthPlassScene final : public Scene {
public:
  QString name() const override {
    return QStringLiteral("Knuth–Plass & hyphens");
  }
  QString defaultText() const override {
    // Soft hyphens (U+00AD) give both breakers discretionary break points.
    return QString::fromUtf8(
        "The para­graph breaker con­sid­ers every way to "
        "break this text into lines and picks the one with the least "
        "bad­ness, ex­act­ly like TeX. Greedy breaking "
        "com­mits line by line and leaves rag­ged, "
        "in­con­sis­tent spac­ing be­hind; "
        "op­ti­mal breaking spreads the slack across the whole "
        "para­graph in­stead, tak­ing dis­cre­tion"
        "­ary hy­phens when they pay for them­selves. The "
        "measure breathes to pose a fresh prob­lem every frame.");
  }

  FrameStats render(SkCanvas *canvas, SkISize size, double t, int /*frame*/,
                    const SceneParams &params, FontContext &ctx) override {
    if (!m_serif)
      m_serif = defaultSerif(ctx);
    m_body.ensure(params, defaultText(), m_serif);

    const float w = size.width(), h = size.height();
    const float em = params.fontSize;
    const float measure =
        w * 0.36f * (1.0f + 0.10f * std::sin(static_cast<float>(t) * 0.5f));

    LayoutOptions opts;
    opts.alignment = params.alignment;
    opts.lineHeight = em * 1.6f;
    opts.hyphenate = true;
    opts.kpMinInterval = em * 3;
    opts.ellipsis = u"…";

    canvas->clear(kPaper);
    drawCaption(canvas, ctx, "greedy", {w * 0.07f, 18});
    drawCaption(canvas, ctx, "knuth-plass (optimal, TeX badness)",
                {w * 0.55f, 18});

    double layoutUs = 0;
    int runs = 0;
    for (int pass = 0; pass < 2; ++pass) {
      opts.breaker = pass == 0 ? Breaker::kGreedy : Breaker::kKnuthPlass;
      const float x = pass == 0 ? w * 0.07f : w * 0.55f;
      BlockFlow flow(SkRect::MakeXYWH(x, 48, measure, h - 80));
      const auto t0 = Clock::now();
      Layout layout = layoutParagraph(ctx, m_body.para, flow, opts);
      layoutUs += toUs(Clock::now() - t0);
      runs += static_cast<int>(layout.runs.size());

      SkPaint rule;
      rule.setAntiAlias(true);
      rule.setStyle(SkPaint::kStroke_Style);
      rule.setStrokeWidth(1);
      rule.setColor(0x3323252B);
      canvas->drawLine(x + measure, 44, x + measure, h - 44, rule);
      layout.drawBatched(canvas, m_body.para);
    }
    return {layoutUs, runs, 0};
  }

private:
  BodyCache m_body;
  sk_sp<SkTypeface> m_serif;
};

// ── Scene 3: infinite loop marquee on a closed figure-eight ───────────────

class LoopScene final : public Scene {
public:
  QString name() const override { return QStringLiteral("Infinite loop"); }
  QString defaultText() const override {
    return QStringLiteral("and the words go round 終わらない文字の環 끝나지 "
                          "않는 글의 고리 文字環繞不息 round and round again "
                          "— ")
        .repeated(4);
  }

  FrameStats render(SkCanvas *canvas, SkISize size, double t, int /*frame*/,
                    const SceneParams &params, FontContext &ctx) override {
    if (!m_serif)
      m_serif = defaultSerif(ctx);
    m_body.ensure(params, defaultText(), m_serif);

    const float w = size.width(), h = size.height();
    if (m_size != size) {
      m_size = size;
      SkPathBuilder pb;
      const SkPoint c = {w * 0.5f, h * 0.5f};
      const int steps = 400;
      for (int i = 0; i <= steps; ++i) {
        const float a = static_cast<float>(i) / steps * 6.2831853f;
        const SkPoint p = {c.x() + w * 0.4f * std::sin(a),
                           c.y() + h * 0.78f * std::sin(a) * std::cos(a)};
        if (i == 0)
          pb.moveTo(p);
        else
          pb.lineTo(p);
      }
      pb.close();
      m_eight = pb.detach();
      SkContourMeasureIter iter(m_eight, false);
      m_contour = iter.next();
    }
    if (!m_contour)
      return {};
    const float loopLen = m_contour->length();

    LineSetFlow flow;
    LineInterval interval;
    interval.contour = m_contour;
    interval.length = loopLen;
    interval.contourStart =
        std::fmod(static_cast<float>(t) * 110.0f, loopLen);
    flow.lines().push_back({interval});

    const auto t0 = Clock::now();
    Layout layout = layoutParagraph(ctx, m_body.para, flow);
    const auto t1 = Clock::now();

    canvas->clear(kPaper);
    SkPaint rail;
    rail.setAntiAlias(true);
    rail.setStyle(SkPaint::kStroke_Style);
    rail.setStrokeWidth(1.2f);
    rail.setColor(0x2A23252B);
    canvas->drawPath(m_eight, rail);
    layout.draw(canvas, m_body.para);
    return {toUs(t1 - t0), static_cast<int>(layout.runs.size()), 0};
  }

private:
  BodyCache m_body;
  sk_sp<SkTypeface> m_serif;
  SkPath m_eight;
  sk_sp<SkContourMeasure> m_contour;
  SkISize m_size = {0, 0};
};

// ── Scene 4: letter rain on an umbrella (full-paragraph relayout) ─────────

class RainScene final : public Scene {
public:
  QString name() const override {
    return QStringLiteral("Letter rain — 700 words");
  }
  bool supportsTextEdit() const override { return false; }

  FrameStats render(SkCanvas *canvas, SkISize size, double /*t*/, int frame,
                    const SceneParams & /*params*/, FontContext &ctx) override {
    if (m_para.text().empty())
      m_para = makeBigParagraph(700, 13.0f);

    const float w = size.width(), h = size.height();
    const SkPoint domeC = {w * 0.5f, h * 0.80f};
    const float domeR = std::min(w, h) * 0.20f;
    const float standoff = domeR + 8;

    // The measure breathes: a different line-breaking problem every frame.
    const float width =
        (w - 90) * (1.0f + 0.05f * std::sin(static_cast<float>(frame) * 0.02f));
    BlockFlow flow(SkRect::MakeXYWH(40, 16, width, h * 0.52f));
    LayoutOptions opts;
    opts.alignment = Alignment::kJustify;
    opts.lineHeight = 21;

    const auto t0 = Clock::now();
    Layout layout = layoutParagraph(ctx, m_para, flow, opts);
    const auto t1 = Clock::now();

    size_t glyphCount = 0;
    forEachPlacedGlyph(layout, m_para, [&](auto...) { glyphCount++; });
    // The breathing measure can overflow a varying tail of words, so the
    // placed-glyph count drifts a little frame to frame — resize keeps the
    // surviving particles' state instead of resetting the whole sky.
    if (m_parts.size() != glyphCount)
      m_parts.resize(glyphCount);
    if (m_parts.empty())
      return {toUs(t1 - t0), static_cast<int>(layout.runs.size()), 0};

    if (frame > 30)
      for (int d = 0; d < 18; ++d) {
        Particle &p = m_parts[m_rng() % m_parts.size()];
        if (p.mode == Particle::kAttached)
          p.mode = Particle::kFalling;
      }

    m_batches.clear();
    size_t idx = 0;
    forEachPlacedGlyph(layout, m_para, [&](const ShapedWord *sw,
                                           SkGlyphID glyph, float adv,
                                           SkColor color, SkPoint rest) {
      Particle &p = m_parts[idx % m_parts.size()];
      idx++;
      const float half = adv * 0.5f;
      const SkPoint restCenter = rest + SkVector{half, 0};
      float c = 1, sn = 0;
      SkPoint at = restCenter;
      switch (p.mode) {
      case Particle::kAttached:
        break;
      case Particle::kFalling: {
        if (p.vy == 0 && p.pos.fY == 0) {
          p.pos = restCenter;
          p.vy = 2.5f + static_cast<float>(m_rng() % 100) * 0.025f;
          p.vx = 0;
          p.spin = (static_cast<float>(m_rng() % 100) - 50.0f) * 0.0015f;
          p.angle = 0;
        }
        p.pos.fY += p.vy;
        p.pos.fX += p.vx;
        p.angle += p.spin;
        SkVector dir = p.pos - domeC;
        if (dir.length() < standoff && p.pos.fY < domeC.fY)
          p.mode = Particle::kSliding;
        if (p.pos.fY > size.height() + 30)
          p = Particle{}; // drained: rejoin the paragraph
        break;
      }
      case Particle::kSliding: {
        SkVector dir = p.pos - domeC;
        dir.normalize();
        p.pos = domeC + dir * standoff;
        SkVector tangent = {-dir.fY, dir.fX};
        if (tangent.fX * dir.fX < 0)
          tangent = -tangent;
        const float slide = p.vy * (0.55f + 0.9f * std::abs(dir.fX));
        p.pos += tangent * slide;
        p.angle = std::atan2(tangent.fY, tangent.fX);
        if (dir.fY > -0.18f) {
          p.mode = Particle::kFalling;
          p.vx = tangent.fX * slide;
        }
        break;
      }
      }
      if (p.mode != Particle::kAttached) {
        at = p.pos;
        quantAngle(p.angle, c, sn);
      }
      GlyphRSXformBatches::Batch &b = m_batches.batchFor(sw, color);
      b.glyphs.push_back(glyph);
      b.xforms.push_back({c, sn, at.fX - c * half, at.fY - sn * half});
    });

    canvas->clear(kPaper);
    SkPaint umbrella;
    umbrella.setAntiAlias(true);
    umbrella.setColor(0xFFB9552F);
    SkPathBuilder dome;
    dome.addArc(SkRect::MakeLTRB(domeC.fX - domeR, domeC.fY - domeR,
                                 domeC.fX + domeR, domeC.fY + domeR),
                180, 180);
    dome.close();
    canvas->drawPath(dome.detach(), umbrella);
    SkPaint pole;
    pole.setAntiAlias(true);
    pole.setStyle(SkPaint::kStroke_Style);
    pole.setStrokeWidth(6);
    pole.setColor(kInk);
    canvas->drawLine(domeC.fX, domeC.fY - 6, domeC.fX, h - 10, pole);
    const int glyphs = m_batches.draw(canvas);
    return {toUs(t1 - t0), static_cast<int>(layout.runs.size()), glyphs};
  }

private:
  struct Particle {
    enum Mode : uint8_t { kAttached, kFalling, kSliding } mode = kAttached;
    SkPoint pos = {0, 0};
    float vx = 0, vy = 0, angle = 0, spin = 0;
  };
  Paragraph m_para;
  std::vector<Particle> m_parts;
  GlyphRSXformBatches m_batches;
  std::mt19937 m_rng{9};
};

// ── Scene 5: ripple pool (click to drop) ──────────────────────────────────

class RippleScene final : public Scene {
public:
  QString name() const override {
    return QStringLiteral("Ripple pool — click me");
  }
  bool supportsTextEdit() const override { return false; }

  void pointerPress(SkPoint at) override { m_pending.push_back(at); }

  FrameStats render(SkCanvas *canvas, SkISize size, double /*t*/, int frame,
                    const SceneParams & /*params*/, FontContext &ctx) override {
    if (m_para.text().empty())
      m_para = makeBigParagraph(700, 13.0f);

    const float w = size.width(), h = size.height();
    if (frame % 90 == 0)
      m_drops.push_back({{60.0f + static_cast<float>(m_rng() % std::max(1, static_cast<int>(w) - 120)),
                          60.0f + static_cast<float>(m_rng() % std::max(1, static_cast<int>(h) - 120))},
                         frame});
    for (const SkPoint &p : m_pending)
      m_drops.push_back({p, frame});
    m_pending.clear();
    std::erase_if(m_drops,
                  [&](const Drop &d) { return frame - d.born > 280; });

    // Live edit mid-ripple: same-length swap, everything else cache-hot.
    static const char *swaps[] = {"letters", "glyphs ", "symbols", "strokes"};
    if (frame > 0 && frame % 150 == 0) {
      const size_t at = m_para.text().find(u"letters");
      if (at != std::u16string::npos)
        m_para.replaceText(static_cast<uint32_t>(at),
                           static_cast<uint32_t>(at + 7),
                           swaps[(frame / 150) % 4]);
    }

    BlockFlow flow(SkRect::MakeXYWH(36, 24, w - 72, h - 48));
    LayoutOptions opts;
    opts.alignment = Alignment::kJustify;
    opts.lineHeight = 21;

    const auto t0 = Clock::now();
    Layout layout = layoutParagraph(ctx, m_para, flow, opts);
    const auto t1 = Clock::now();

    m_batches.clear();
    forEachPlacedGlyph(layout, m_para, [&](const ShapedWord *sw,
                                           SkGlyphID glyph, float adv,
                                           SkColor color, SkPoint rest) {
      SkVector offset = {0, 0};
      float tilt = 0;
      for (const Drop &drop : m_drops) {
        SkVector rv = rest - drop.center;
        const float r = rv.length() + 1.0f;
        const float ringR = 6.0f * static_cast<float>(frame - drop.born);
        const float dr = (r - ringR) / 46.0f;
        if (dr > 3 || dr < -3)
          continue;
        const float amp =
            42.0f * std::exp(-static_cast<float>(frame - drop.born) / 150.0f);
        const float pulse = amp * std::exp(-dr * dr);
        offset += rv * (pulse / r);
        tilt += pulse * 0.012f * (dr < 0 ? -1.0f : 1.0f);
      }
      float c, sn;
      quantAngle(tilt, c, sn);
      const SkPoint at = rest + offset;
      const float half = adv * 0.5f;
      GlyphRSXformBatches::Batch &b = m_batches.batchFor(sw, color);
      b.glyphs.push_back(glyph);
      b.xforms.push_back({c, sn, at.fX - c * half + half, at.fY - sn * half});
    });

    canvas->clear(kPaper);
    const int glyphs = m_batches.draw(canvas);
    return {toUs(t1 - t0), static_cast<int>(layout.runs.size()), glyphs};
  }

private:
  struct Drop {
    SkPoint center;
    int born;
  };
  Paragraph m_para;
  std::vector<Drop> m_drops;
  std::vector<SkPoint> m_pending;
  GlyphRSXformBatches m_batches;
  std::mt19937 m_rng{31};
};

// ── Scene 6: vertical CJK with ruby, kenten, tate-chu-yoko ────────────────

class VerticalScene final : public Scene {
public:
  QString name() const override {
    return QStringLiteral("Vertical CJK — ruby · kenten · 縦中横");
  }
  bool supportsTextEdit() const override { return false; }

  FrameStats render(SkCanvas *canvas, SkISize size, double t, int /*frame*/,
                    const SceneParams & /*params*/, FontContext &ctx) override {
    const float w = size.width(), h = size.height();
    const float em = std::clamp(std::min(w, h) / 24.0f, 16.0f, 30.0f);
    if (!m_mincho) {
      m_mincho =
          ctx.fontMgr()->matchFamilyStyle("Hiragino Mincho ProN", SkFontStyle());
      if (!m_mincho)
        m_mincho = ctx.defaultTypeface();
    }
    if (em != m_em) {
      m_em = em;
      buildParagraphs(em);
    }

    canvas->clear(kPaper);

    // Vertical-rl block on the right.
    VerticalBlockFlow vertFlow(
        SkRect::MakeXYWH(w * 0.42f, 30, w * 0.55f, h - 90));
    LayoutOptions vertOpts;
    vertOpts.lineHeight = em * 1.9f;
    const auto t0 = Clock::now();
    Layout vertLayout = layoutParagraph(ctx, m_vert, vertFlow, vertOpts);
    const auto t1 = Clock::now();
    vertLayout.draw(canvas, m_vert);

    // Ruby + kenten are external utilities over the placed runs.
    rubyVertical(canvas, ctx, vertLayout, u"縦組", "たてぐみ");
    rubyVertical(canvas, ctx, vertLayout, u"文章", "ぶんしょう");
    rubyVertical(canvas, ctx, vertLayout, u"対応", "たいおう");
    kentenVertical(canvas, vertLayout, u"上から下へ",
                   0.6f + 0.4f * static_cast<float>(std::sin(t * 2.0)));

    // Horizontal comparison block on the left.
    BlockFlow horizFlow(SkRect::MakeXYWH(30, h * 0.14f, w * 0.34f, h * 0.7f));
    LayoutOptions horizOpts;
    horizOpts.lineHeight = em * 1.9f;
    Layout horizLayout = layoutParagraph(ctx, m_horiz, horizFlow, horizOpts);
    horizLayout.draw(canvas, m_horiz);
    rubyHorizontal(canvas, ctx, horizLayout, u"漢字", "かんじ");
    rubyHorizontal(canvas, ctx, horizLayout, u"圏点", "けんてん");

    drawCaption(canvas, ctx,
                "vertical-rl: UTR#50 orientation, 'vert' forms, "
                "tate-chu-yoko digits, ruby + kenten on top",
                {30, h - 28}, w - 60);

    return {toUs(t1 - t0),
            static_cast<int>(vertLayout.runs.size() + horizLayout.runs.size()),
            0};
  }

private:
  TextStyle jp(float size, VerticalForm form = VerticalForm::kAuto) const {
    TextStyle s = makeStyle(size, kInk, "ja", m_mincho);
    s.shaping.verticalForm = form;
    return s;
  }

  void buildParagraphs(float em) {
    m_vert.clear();
    m_vert.appendText("縦組みの文章は、上から下へ、", jp(em));
    m_vert.appendText("右から左へと流れる。平成", jp(em));
    m_vert.appendText("31", jp(em, VerticalForm::kTateChuYoko));
    m_vert.appendText("年", jp(em));
    m_vert.appendText("12", jp(em, VerticalForm::kTateChuYoko));
    m_vert.appendText("月、", jp(em));
    m_vert.appendText("TextFlow", jp(em));
    m_vert.appendText("は縦書きに対応した。", jp(em));
    m_vert.setWritingMode(WritingMode::kVerticalRL);

    m_horiz.clear();
    m_horiz.appendText(
        "漢字にルビを振ると、誰でも読みやすい。強調したい語には圏点を打つ。",
        jp(em * 0.8f));
  }

  void rubyVertical(SkCanvas *canvas, FontContext &ctx, const Layout &layout,
                    std::u16string_view base, const char *rubyText) {
    const std::vector<CharRange> found = findAll(m_vert, base);
    if (found.empty())
      return;
    bool valid = false;
    int line = 0;
    SkPoint origin = {0, 0};
    float begin = 0, end = 0;
    for (const PlacedRun &run : layout.runs) {
      const Word &word = m_vert.words()[run.wordIndex];
      if (word.textEnd <= found[0].start || word.textBegin >= found[0].end ||
          run.transformed)
        continue;
      if (!valid) {
        valid = true;
        line = run.line;
        origin = run.origin;
        begin = 0;
        end = word.width;
      } else if (run.line == line) {
        const float offset = run.origin.y() - origin.y();
        begin = std::min(begin, offset);
        end = std::max(end, offset + word.width);
      }
    }
    if (!valid)
      return;
    Paragraph ruby;
    ruby.appendText(rubyText, jp(m_em * 0.5f));
    ruby.setWritingMode(WritingMode::kVerticalRL);
    const float length = ruby.naturalWidth(ctx);
    LineSetFlow flow;
    flow.lines().push_back({LineInterval{
        {origin.x() + m_em * 0.62f,
         origin.y() + (begin + end) * 0.5f - length * 0.5f},
        {0, 1},
        length + 1}});
    layoutParagraph(ctx, ruby, flow).draw(canvas, ruby);
  }

  void kentenVertical(SkCanvas *canvas, const Layout &layout,
                      std::u16string_view emphasis, float alpha) {
    const std::vector<CharRange> found = findAll(m_vert, emphasis);
    if (found.empty())
      return;
    SkPaint dot;
    dot.setAntiAlias(true);
    dot.setColor(kAccent);
    dot.setAlphaf(alpha);
    for (const PlacedRun &run : layout.runs) {
      const Word &word = m_vert.words()[run.wordIndex];
      if (word.textEnd <= found[0].start || word.textBegin >= found[0].end ||
          run.transformed || !run.shaped->vertical)
        continue;
      float pen = 0;
      const ShapedWord &sw = *run.shaped;
      for (size_t g = 0; g < sw.advances.size(); ++g) {
        const uint32_t at = word.textBegin + sw.clusters[g];
        if (at >= found[0].start && at < found[0].end)
          canvas->drawCircle(run.origin.x() + m_em * 0.60f,
                             run.origin.y() + pen + sw.advances[g] * 0.5f,
                             m_em * 0.07f, dot);
        pen += sw.advances[g];
      }
    }
  }

  void rubyHorizontal(SkCanvas *canvas, FontContext &ctx, const Layout &layout,
                      std::u16string_view base, const char *rubyText) {
    const std::vector<CharRange> found = findAll(m_horiz, base);
    if (found.empty())
      return;
    float x0 = 0, x1 = 0, baseline = 0;
    bool valid = false;
    for (const PlacedRun &run : layout.runs) {
      const Word &word = m_horiz.words()[run.wordIndex];
      if (word.textEnd <= found[0].start || word.textBegin >= found[0].end)
        continue;
      if (!valid) {
        valid = true;
        x0 = run.origin.x();
        x1 = run.origin.x() + word.width;
        baseline = run.origin.y();
      } else if (run.origin.y() == baseline) {
        x1 = std::max(x1, run.origin.x() + word.width);
      }
    }
    if (!valid)
      return;
    Paragraph ruby;
    ruby.appendText(rubyText, jp(m_em * 0.4f));
    const float width = ruby.naturalWidth(ctx);
    LineSetFlow flow;
    flow.lines().push_back({LineInterval{
        {(x0 + x1) * 0.5f - width * 0.5f, baseline - m_em * 0.92f},
        {1, 0},
        width + 1}});
    layoutParagraph(ctx, ruby, flow).draw(canvas, ruby);
  }

  Paragraph m_vert, m_horiz;
  sk_sp<SkTypeface> m_mincho;
  float m_em = 0;
};

// ── Scene 7: query layer — regex markers that follow live edits ───────────

class MarkersScene final : public Scene {
public:
  QString name() const override { return QStringLiteral("Query & markers"); }
  QString defaultText() const override {
    return QStringLiteral(
        "Captain Ada watches the Beacon while Turing recalibrates the "
        "Lattice. Every capitalized Word in this paragraph was found once by "
        "an ICU regex and registered as a named marker set; the highlights "
        "you see are plain setPaint calls over those ranges. The paragraph "
        "keeps editing itself — words swap in and out below the markers — "
        "and the ranges follow the edits through the Paragraph's revision "
        "log, exactly like DOM Range objects. Delete the Beacon and its "
        "marker collapses; retype it and a fresh query picks it up again.");
  }

  FrameStats render(SkCanvas *canvas, SkISize size, double t, int frame,
                    const SceneParams &params, FontContext &ctx) override {
    if (!m_serif)
      m_serif = defaultSerif(ctx);
    if (m_body.ensure(params, defaultText(), m_serif)) {
      m_marks = MarkerSet(m_body.para);
      m_marks.set("caps",
                  findRegex(m_body.para, "\\b\\p{Lu}\\p{Ll}+").value_or(
                      std::vector<CharRange>{}));
    }

    // Scripted live edits: swap a word every ~2.5s; markers ride along.
    static const char *cycle[] = {"watches", "guards ", "studies", "shadows"};
    if (frame > 0 && frame % 150 == 0) {
      for (const char *word : cycle) {
        const size_t at = m_body.para.text().find(
            std::u16string(word, word + 7).c_str());
        if (at != std::u16string::npos) {
          m_body.para.replaceText(
              static_cast<uint32_t>(at), static_cast<uint32_t>(at + 7),
              cycle[(frame / 150) % 4]);
          break;
        }
      }
    }

    // Hue-cycling highlight over every marker range — paint-only restyle,
    // zero reshaping.
    PaintStyle highlight;
    const SkScalar hsv[3] = {
        std::fmod(static_cast<float>(t) * 40.0f, 360.0f), 0.75f, 0.72f};
    highlight.color = SkHSVToColor(hsv);
    m_marks.applyPaint(m_body.para, "caps", highlight);

    const float w = size.width(), h = size.height();
    BlockFlow flow(SkRect::MakeXYWH(w * 0.1f, 40, w * 0.8f, h - 80));
    LayoutOptions opts;
    opts.alignment = params.alignment;
    opts.breaker = params.breaker;
    opts.lineHeight = params.fontSize * 1.8f;

    const auto t0 = Clock::now();
    Layout layout = layoutParagraph(ctx, m_body.para, flow, opts);
    const auto t1 = Clock::now();

    canvas->clear(kPaper);
    layout.drawBatched(canvas, m_body.para);
    drawCaption(canvas, ctx,
                "regex \\b\\p{Lu}\\p{Ll}+ → MarkerSet; ranges follow the "
                "scripted edits",
                {w * 0.1f, h - 30}, w * 0.8f);
    return {toUs(t1 - t0), static_cast<int>(layout.runs.size()), 0};
  }

private:
  BodyCache m_body;
  MarkerSet m_marks;
  sk_sp<SkTypeface> m_serif;
};

// ── Scene 8: inline placeholders — pills and figures woven into the flow ──

class SlotsScene final : public Scene {
public:
  QString name() const override {
    return QStringLiteral("Inline slots & pills");
  }
  bool supportsTextEdit() const override { return false; }

  FrameStats render(SkCanvas *canvas, SkISize size, double t, int /*frame*/,
                    const SceneParams &params, FontContext &ctx) override {
    if (!m_serif) {
      m_serif = defaultSerif(ctx);
      m_sans = ctx.fontMgr()->matchFamilyStyle("Noto Sans", SkFontStyle());
      if (!m_sans)
        m_sans = ctx.defaultTypeface();
    }
    const float em = params.fontSize;
    if (m_builtEm != em) {
      m_builtEm = em;
      build(ctx, em);
    }

    // Pulse the first pill — resizing an inline object relayouts the whole
    // paragraph live, and reshapes exactly zero words.
    m_para.setPlaceholder(
        0, {m_pillWidths[0] *
                (1.0f + 0.4f * (0.5f + 0.5f * static_cast<float>(
                                                  std::sin(t * 1.7)))),
            em * 1.35f, em * 0.3f});

    const float w = size.width(), h = size.height();
    BlockFlow flow(SkRect::MakeXYWH(w * 0.1f, 44, w * 0.8f, h - 90));
    LayoutOptions opts;
    opts.alignment = params.alignment;
    opts.breaker = params.breaker;
    opts.lineHeight = em * 2.1f;

    const auto t0 = Clock::now();
    Layout layout = layoutParagraph(ctx, m_para, flow, opts);
    const auto t1 = Clock::now();

    canvas->clear(kPaper);
    layout.drawBatched(canvas, m_para);

    // Draw whatever belongs in each slot: pills for the first four, a
    // little inline "figure" for the last.
    const SkFont pillFont = makeFont(m_sans, em * 0.68f);
    SkFontMetrics pillMetrics;
    pillFont.getMetrics(&pillMetrics);
    for (const Layout::PlacedPlaceholder &placed :
         layout.placeholderRects(m_para)) {
      if (placed.index < static_cast<int>(m_pillTexts.size())) {
        SkPaint bg;
        bg.setAntiAlias(true);
        bg.setColor(placed.index == 0 ? kAccent : kBlue);
        canvas->drawRoundRect(placed.rect, placed.rect.height() * 0.5f,
                              placed.rect.height() * 0.5f, bg);
        Paragraph &label =
            m_pillLabels.get(m_pillTexts[static_cast<size_t>(placed.index)],
                             m_sans, em * 0.68f);
        const float textW = label.naturalWidth(ctx);
        PaintStyle white;
        white.color = SK_ColorWHITE;
        layoutLabel(ctx, label,
                    {placed.rect.centerX() - textW * 0.5f,
                     placed.rect.centerY() + middleBaselineOffset(pillMetrics)})
            .draw(canvas, label, &white);
      } else {
        SkPaint fill;
        fill.setAntiAlias(true);
        fill.setColor(kShape);
        canvas->drawRoundRect(placed.rect, 6, 6, fill);
        SkPaint line;
        line.setAntiAlias(true);
        line.setStyle(SkPaint::kStroke_Style);
        line.setStrokeWidth(2);
        line.setColor(kAccent);
        // A tiny sparkline so the "figure" reads as content.
        SkPathBuilder spark;
        for (int i = 0; i <= 16; ++i) {
          const float sx =
              placed.rect.left() + placed.rect.width() * i / 16.0f;
          const float sy = placed.rect.centerY() -
                           placed.rect.height() * 0.3f *
                               std::sin(i * 0.7f + static_cast<float>(t));
          if (i == 0)
            spark.moveTo(sx, sy);
          else
            spark.lineTo(sx, sy);
        }
        canvas->drawPath(spark.detach(), line);
      }
    }

    drawCaption(canvas, ctx,
                "appendPlaceholder() weaves fixed-size slots into the flow; "
                "placeholderRects() reports where they landed",
                {w * 0.1f, h - 30}, w * 0.8f);
    return {toUs(t1 - t0), static_cast<int>(layout.runs.size()), 0};
  }

private:
  void build(FontContext &ctx, float em) {
    m_pillTexts = {"LOW RISK", "42 ms", "β-channel", "cache-hot"};
    m_pillWidths.clear();
    for (const char *text : m_pillTexts) {
      Paragraph &label = m_pillLabels.get(text, m_sans, em * 0.68f);
      m_pillWidths.push_back(label.naturalWidth(ctx) + em * 1.1f);
    }

    m_para.clear();
    const TextStyle body = makeStyle(em, kInk, "", m_serif);
    const Placeholder pill{0, em * 1.35f, em * 0.3f};
    auto addPill = [&](size_t i) {
      Placeholder p = pill;
      p.width = m_pillWidths[i];
      m_para.appendPlaceholder(p, body);
    };
    m_para.appendText("Status pills ride the baseline like words — ", body);
    addPill(0);
    m_para.appendText(
        " — and the flow simply makes room: it wraps them, justifies around "
        "them, and re-places them when they resize. Round-trip latency ",
        body);
    addPill(1);
    m_para.appendText(" stays within budget, the experiment on the ", body);
    addPill(2);
    m_para.appendText(" keeps converging, and every relayout here is ", body);
    addPill(3);
    m_para.appendText(
        ". Even a small figure can sit inline without leaving the "
        "paragraph ",
        body);
    // Sized to the line: lines don't grow per-box yet (see README
    // limitations), so a slot taller than the leading would overlap the
    // previous line.
    m_para.appendPlaceholder({em * 7.0f, em * 1.8f, em * 0.3f}, body);
    m_para.appendText(
        " because the breaker treats every slot as an unbreakable word — "
        "the object-replacement character anchors it in the text, and the "
        "layout reports the rect where it landed.",
        body);
  }

  Paragraph m_para;
  LabelCache m_pillLabels;
  std::vector<const char *> m_pillTexts;
  std::vector<float> m_pillWidths;
  sk_sp<SkTypeface> m_serif, m_sans;
  float m_builtEm = 0;
};

} // namespace

std::vector<std::unique_ptr<Scene>> makeScenes() {
  std::vector<std::unique_ptr<Scene>> scenes;
  scenes.push_back(std::make_unique<ExclusionsScene>());
  scenes.push_back(std::make_unique<KnuthPlassScene>());
  scenes.push_back(std::make_unique<LoopScene>());
  scenes.push_back(std::make_unique<RainScene>());
  scenes.push_back(std::make_unique<RippleScene>());
  scenes.push_back(std::make_unique<VerticalScene>());
  scenes.push_back(std::make_unique<MarkersScene>());
  scenes.push_back(std::make_unique<SlotsScene>());
  return scenes;
}

} // namespace gallery
