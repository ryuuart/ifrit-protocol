#pragma once

// Construction data and drawing primitives owned by this study.

#include <include/core/SkFontMgr.h>
#include <include/core/SkMaskFilter.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkString.h>
#include <include/effects/SkImageFilters.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilcompose/brush/Adaptors.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/brush/LayerStyles.h>
#include <sigilcompose/brush/PixelStyles.h>
#include <sigilcompose/core/Grid.h>
#include <sigilcompose/core/Instances.h>
#include <sigilcompose/core/Paint.h>
#include <sigilcompose/core/Pattern.h>
#include <sigilcompose/kit/Chrome.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Gloss.h>
#include <sigilcompose/kit/Kinetic.h>
#include <sigilcompose/kit/Placers.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilgeometry/kit/Corners.h>
#include <sigilgeometry/kit/Generators.h>
#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilgeometry/path/Edges.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/kit/Pbr.h>
#include <sigilmaterial/pattern/Patterns.h>
#include <sigilmaterial/sdf/Sdf.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilmaterial/skia/Effect.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmotion/bind/Bind.h>
#include <sigilmotion/values/Keyframes.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Page.h>
#include <sigilsketch/kit/Scrollbar.h>
#include <sigilworld/frame/Frame.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "../twoadvanced_v3/TwoAdvanced.h"

namespace sketch = sigil::sketch;
namespace world = sigil::world;
namespace camera = sigil::geometry::mesh::camera;
namespace field = sigil::material::field;
namespace mskia = sigil::material::skia;
namespace msdf = sigil::material::sdf;
namespace motion = sigil::motion;
namespace path = sigil::geometry::path;
namespace patterns = sigil::material::pattern;
namespace shapes = sigil::geometry::shapes;
namespace styles = sigil::compose::styles;
namespace weave = sigil::weave;

using namespace sigil::compose;
// Absolute placement: this composition is pinned, so a node says
// where it goes rather than a layout deciding.
using sigil::compose::kit::at;
using namespace std::chrono_literals;
namespace ch = choreograph;

namespace tav {
using namespace twoadvanced;

// ---------------------------------------------------------------------------
// Palette — every value sampled from one of the reference artefacts above,
// never picked by eye.

constexpr SkColor4f kBgTop = hexColor(0x4A100F);  // page gradient, top
constexpr SkColor4f kBgMid = hexColor(0x1A0001);
constexpr SkColor4f kBgBot = hexColor(0x0A0000);   // …faded to near-black
constexpr SkColor4f kChrome = hexColor(0x571119);  // THE chrome maroon
constexpr SkColor4f kChromeHi = hexColor(0x6A1B21);
constexpr SkColor4f kD1 = hexColor(0x180707);  // footer-dock HUD darks
constexpr SkColor4f kD2 = hexColor(0x260909);
constexpr SkColor4f kD3 = hexColor(0x370C0D);
constexpr SkColor4f kD4 = hexColor(0x400E0F);
constexpr SkColor4f kD5 = hexColor(0x4C1010);
constexpr SkColor4f kD6 = hexColor(0x7A2626);    // dock hairline/label ink
constexpr SkColor4f kD7 = hexColor(0xA34040);    // dock title ink
constexpr SkColor4f kCyan = hexColor(0x7BDAD6);  // logo wordmark core
constexpr SkColor4f kCyanRing = hexColor(0x95C9CC);
constexpr SkColor4f kDust = hexColor(0x8D7777);  // the dusty-rose third neutral
constexpr SkColor4f kDustDim = hexColor(0x735757);
constexpr SkColor4f kTealBar = hexColor(0x2C7B80);  // status-bar segment
constexpr SkColor4f kGlow = hexColor(0x01D0D5);     // MAINFRAME portal core
constexpr SkColor4f kPanel = hexColor(0x579797);    // monitor-panel body
constexpr SkColor4f kPanelHi = hexColor(0x84B8B6);
constexpr SkColor4f kPanelSh = hexColor(0x3C8282);
constexpr SkColor4f kCta = hexColor(0x700000);  // LAUNCH / ARCHIVES core
constexpr SkColor4f kCtaHi = hexColor(0xB27E82);
constexpr SkColor4f kNear = hexColor(0xF3F3F3);
constexpr SkColor4f kBody = hexColor(0xC9DEDD);
constexpr SkColor4f kDate = hexColor(0x1C4040);
constexpr SkColor4f kHeadDim = hexColor(0xB8A0A0);

/** The shadow tone: the complement spelling of `mskia::scale()`, because a
 * bevel is authored as "how much darker" rather than as a surviving fraction.
 */
inline SkColor4f dark(SkColor4f c, float k) { return mskia::scale(c, 1 - k); }

// ---------------------------------------------------------------------------
// Type — the studio's chassis (the faces, the 1/1000-em tracking unit and
// the text alias) plus THIS artefact's own register: Helvetica
// CondensedBlack is the whole chrome voice, Arial Black is the headline
// weight, and Arial is the only thing prose is ever set in.

inline sigil::weave::TextStyle micro(float size, SkColor4f c, float tr = 200) {
  return sigil::weave::kit::tracked(condBlack(), size, c, tr, 0.92f);
}
inline sigil::weave::TextStyle label(float size, SkColor4f c, float tr = 100) {
  return sigil::weave::kit::tracked(condBlack(), size, c, tr, 0.88f);
}
inline sigil::weave::TextStyle heavy(float size, SkColor4f c, float tr = 40) {
  return sigil::weave::kit::tracked(blackFace(), size, c, tr, 0.94f);
}
inline sigil::weave::TextStyle prose(float size, SkColor4f c) {
  return sigil::weave::kit::tracked(arial(), size, c, 0);
}

// ---------------------------------------------------------------------------
// Geometry vocabulary — chamfers, not radii. Nothing on this interface is
// round; every corner that is not square is cut at 45°.

/** An OPEN hairline across the node — the trim() reveal primitive: a
 *  stroked open outline draws itself on when trim's end ramps 0→1. */
inline Shape ray(float dirX, float dirY) {
  return keyedShape(std::pair{dirX, dirY}, [dirX, dirY](SkSize s) {
    SkPathBuilder b;
    b.moveTo(dirX < 0 ? s.width() : 0, dirY < 0 ? s.height() : 0);
    b.lineTo(dirX < 0 ? 0 : s.width(), dirY < 0 ? 0 : s.height());
    return b.detach();
  });
}

// ---------------------------------------------------------------------------
// Decoration vocabulary — the hairline kit the whole idiom runs on. All
// value schemes, so a static bracketed panel prunes with no memo.

/** The rail flare tick read off leftsidepanel.gif: a near-vertical
 *  hairline ending in a small flag, with a soft highlight travelling down
 *  it once per `period`. The rail is 24 px wide at this ×2 scale, so a
 *  90 px flare has to lean about 8° off VERTICAL to fit inside it. */
struct RailFlares {
  SkColor4f color = hexColor(0x99AAAA);
  float period = 6.0f, phase = 0.0f;

  bool operator==(const RailFlares&) const = default;
  bool isAnimated() const { return true; }
  void paint(SkCanvas& c, const PaintContext& ctx) const {
    const float w = ctx.size.width(), h = ctx.size.height();
    const float ys[3] = {h * 0.167f, h * 0.5f, h * 0.833f};
    const float len = 90.0f, lean = 12.6f;
    const double tt = ctx.elapsedSeconds + phase;
    const float scan = (float)std::fmod(tt, (double)period) / period;
    SkPaint p;
    p.setAntiAlias(true);
    p.setStyle(SkPaint::kStroke_Style);
    p.setStrokeWidth(2);
    for (float y : ys) {
      const float y0 = y - len * 0.5f;
      const float x0 = w * 0.5f - lean * 0.5f;
      SkPathBuilder b;
      b.moveTo(x0, y0);
      b.lineTo(x0 + lean, y0 + len);
      const SkPath path = b.detach();
      p.setColor4f(mskia::withAlpha(color, 0.55f), nullptr);
      c.drawPath(path, p);
      SkPaint f;
      f.setAntiAlias(true);
      f.setColor4f(mskia::withAlpha(color, 0.8f), nullptr);
      c.drawRect(SkRect::MakeXYWH(x0 + lean - 3, y0 + len, 6, 3), f);
      SkPaint g;
      g.setAntiAlias(true);
      g.setStyle(SkPaint::kStroke_Style);
      g.setStrokeWidth(2);
      g.setColor4f(
          mskia::withAlpha(kCyan, 0.9f * (1.0f - std::abs(scan - 0.5f) * 2)),
          nullptr);
      SkPathBuilder hb;
      hb.moveTo(x0 + lean * scan, y0 + len * scan);
      const float s2 = std::min(1.0f, scan + 0.16f);
      hb.lineTo(x0 + lean * s2, y0 + len * s2);
      c.drawPath(hb.detach(), g);
    }
  }
};

// ---------------------------------------------------------------------------

/** THE TWO PANEL CLASSES the SWF's symbol table names — FSingleBevelPanel
 *  and FDoubleBevelPanel — as one function over the era's token set. The
 *  base fill, a lifted top/left highlight over a darkened bottom/right
 *  shadow (three px against two, because the SWF's were), and — where a
 *  @p gap is asked for — the same pair again fainter that far in.
 *  MAINFRAME, FEATURE SYSTEM and PRESS UPDATES wear the doubled one;
 *  everything smaller is single.
 *
 *  Both tones come off the face, which is why the two classes were one
 *  component with a flag in 2Advanced's own kit and are one call here.
 *  The pair is kept INSIDE the silhouette, so a chamfered panel wears its
 *  bevel on the chamfers with nothing said about corners, and the inner
 *  ring rides OVER the content because at a three-pixel gap on a
 *  three-pixel padding it stands exactly on the padding line. */
inline Element bevelPanel(Element e, SkColor4f base, float gap = 0) {
  e.fill(base);
  kit::bevelled(e, kit::bevels::flash(base, gap));
  return e;
}

}  // namespace tav

// ===========================================================================
