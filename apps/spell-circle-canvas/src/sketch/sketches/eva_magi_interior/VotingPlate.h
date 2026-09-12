#pragma once

// The voting plate: palette, type registers, panel layout and circuitry.

#include <include/core/SkCanvas.h>
#include <include/core/SkFontMgr.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkTypeface.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilcompose/brush/Adaptors.h>
#include <sigilcompose/brush/Brushes.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/brush/Hatches.h>
#include <sigilcompose/brush/Lines.h>
#include <sigilcompose/brush/Rails.h>
#include <sigilcompose/brush/Ribbons.h>
#include <sigilcompose/brush/Stamps.h>
#include <sigilcompose/core/Paint.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Routers.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilcompose/kit/Strokes.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilcore/compute/Chance.h>
#include <sigilgeometry/kit/Corners.h>
#include <sigilgeometry/kit/Curves.h>
#include <sigilgeometry/kit/Generators.h>
#include <sigilgeometry/path/Arrange.h>
#include <sigilgeometry/path/Edges.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilmaterial/skia/Effect.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmeasure/check/Check.h>
#include <sigilmotion/bind/Bind.h>
#include <sigilmotion/values/Keyframes.h>
#include <sigilmotion/values/Time.h>
#include <sigilmotion/values/Transition.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Page.h>
#include <sigilsketch/kit/Rows.h>
#include <sigilsketch/kit/Theme.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "EvangelionUi.h"
#include "Infection.h"

namespace sketch = sigil::sketch;
namespace chance = sigil::core::chance;
namespace mskia = sigil::material::skia;
namespace motion = sigil::motion;
namespace arrange = sigil::geometry::arrange;
namespace path = sigil::geometry::path;
namespace shapes = sigil::geometry::shapes;
namespace measure = sigil::measure;

using namespace sigil::compose;
using namespace std::chrono_literals;
namespace weave = sigil::weave;
namespace ch = choreograph;

namespace magi {

constexpr float kW = 1440.0f, kH = 1052.0f;

const SkColor4f kGround = hexColor(0x040001);
const SkColor4f kMint = hexColor(0x59DFA4);
const SkColor4f kRed = hexColor(0xAA0506);  // MELCHIOR p50 — and the traces, to
const SkColor4f kRedHot =
    hexColor(0xB0090A);  // within 1 LSB: ONE colour, TWO
                         // coverages, which is the whole idea
const SkColor4f kTraceDark = hexColor(0x080102);  // the pour's own keyline
const SkColor4f kInk = hexColor(0x00093C);        // label on a panel: dark NAVY
const SkColor4f kInkRed =
    hexColor(0x3A0A18);  // the same ink over MELCHIOR's red
const SkColor4f kOrange = hexColor(0xE56C1A);  // rails, rims, the MAGI mark
const SkColor4f kOrangeDim = hexColor(0xB4571A);
const SkColor4f kKanji =
    hexColor(0xE16417);  // very slightly redder than the rails
const SkColor4f kKanjiHot = hexColor(0xFFA050);  // 提訴 mid-pulse
const SkColor4f kGold = hexColor(0xC4AC50);
const SkColor4f kGoldHot = hexColor(0xE8D371);
const SkColor4f kGoldPeak = hexColor(0xF4DD8C);
const SkColor4f kGreen =
    hexColor(0x175746);  // 0.16% of the frame and it carries
const SkColor4f kGreenHi = hexColor(0x2A6B58);    // the whole top band
const SkColor4f kEdgeLight = hexColor(0xA88CAF);  // the cel's own edge light
const SkColor4f kBgRule =
    hexColor(0x6E3228);  // the plate's dim diagonal furniture

// ---------------------------------------------------------------------------
// TYPE

inline sk_sp<SkTypeface> latin() { return evangelion::condensedBold(); }
inline sk_sp<SkTypeface> latinPlain() { return evangelion::condensedRegular(); }
inline sk_sp<SkTypeface> han() { return evangelion::minchoHeavy(); }

// ---------------------------------------------------------------------------
// THE PLATE, in a rectified coordinate system. The side panels are one module
// mirrored around the central axis; the upper panel presents a matching flat.

struct Panel {
  const char* key;
  const char* label;
  const char* number;
  SkRect box;
  float rotation;
  int circuitRuns;
  SkPoint seed;
};

struct InfectionTiming {
  double seedAt;
  double fullAt;
  double levels;
  double phase;
};

// Whole cells still switch as steps, but each panel advances through enough
// levels that adjacent traces turn over instead of large regions jumping.
inline constexpr std::array<InfectionTiming, 3> kInfection = {{
    {12.0, 22.0, 64.0, 0.0},
    {0.48, 6.0, 96.0, 0.0},
    {-0.5, 2.0, 48.0, 0.37},
}};

inline std::vector<Panel> panels() {
  const evangelion::MagiVoteLayout layout;

  return {
      {"casper",
       "CASPER",
       "3",
       layout.moduleRect(3),
       layout.rotationFor(3),
       0,
       {11.5f, 1.5f}},
      {"balthasar",
       "BALTHASAR",
       "2",
       layout.moduleRect(2),
       layout.rotationFor(2),
       13,
       {6.5f, 5.0f}},
      {"melchior",
       "MELCHIOR",
       "1",
       layout.moduleRect(1),
       layout.rotationFor(1),
       6,
       {10.5f, 0.5f}},
  };
}

// ---------------------------------------------------------------------------
// The panel's own orthogonal circuitry — near-navy hairlines on the azure,
// generated as point runs and fed to lines::Rails, because the frame shows
// DOUBLED runs and a single hairline is not the honest spelling.

/** One number in [0, 1) for a run, a step in it and a salt — the three
 *  folded into one seed and read off the library's stream, so nothing
 *  here carries a mixer. */
inline float hashF(int a, int b, int salt) {
  return chance::Stream::mix64((uint32_t)(a * 73856093) ^
                               (uint32_t)(b * 19349663) ^
                               (uint32_t)(salt * 83492791))
      .unit();
}
// THE CELL IS THE TRACE'S WIDTH. At thirty-two pixels a finger is a slab
// and the pour reads as a mask over the panel; the epigraph the whole
// artefact hangs on is "the growing lines are the electrical circuits",
// and a circuit's line is thin. Ten pixels puts nine times as many cells
// under the same front, so the anisotropic lobes read as the long
// orthogonal runs they compute — a trace network, not a stain.
constexpr float kCell = 10.0f;
constexpr int kDX[4] = {1, 0, -1, 0};
constexpr int kDY[4] = {0, 1, 0, -1};

inline SkPath ownCircuitry(SkSize s, int salt, int runs) {
  SkPathBuilder b;
  for (int r = 0; r < runs; ++r) {
    float x = std::round(hashF(r, 3, salt) * s.width() / kCell) * kCell;
    float y = std::round(hashF(r, 7, salt) * s.height() / kCell) * kCell;
    int dir = (int)((unsigned)(int)(hashF(r, 11, salt) * 4.0f) & 3u);
    const int steps = 3 + (int)(hashF(r, 13, salt) * 7.0f);
    bool started = false;
    for (int i = 0; i < steps; ++i) {
      // jackestar's lineAngleVariation, as a per-step turn probability
      if (hashF(r, i * 31 + 5, salt) < 0.26f)
        dir = (int)((unsigned)(dir + (hashF(r, i * 37, salt) < 0.5f ? 1 : 3)) &
                    3u);
      const float nx = x + (float)kDX[dir] * kCell;
      const float ny = y + (float)kDY[dir] * kCell;
      if (nx < 4 || ny < 4 || nx > s.width() - 4 || ny > s.height() - 4) break;
      if (!started) {
        b.moveTo(x, y);
        started = true;
      }
      b.lineTo(nx, ny);
      x = nx;
      y = ny;
    }
  }
  return b.detach();
}

/** The hollow pads the frame shows on the azure. */
inline SkPath ownPads(SkSize s, int salt, int count) {
  SkPathBuilder b;
  for (int i = 0; i < count; ++i) {
    const float x = std::round(hashF(i, 21, salt) * s.width() / kCell) * kCell;
    const float y = std::round(hashF(i, 23, salt) * s.height() / kCell) * kCell;
    const float w = kCell * (1.0f + std::floor(hashF(i, 27, salt) * 3.0f));
    const float h = kCell * (hashF(i, 29, salt) < 0.6f ? 0.5f : 1.0f);
    if (x + w > s.width() - 6 || y + h > s.height() - 6 || x < 6 || y < 6)
      continue;
    b.addRect(SkRect::MakeXYWH(x, y, w, h));
  }
  return b.detach();
}

}  // namespace magi
