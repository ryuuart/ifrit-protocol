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
#include <include/core/SkFontStyle.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkPoint.h>
#include <include/core/SkRect.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkSize.h>
#include <include/core/SkTypeface.h>
#include <sigilgeometry/kit/Generators.h>
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
 *  Unmasked corners use the configured radius. Masked corners replace that
 *  arc with a straight horizontal-by-vertical clip, so a panel may express a
 *  non-square
 *  chamfer without baking its final size into a path. A zero radius produces
 *  a square corner. */
struct PanelCorners {
  float radius = 0.0f;
  SkVector cut{0.0f, 0.0f};
  uint8_t cutMask = CutNone;
};

inline sigil::geometry::shapes::OutlineFn panel(PanelCorners corners) {
  return [corners](SkSize size) {
    const float width = size.width();
    const float height = size.height();
    const float radius =
        std::clamp(corners.radius, 0.0f, std::min(width, height) * 0.5f);
    const float cutX = std::clamp(corners.cut.fX, 0.0f, width * 0.5f);
    const float cutY = std::clamp(corners.cut.fY, 0.0f, height * 0.5f);
    const float diameter = radius * 2.0f;

    SkPathBuilder path;
    if (corners.cutMask & CutTopLeft) {
      path.moveTo(cutX, 0.0f);
    } else if (radius > 0.0f) {
      path.moveTo(0.0f, radius);
      path.arcTo(SkRect::MakeXYWH(0, 0, diameter, diameter), 180, 90, false);
    } else {
      path.moveTo(0.0f, 0.0f);
    }

    if (corners.cutMask & CutTopRight) {
      path.lineTo(width - cutX, 0.0f);
      path.lineTo(width, cutY);
    } else if (radius > 0.0f) {
      path.lineTo(width - radius, 0.0f);
      path.arcTo(SkRect::MakeXYWH(width - diameter, 0, diameter, diameter), 270,
                 90, false);
    } else {
      path.lineTo(width, 0.0f);
    }

    if (corners.cutMask & CutBottomRight) {
      path.lineTo(width, height - cutY);
      path.lineTo(width - cutX, height);
    } else if (radius > 0.0f) {
      path.lineTo(width, height - radius);
      path.arcTo(SkRect::MakeXYWH(width - diameter, height - diameter, diameter,
                                  diameter),
                 0, 90, false);
    } else {
      path.lineTo(width, height);
    }

    if (corners.cutMask & CutBottomLeft) {
      path.lineTo(cutX, height);
      path.lineTo(0.0f, height - cutY);
    } else if (radius > 0.0f) {
      path.lineTo(radius, height);
      path.arcTo(SkRect::MakeXYWH(0, height - diameter, diameter, diameter), 90,
                 90, false);
    } else {
      path.lineTo(0.0f, height);
    }

    if (corners.cutMask & CutTopLeft) path.lineTo(0.0f, cutY);
    path.close();
    return path.detach();
  };
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

  [[nodiscard]] sigil::geometry::shapes::OutlineFn outline() const {
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

inline const sk_sp<SkTypeface>& groteskBold() {
  static const sk_sp<SkTypeface> face = sigil::weave::ports::pickTypeface(
      {"Helvetica", "Arial"}, SkFontStyle::kBold_Weight);
  return face;
}

inline const sk_sp<SkTypeface>& condensedBold() {
  static const sk_sp<SkTypeface> face = sigil::weave::ports::pickTypeface(
      {"Helvetica Neue", "Arial Narrow", "DIN Condensed"},
      SkFontStyle(SkFontStyle::kBold_Weight, SkFontStyle::kCondensed_Width,
                  SkFontStyle::kUpright_Slant));
  return face;
}

inline const sk_sp<SkTypeface>& condensedRegular() {
  static const sk_sp<SkTypeface> face = sigil::weave::ports::pickTypeface(
      {"Helvetica Neue", "Arial Narrow", "DIN Condensed"},
      SkFontStyle(SkFontStyle::kNormal_Weight, SkFontStyle::kCondensed_Width,
                  SkFontStyle::kUpright_Slant));
  return face;
}

inline const sk_sp<SkTypeface>& minchoHeavy() {
  static const sk_sp<SkTypeface> face = sigil::weave::ports::pickTypeface(
      {"Hiragino Mincho ProN", "Songti SC", "Noto Serif CJK JP"},
      SkFontStyle::kBold_Weight);
  return face;
}

inline sigil::weave::TextStyle type(const sk_sp<SkTypeface>& face, float size,
                                    SkColor4f color, float scaleX = 1.0f,
                                    float tracking = 0.0f) {
  return sigil::weave::textStyle({.face = face,
                                  .size = size,
                                  .color = color,
                                  .track = tracking,
                                  .condense = scaleX});
}

/** The Japanese display register used where Matisse EB is unavailable. */
inline sigil::weave::TextStyle minchoDisplay(float size, SkColor4f color,
                                             float scaleX = 1.12f) {
  sigil::weave::TextStyle style = type(minchoHeavy(), size, color, scaleX);
  style.paint.foreground.setStyle(SkPaint::kStrokeAndFill_Style);
  style.paint.foreground.setStrokeWidth(std::max(1.5f, size * 0.018f));
  return style;
}

}  // namespace evangelion
