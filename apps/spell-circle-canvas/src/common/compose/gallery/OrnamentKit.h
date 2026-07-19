#pragma once
// Parameterized ornament components shared by the showcase scenes.
// Everything here is a component over the public API: colors arrive as
// data (a Palette), so the same vine border, flourish, or carved frame
// renders in any tint — illuminated azure, blood crimson, royal oak.

#include "GalleryCore.h"

#include <ifritcompose/Decorations.h>

#include <include/core/SkPathBuilder.h>
#include <include/effects/SkPerlinNoiseShader.h>

#include <cmath>

namespace compose_gallery {

/** A manuscript palette — every ornament component below is driven by
 *  one of these, never by hard-coded colors. */
struct Palette {
  SkColor4f parchment; // page/panel ground
  SkColor4f ink;       // body text on that ground
  SkColor4f stem;      // vine stems / frame wood
  SkColor4f leaf;      // vine leaves
  SkColor4f gold;      // gilded accents: berries, trims, bosses
};

inline Palette azurePalette() {
  return {{0.91f, 0.86f, 0.72f, 1},
          {0.16f, 0.13f, 0.10f, 1},
          {0.16f, 0.29f, 0.48f, 1},
          {0.24f, 0.45f, 0.38f, 1},
          {0.83f, 0.62f, 0.18f, 1}};
}

inline Palette crimsonPalette() {
  return {{0.93f, 0.83f, 0.72f, 1},
          {0.22f, 0.09f, 0.07f, 1},
          {0.52f, 0.13f, 0.14f, 1},
          {0.35f, 0.36f, 0.14f, 1},
          {0.85f, 0.58f, 0.21f, 1}};
}

inline Palette emeraldPalette() {
  return {{0.85f, 0.89f, 0.76f, 1},
          {0.10f, 0.16f, 0.11f, 1},
          {0.13f, 0.35f, 0.22f, 1},
          {0.30f, 0.52f, 0.24f, 1},
          {0.80f, 0.66f, 0.25f, 1}};
}

inline Palette oakPalette() {
  return {{0.88f, 0.80f, 0.62f, 1},
          {0.20f, 0.13f, 0.07f, 1},
          {0.36f, 0.22f, 0.11f, 1},
          {0.45f, 0.33f, 0.15f, 1},
          {0.85f, 0.64f, 0.22f, 1}};
}

/** Parchment ground: the base tint modulated by fractal noise — the
 *  patterned dialog background, in any color. */
inline Fill parchmentFill(SkColor4f base, float frequency = 0.045f) {
  sk_sp<SkShader> noise =
      SkShaders::MakeFractalNoise(frequency, frequency, 3, 7.0f);
  return Fill::shader(SkShaders::Blend(
      SkBlendMode::kSoftLight,
      SkShaders::Color(base, nullptr), std::move(noise)));
}

/** Curling vine run along the node's outline: seeded curls alternate
 *  sides of the contour, a leaf rides each curl, gilded berries land
 *  every third step. The general procedural border, palette-driven. */
inline ContourWalk vineWalk(const Palette &pal, float spacing = 26.0f,
                            float scale = 1.0f) {
  ContourWalk walk;
  walk.spacing = spacing;
  walk.draw = [pal, scale](SkCanvas &c, const PathSample &s,
                           const PaintContext &) {
    const int step = (int)(s.distance / 26.0f);
    const uint32_t h = (uint32_t)step * 2654435761u;
    const float side = (step & 1) ? 1.0f : -1.0f;
    c.scale(scale, scale * side);

    SkPaint p;
    p.setAntiAlias(true);
    p.setStyle(SkPaint::kStroke_Style);
    p.setStrokeCap(SkPaint::kRound_Cap);
    p.setStrokeWidth(1.8f);
    p.setColor4f(pal.stem, nullptr);

    // The stem: an S-curl lifting off the contour, seeded height.
    const float tipX = 2.0f + (float)(h % 5);
    const float tipY = -13.0f - (float)(h % 6);
    SkPathBuilder b;
    b.moveTo(-10, 0);
    b.cubicTo(-2, -1, 5, -6, tipX, tipY);
    // Curl: a hook past the tip.
    b.cubicTo(tipX + 4, tipY - 3, tipX + 5, tipY + 2, tipX + 1,
              tipY + 3);
    c.drawPath(b.detach(), p);

    // A leaf riding the curl.
    p.setStyle(SkPaint::kFill_Style);
    p.setColor4f(pal.leaf, nullptr);
    c.save();
    c.translate(1, -8);
    c.rotate(-38.0f + (float)(h % 34));
    c.drawOval(SkRect::MakeXYWH(0, -2.6f, 9.5f, 5.2f), p);
    c.restore();

    // Gilded berry every third step.
    if (step % 3 == 0) {
      p.setColor4f(pal.gold, nullptr);
      c.drawCircle(tipX, tipY, 2.3f, p);
    }
  };
  return walk;
}

/** An illuminated corner flourish: a sweeping spiral stem with leaves
 *  and gilded dots, drawn for the top-left and mirrored into any
 *  quadrant (0=NW, 1=NE, 2=SE, 3=SW). */
inline PaintProgram cornerFlourish(const Palette &pal, int quadrant) {
  return [pal, quadrant](SkCanvas &c, const PaintContext &ctx) {
    const float w = ctx.size.width(), h = ctx.size.height();
    const bool right = quadrant == 1 || quadrant == 2;
    const bool bottom = quadrant >= 2;
    c.translate(right ? w : 0, bottom ? h : 0);
    c.scale(right ? -1 : 1, bottom ? -1 : 1);

    SkPaint p;
    p.setAntiAlias(true);
    p.setStyle(SkPaint::kStroke_Style);
    p.setStrokeCap(SkPaint::kRound_Cap);
    p.setStrokeWidth(2.6f);
    p.setColor4f(pal.stem, nullptr);

    // The main sweep out of the corner, plus two branching curls.
    SkPathBuilder b;
    b.moveTo(4, 4);
    b.cubicTo(w * 0.55f, h * 0.06f, w * 0.72f, h * 0.22f, w * 0.88f,
              h * 0.52f);
    b.moveTo(w * 0.34f, h * 0.10f);
    b.cubicTo(w * 0.40f, h * 0.34f, w * 0.30f, h * 0.52f, w * 0.14f,
              h * 0.60f);
    b.moveTo(w * 0.60f, h * 0.16f);
    b.cubicTo(w * 0.68f, h * 0.36f, w * 0.58f, h * 0.52f, w * 0.44f,
              h * 0.58f);
    c.drawPath(b.detach(), p);

    // Spiral terminal on the main sweep.
    b.moveTo(w * 0.88f, h * 0.52f);
    b.cubicTo(w * 0.96f, h * 0.66f, w * 0.86f, h * 0.76f, w * 0.78f,
              h * 0.68f);
    b.cubicTo(w * 0.74f, h * 0.62f, w * 0.80f, h * 0.56f, w * 0.85f,
              h * 0.60f);
    p.setStrokeWidth(1.8f);
    c.drawPath(b.detach(), p);

    // Leaves along the sweeps.
    p.setStyle(SkPaint::kFill_Style);
    const SkPoint leafAt[4] = {{w * 0.30f, h * 0.08f},
                               {w * 0.58f, h * 0.14f},
                               {w * 0.36f, h * 0.42f},
                               {w * 0.60f, h * 0.44f}};
    const float leafRot[4] = {18, 40, 96, 74};
    for (int i = 0; i < 4; ++i) {
      p.setColor4f(pal.leaf, nullptr);
      c.save();
      c.translate(leafAt[i].x(), leafAt[i].y());
      c.rotate(leafRot[i]);
      c.drawOval(SkRect::MakeXYWH(0, -4, 15, 8), p);
      c.restore();
    }

    // Gilded dots at the terminals.
    p.setColor4f(pal.gold, nullptr);
    c.drawCircle(w * 0.14f, h * 0.60f, 3.4f, p);
    c.drawCircle(w * 0.44f, h * 0.58f, 3.0f, p);
    c.drawCircle(w * 0.82f, h * 0.64f, 2.6f, p);
    c.drawCircle(8, 8, 4.2f, p);
  };
}

/** Starburst outline for spiky shout dialogs: `spikes` points, `depth`
 *  0..1 how deep the valleys cut. */
inline std::function<SkPath(SkSize)> starburstOutline(int spikes,
                                                      float depth) {
  return [spikes, depth](SkSize s) {
    SkPathBuilder b;
    const float cx = s.width() / 2, cy = s.height() / 2;
    const int n = spikes * 2;
    for (int i = 0; i < n; ++i) {
      const float a = (float)i / (float)n * 6.28318f - 1.5708f;
      const float k = (i & 1) ? 1.0f - depth : 1.0f;
      const SkPoint pt = {cx + std::cos(a) * cx * k,
                          cy + std::sin(a) * cy * k};
      if (i == 0)
        b.moveTo(pt);
      else
        b.lineTo(pt);
    }
    b.close();
    return b.detach();
  };
}

/** Scalloped outline: rounded lobes bulging out of each edge — the
 *  cloud-bubble / wax-seal silhouette. */
inline std::function<SkPath(SkSize)> scallopOutline(float lobe = 14.0f) {
  return [lobe](SkSize s) {
    SkPathBuilder b;
    const float w = s.width(), h = s.height(), inset = lobe * 0.5f;
    auto edge = [&](SkPoint from, SkPoint to) {
      const float dx = to.x() - from.x(), dy = to.y() - from.y();
      const float len = std::sqrt(dx * dx + dy * dy);
      const int n = std::max(1, (int)(len / lobe));
      // Outward normal for clockwise travel.
      const float nx = dy / len, ny = -dx / len;
      for (int i = 0; i < n; ++i) {
        const float t0 = (float)i / n, t1 = (float)(i + 1) / n;
        const SkPoint mid = {from.x() + dx * (t0 + t1) * 0.5f +
                                 nx * lobe * 0.55f,
                             from.y() + dy * (t0 + t1) * 0.5f +
                                 ny * lobe * 0.55f};
        b.quadTo(mid, {from.x() + dx * t1, from.y() + dy * t1});
      }
    };
    b.moveTo(inset, inset);
    edge({inset, inset}, {w - inset, inset});
    edge({w - inset, inset}, {w - inset, h - inset});
    edge({w - inset, h - inset}, {inset, h - inset});
    edge({inset, h - inset}, {inset, inset});
    b.close();
    return b.detach();
  };
}

/** Draws a carved dialog frame onto an intermediate canvas and hands
 *  back the texture: rounded wood band, gilded trim, corner bosses,
 *  edge studs, translucent parchment center. The nine-slice source —
 *  generate once per palette, stretch everywhere. `size` must be
 *  sliceable at divs {size/3, 2*size/3}. */
inline sk_sp<SkImage> makeCarvedFrame(const Palette &pal, int size = 96) {
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(size, size));
  SkCanvas &c = *surface->getCanvas();
  c.clear(SK_ColorTRANSPARENT);
  const float s = (float)size;

  SkPaint p;
  p.setAntiAlias(true);

  // Parchment center (stretches under content).
  SkColor4f ground = pal.parchment;
  ground.fA = 0.96f;
  p.setColor4f(ground, nullptr);
  c.drawRoundRect(SkRect::MakeLTRB(5, 5, s - 5, s - 5), s * 0.14f,
                  s * 0.14f, p);

  // Wood band.
  p.setStyle(SkPaint::kStroke_Style);
  p.setStrokeWidth(s * 0.10f);
  p.setColor4f(pal.stem, nullptr);
  c.drawRoundRect(SkRect::MakeLTRB(s * 0.07f, s * 0.07f, s * 0.93f,
                                   s * 0.93f),
                  s * 0.16f, s * 0.16f, p);

  // Gilded trims inside and outside the band.
  p.setStrokeWidth(1.6f);
  p.setColor4f(pal.gold, nullptr);
  c.drawRoundRect(SkRect::MakeLTRB(s * 0.135f, s * 0.135f, s * 0.865f,
                                   s * 0.865f),
                  s * 0.10f, s * 0.10f, p);
  c.drawRoundRect(SkRect::MakeLTRB(s * 0.02f, s * 0.02f, s * 0.98f,
                                   s * 0.98f),
                  s * 0.20f, s * 0.20f, p);

  // Corner bosses: darker disc + gilded diamond stud.
  p.setStyle(SkPaint::kFill_Style);
  const float bossAt[4][2] = {{s * 0.13f, s * 0.13f},
                              {s * 0.87f, s * 0.13f},
                              {s * 0.87f, s * 0.87f},
                              {s * 0.13f, s * 0.87f}};
  for (auto &at : bossAt) {
    SkColor4f dark = pal.stem;
    dark.fR *= 0.6f; dark.fG *= 0.6f; dark.fB *= 0.6f;
    p.setColor4f(dark, nullptr);
    c.drawCircle(at[0], at[1], s * 0.085f, p);
    p.setColor4f(pal.gold, nullptr);
    SkPathBuilder diamond;
    const float r = s * 0.042f;
    diamond.moveTo(at[0], at[1] - r);
    diamond.lineTo(at[0] + r, at[1]);
    diamond.lineTo(at[0], at[1] + r);
    diamond.lineTo(at[0] - r, at[1]);
    diamond.close();
    c.drawPath(diamond.detach(), p);
  }

  // Edge studs at the thirds (they live in the stretchable bands, so
  // they repeat visually as ticks when the frame stretches).
  p.setColor4f(pal.gold, nullptr);
  for (float t : {0.5f}) {
    c.drawCircle(s * t, s * 0.075f, 2.6f, p);
    c.drawCircle(s * t, s * 0.925f, 2.6f, p);
    c.drawCircle(s * 0.075f, s * t, 2.6f, p);
    c.drawCircle(s * 0.925f, s * t, 2.6f, p);
  }

  return surface->makeImageSnapshot();
}

/** The carved frame as a nine-slice decoration for any box size. */
inline Slice carvedFrameSlice(
    const std::shared_ptr<const ifrit::image::ImageAsset> &asset) {
  Slice nine;
  nine.asset = asset;
  const int size = asset ? asset->width() : 96;
  nine.xDivs = {size / 3, size * 2 / 3};
  nine.yDivs = {size / 3, size * 2 / 3};
  return nine;
}

/** An illuminated panel: parchment ground, ink-colored chrome stroke,
 *  vine border — the parameterized dialog every palette shares. */
inline Element illuminatedPanel(const Palette &pal) {
  return box().corners({10})
      .fill(parchmentFill(pal.parchment))
      .background(ifrit::compose::util::shadow({0, 0, 0, 0.35f}, {2, 3},
                                               8))
      .foreground(ifrit::compose::util::stroke(2.4f,
                                               Fill::color(pal.stem)))
      .foreground(vineWalk(pal, 24.0f, 0.75f));
}

} // namespace compose_gallery
