#pragma once

// Page dimensions, palette and type registers.

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkColorFilter.h>
#include <include/core/SkData.h>
#include <include/core/SkFontMgr.h>
#include <include/core/SkMaskFilter.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkString.h>
#include <include/effects/SkImageFilters.h>
#include <sigilcompose/brush/Adaptors.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/brush/LayerStyles.h>
#include <sigilcompose/core/Paint.h>
#include <sigilcompose/core/Pattern.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Kinetic.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilcompose/typography/Typography.h>
#include <sigildata/decode/Json.h>
#include <sigilgeometry/kit/Corners.h>
#include <sigilgeometry/kit/Generators.h>
#include <sigilgeometry/path/Edges.h>
#include <sigilimage/asset/Embedded.h>
#include <sigilimage/asset/ImageAsset.h>
#include <sigilmaterial/pattern/Patterns.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilmaterial/skia/Effect.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmotion/bind/Bind.h>
#include <sigilmotion/values/Keyframes.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Page.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <span>
#include <string>
#include <vector>

#include "TwoAdvanced.h"

namespace data = sigil::data;
namespace sketch = sigil::sketch;
namespace mskia = sigil::material::skia;
namespace patterns = sigil::material::pattern;
namespace motion = sigil::motion;
namespace path = sigil::geometry::path;
namespace shapes = sigil::geometry::shapes;
namespace weave = sigil::weave;

using namespace sigil::compose;
// Absolute placement: this composition is pinned, so a node says
// where it goes rather than a layout deciding.
using sigil::compose::kit::at;
using namespace std::chrono_literals;
namespace ch = choreograph;

namespace tv3 {
using namespace twoadvanced;

// ---------------------------------------------------------------------------
// Palette — sampled from the studio's own 1920×1080 capture, never eyed.

constexpr SkColor4f kPage = hexColor(0x182337);    // outer page ground
constexpr SkColor4f kPageHi = hexColor(0x314361);  // page ground, top of ramp
constexpr SkColor4f kDeep = hexColor(0x1C283C);    // stage art darks
constexpr SkColor4f kSeam = hexColor(0x2E3C57);    // seams, scroll strip
constexpr SkColor4f kNavbar = hexColor(0x4A5972);  // navbar body
constexpr SkColor4f kSteel = hexColor(0x7886A6);   // the mid chrome steel
constexpr SkColor4f kSteelDim = hexColor(0x68758A);
constexpr SkColor4f kSteelHi = hexColor(0xC3CCD8);  // lifted chrome
constexpr SkColor4f kNear = hexColor(0xF1F4F8);     // titles, wordmark
constexpr SkColor4f kBody = hexColor(0xA8B2C0);     // module body copy
constexpr SkColor4f kInk = hexColor(0x202B3F);      // dark type on steel bars
constexpr SkColor4f kHost = hexColor(0xE8920A);     // the ONE saturated mark

// ---------------------------------------------------------------------------
// Type — the studio's chassis (the faces, the 1/1000-em tracking unit and
// the text alias) plus THIS artefact's own register: the 2024 rebuild is
// lettered in one grotesque at two weights, tracked wide for the chrome
// and loose for the prose. The chrome's face and condensation are the
// page's own font, stated once on its root; a chrome label says only its
// size, colour and tracking, and the prose restates the face it is set
// in and that it is not condensed.

/** The chrome register: tracking quoted in 1/1000 em of @p size. */
inline sigil::weave::Type micro(float size, SkColor4f c, float tr = 160) {
  return {.size = size, .color = c, .track = size * tr / 1000.0f};
}
/** The prose register: the medium weight, loosely tracked, uncondensed. */
inline sigil::weave::Type prose(float size, SkColor4f c) {
  return {.face = grot(),
          .size = size,
          .color = c,
          .track = size * 30 / 1000.0f,
          .condense = 1.0f};
}

// ---------------------------------------------------------------------------
// The page's own copy, beside the sketch

/** THE RECORD AT @p key of @p doc — `data/content.json`, whose keys are
 *  `wordmark`, `navbar`, `art`, `scroll`, `footer`, `boot` and `modules`
 *  (one record per module, each with its glyph, its bar, its head and its
 *  copy). A missing file or key reads as a null value, so a reader falls
 *  back to no words at all. */
inline const data::Json& record(const std::shared_ptr<const data::Json>& doc,
                                const char* key) {
  static const data::Json none;
  return doc ? (*doc)[key] : none;
}

/** @p node's words — empty where the document does not carry them. */
inline Utf8 words(const data::Json& node) { return Utf8(node.text()); }

/** The members of @p node's list, as the run `each()` walks. */
inline std::span<const data::Json> run(const data::Json& node) {
  return node.items();
}

/** The studio's two lines from a value read off the DOCUMENT rather than
 *  from a literal, spelled the same way. */
inline Element t(const Utf8& s, weave::Type partial) {
  return text(s).font(std::move(partial));
}
inline Element t(const Utf8& s, weave::TextStyle style) {
  return text(s, std::move(style));
}

// ---------------------------------------------------------------------------
// Frame constants — the capture's own geometry.

constexpr float kW = 1920, kH = 1080;
constexpr float kStageX = 300, kStageW = 1320;
constexpr float kArtY = 217, kArtH = 398;  // the home art band
constexpr float kModY = 645;               // three-module row
constexpr float kModH = 252;               // bodies end on the divider band
// The support module's right column runs to three lines of copy where its
// left one runs to two, and its call-to-action is pinned to the panel floor:
// the row has to be tall enough for the LONGER column or the third line and
// the button share a baseline.
constexpr float kRowY = 941, kRowH = 91;           // mailing/support/follow row
constexpr float kPanelW = (kStageW - 2 * 10) / 3;  // 433⅓ — three across

}  // namespace tv3

// ===========================================================================
