#pragma once
// The geometric grammar shared by the MAGI studies.
//
// The screens differ in palette and topology, but they use the same small
// vocabulary: clipped rectangular panels, rounded capsules which may replace
// a corner with a straight cut, one T-shaped three-cell MAGI module, a narrow
// grotesque for Latin labels, and an extra-heavy mincho for Japanese display
// type. Keeping those here makes a changed stroke, corner, or fallback one UI
// decision instead of two unrelated sketches drifting apart.
//
// Placement does not belong here. Each frame decides where its panels and
// labels stand; this header only describes the shapes and type registers they
// share.

#include <include/core/SkColor.h>
#include <include/core/SkColorFilter.h>
#include <include/core/SkFontStyle.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkPoint.h>
#include <include/core/SkRect.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkSize.h>
#include <include/core/SkTypeface.h>
#include <include/effects/SkImageFilters.h>
#include <sigilgeometry/kit/Corners.h>
#include <sigilgeometry/kit/Generators.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/skia/Effect.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

#include <algorithm>
#include <cstdint>

namespace evangelion {

enum CutCorner : uint8_t {
  CutNone = 0,
  CutTopLeft = 1,
  CutTopRight = 2,
  CutBottomRight = 4,
  CutBottomLeft = 8,
};

/** A rectangular UI panel whose corners follow one rule.
 *
 *  Unmasked corners take the configured radius; masked corners replace
 *  that arc with a straight horizontal-by-vertical cut, so a panel
 *  expresses a non-square chamfer without baking its final size into a
 *  path. A zero radius is a square corner.
 *
 *  The mask's bits are the geometry kit's, corner for corner, so the two
 *  spellings are one value. */
struct PanelCorners {
  float radius = 0.0f;
  SkVector cut{0.0f, 0.0f};
  uint8_t cutMask = CutNone;
};

inline sigil::geometry::shapes::Chamfered panel(PanelCorners corners) {
  return sigil::geometry::shapes::Chamfered{
      .cut = corners.cut.fX,
      .cutRise = corners.cut.fY,
      .radius = corners.radius,
      .mask = sigil::geometry::shapes::Corner(corners.cutMask)};
}

/** The one three-cell MAGI module used at every network site. */
struct MagiModule {
  float barWidth = 343.0f;
  float barHeight = 176.0f;
  float stemWidth = 128.0f;
  float stemHeight = 104.0f;
  float cellWidth = 88.0f;
  float cellHeight = 150.0f;
  float cellRadius = 16.0f;
  float margin = 20.0f;

  [[nodiscard]] constexpr float totalHeight() const {
    return barHeight + stemHeight;
  }
  [[nodiscard]] constexpr float stemLeft() const {
    return (barWidth - stemWidth) * 0.5f;
  }
  [[nodiscard]] constexpr SkPoint labelCentre() const {
    return {barWidth * 0.5f + 2.0f, 66.0f};
  }

  [[nodiscard]] sigil::geometry::shapes::OutlineFunction outline() const {
    const MagiModule geometry = *this;
    return [geometry](SkSize) {
      const float stemLeft = geometry.stemLeft();
      SkPathBuilder path;
      path.moveTo(0, 0);
      path.lineTo(geometry.barWidth, 0);
      path.lineTo(geometry.barWidth, geometry.barHeight);
      path.lineTo(stemLeft + geometry.stemWidth, geometry.barHeight);
      path.lineTo(stemLeft + geometry.stemWidth, geometry.totalHeight());
      path.lineTo(stemLeft, geometry.totalHeight());
      path.lineTo(stemLeft, geometry.barHeight);
      path.lineTo(0, geometry.barHeight);
      path.close();
      return path.detach();
    };
  }

  /** Cell 1 is left, cell 3 is right, and cell 2 occupies the stem. */
  [[nodiscard]] SkRect cell(int number) const {
    if (number == 1)
      return SkRect::MakeXYWH(margin, margin - 1.0f, cellWidth, cellHeight);
    if (number == 3)
      return SkRect::MakeXYWH(barWidth - margin - cellWidth, margin - 1.0f,
                              cellWidth, cellHeight);
    return SkRect::MakeXYWH((barWidth - cellWidth) * 0.5f,
                            totalHeight() - margin - cellHeight, cellWidth,
                            cellHeight);
  }
};

/** The flat voting plate is generated from one square module on a three-way
 *  radial register. The ring is deliberately offset below the module centroid:
 *  it is a rear bus, not the construction circle used to place the modules. */
struct MagiVoteLayout {
  float canvasWidth = 1440.0f;
  float canvasHeight = 1052.0f;
  float frameInsetX = 72.0f;
  float frameInsetY = 58.0f;
  float moduleSide = 336.0f;
  SkPoint moduleCentre{720.0f, 565.0f};
  float moduleRadius = 298.0f;
  SkPoint busCentre{720.0f, 589.0f};
  float busRadius = 278.0f;

  [[nodiscard]] SkRect frame() const {
    return SkRect::MakeLTRB(frameInsetX, frameInsetY, canvasWidth - frameInsetX,
                            canvasHeight - frameInsetY);
  }

  /** MAGI numbering is MELCHIOR 1, BALTHASAR 2, CASPER 3. */
  [[nodiscard]] SkPoint centreFor(int number) const {
    constexpr float kRootThreeOverTwo = 0.866025404f;
    if (number == 1)
      return {moduleCentre.fX + moduleRadius * kRootThreeOverTwo,
              moduleCentre.fY + moduleRadius * 0.5f};
    if (number == 3)
      return {moduleCentre.fX - moduleRadius * kRootThreeOverTwo,
              moduleCentre.fY + moduleRadius * 0.5f};
    return {moduleCentre.fX, moduleCentre.fY - moduleRadius};
  }

  [[nodiscard]] float rotationFor(int number) const {
    if (number == 1) return -60.0f;
    if (number == 3) return 60.0f;
    return 0.0f;
  }

  /** Names face the shared bus; numerals occupy the outward half. */
  [[nodiscard]] float nameSlotY(int number) const {
    return number == 2 ? 0.80f : 0.31f;
  }
  [[nodiscard]] float numberSlotY(int number) const {
    return number == 2 ? 0.31f : 0.80f;
  }

  [[nodiscard]] SkRect moduleRect(int number) const {
    const SkPoint centre = centreFor(number);
    return SkRect::MakeXYWH(centre.fX - moduleSide * 0.5f,
                            centre.fY - moduleSide * 0.5f, moduleSide,
                            moduleSide);
  }
};

inline sk_sp<SkTypeface> groteskBold() {
  return sigil::weave::ports::face({"Helvetica", "Arial"},
                                   SkFontStyle::kBold_Weight);
}

inline sk_sp<SkTypeface> condensedBold() {
  return sigil::weave::ports::face(
      {"Helvetica Neue", "Arial Narrow", "DIN Condensed"},
      SkFontStyle(SkFontStyle::kBold_Weight, SkFontStyle::kCondensed_Width,
                  SkFontStyle::kUpright_Slant));
}

inline sk_sp<SkTypeface> condensedRegular() {
  return sigil::weave::ports::face(
      {"Helvetica Neue", "Arial Narrow", "DIN Condensed"},
      SkFontStyle(SkFontStyle::kNormal_Weight, SkFontStyle::kCondensed_Width,
                  SkFontStyle::kUpright_Slant));
}

inline sk_sp<SkTypeface> moduleLabel() {
  return sigil::weave::ports::face({"Helvetica", "Arial"});
}

inline sk_sp<SkTypeface> magiWordmark() {
  return sigil::weave::ports::face({"Times New Roman", "Times", "Noto Serif"},
                                   SkFontStyle::kBold_Weight);
}

inline sk_sp<SkTypeface> voteNumeral(int number) {
  return sigil::weave::ports::face(
      {"Helvetica", "Arial"}, SkFontStyle::kBold_Weight,
      number == 2 ? SkFontStyle::kOblique_Slant : SkFontStyle::kUpright_Slant);
}

inline sk_sp<SkTypeface> minchoHeavy() {
  return sigil::weave::ports::face(
      {"FOT-Matisse Pro EB", "FOT-Matisse ProN EB", "MatissePro-EB",
       "Noto Serif JP", "Hiragino Mincho ProN", "Noto Serif CJK JP"},
      SkFontStyle::kBlack_Weight);
}

/** A continuous halo with a softly exposed core. Skia's Gaussian filters
 *  integrate thin strokes without the repeated contours of a sparse gather.
 *  Screen blending preserves saturated panel colours under their own light. */
inline sigil::material::skia::Effect phosphor(float sigma = 1.6f,
                                              float strength = 0.55f) {
  using sigil::material::skia::Effect;
  const auto bright = Effect::brightPass(0.55f, 0.25f).imageFilter();
  auto halo = SkImageFilters::Blur(sigma, sigma, bright);
  const float gain[20] = {1, 0, 0, 0, 0, 0, 1, 0, 0,        0,
                          0, 0, 1, 0, 0, 0, 0, 0, strength, 0};
  halo = SkImageFilters::ColorFilter(SkColorFilters::Matrix(gain), halo);
  halo = SkImageFilters::Blend(SkBlendMode::kDstOut, halo, bright);
  auto core = SkImageFilters::Blur(0.35f, 0.35f, nullptr);
  return Effect::filter(
      SkImageFilters::Blend(SkBlendMode::kScreen, core, halo));
}

inline sigil::material::Material tube() {
  return sigil::material::field::crtOverlay({
      .uScanPitch = 2.0f,
      .uScanStrength = 0.012f,
      .uVigStrength = 0.16f,
      .uGrain = 0.035f,
  });
}

/** The Japanese display register used where Matisse EB is unavailable. A
 *  WHOLE style rather than a partial for the cascade: a stand-in below
 *  extra-bold is thickened with a stroke on its paint, which a partial
 *  cannot state. */
inline sigil::weave::TextStyle minchoDisplay(float size, SkColor4f color,
                                             float scaleX = 1.30f) {
  sigil::weave::TextStyle style =
      sigil::weave::textStyle({.face = minchoHeavy(),
                               .size = size,
                               .color = color,
                               .condense = scaleX});
  if (style.shaping.typeface && style.shaping.typeface->fontStyle().weight() <
                                    SkFontStyle::kExtraBold_Weight) {
    style.paint.foreground.setStyle(SkPaint::kStrokeAndFill_Style);
    style.paint.foreground.setStrokeWidth(size * 0.012f);
  }
  return style;
}

}  // namespace evangelion
