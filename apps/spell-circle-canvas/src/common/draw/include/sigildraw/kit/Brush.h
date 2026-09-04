#pragma once

/** @file
 * Procedural drawing tools over a SigilDraw Pen.
 *
 * A Brush is ordinary style data. A Stroke is ordinary sampled geometry.
 * paint() combines the two through the pen's existing points, lines, colour,
 * blending and deterministic random stream, then restores the style it found.
 */

#include <include/core/SkColor.h>
#include <include/core/SkPoint.h>
#include <sigildraw/Constants.h>
#include <sigildraw/Pen.h>
#include <sigildraw/kit/Dab.h>
#include <sigildraw/kit/Path.h>

#include <functional>
#include <optional>
#include <span>

namespace sigil::draw::brush {

/** How the tool deposits pigment along the sampled centreline. */
enum class Tip {
  Grain,    ///< dry-media particles dispersed around the centreline
  Fibers,   ///< parallel, intermittently dry hairs
  Nib,      ///< one continuous pressure-width mark
  Scatter,  ///< particles distributed around each sample
  Image,    ///< a luminance or alpha mask stamped at every dab
  Custom,   ///< a caller-provided drawing callback stamped at every dab
};

/** How an image or callback tip is oriented at each dab. */
enum class Rotation {
  Fixed,
  Natural,
  Random,
  Tilt,  ///< follows the azimuth of a tilted stylus
};

/** How an image tip becomes a mask. Inverted luminance accepts the common
 * dark-mark-on-white-tip image directly; Alpha uses an authored alpha channel.
 */
enum class ImageMask { InvertedLuminance, Alpha };

/** A three-point pressure envelope. The first half interpolates start to
 *  middle and the second half interpolates middle to end. */
struct Pressure {
  struct Gaussian {
    float center = 0.5f;
    float width = 0.5f;
    float sharpness = 3.25f;
    float minimum = 0.0f;
    float maximum = 1.0f;
    float centerJitter = 0.0f;
    float widthJitter = 0.0f;
  };

  struct Variation {
    float offset = 0.08f;
    float scale = 0.08f;
    float warp = 0.06f;
    float tilt = 0.06f;

    bool operator==(const Variation&) const = default;
  };

  float start = 1.0f;
  float middle = 1.0f;
  float end = 1.0f;
  std::function<float(float)> curve;
  std::optional<Gaussian> gaussian;
  std::optional<Variation> variation = Variation{};

  [[nodiscard]] float at(float progress) const;
  [[nodiscard]] static Pressure gaussianProfile(float centerJitter,
                                                float widthJitter,
                                                float minimum, float maximum);
};

/** The full description of one procedural drawing tool. Width, spacing and
 * scatter are canvas units; opacity is a unit value; grain is a non-negative
 * deposit density; bristles is the fiber or particle count. */
struct Brush {
  Tip tip = Tip::Grain;
  SkColor4f color{0, 0, 0, 1};
  float width = 8.0f;
  float spacing = 1.0f;
  float opacity = 0.45f;
  float scatter = 0.3f;
  float grain = 0.8f;
  int bristles = 12;
  Pressure pressure{0.65f, 1.0f, 0.45f};
  Constant blend = BLEND;
  Rotation rotation = Rotation::Natural;
  float angle = 0.0f;
  float aspect = 1.0f;
  float sizeJitter = 0.0f;
  float opacityJitter = 0.0f;
  float spacingJitter = 0.0f;
  float speedSize = 0.0f;
  float speedOpacity = 0.0f;
  float speedReference = 1200.0f;
  float pressureSize = 1.0f;
  float pressureOpacity = 0.1f;
  float tiltSize = 0.0f;
  float tiltOpacity = 0.0f;
  float tiltAspect = 0.0f;
  float tiltOffset = 0.0f;
  float sharpness = 0.5f;
  float noise = 0.0f;
  bool markerTip = true;
  sk_sp<SkImage> imageTip;
  ImageMask imageMask = ImageMask::InvertedLuminance;
  /** A custom tip draws around the origin in a nominal one-unit square. The
   * engine has already applied position, rotation and final dab size. */
  std::function<void(Pen&, const Dab&)> customTip;
};

/** Stock tools. Every field remains public on the returned value, so a sketch
 *  can tune it or describe a tool of its own without a registry. */
Brush pencil(SkColor4f color, float width = 1.4f);
Brush charcoal(SkColor4f color, float width = 9.0f);
Brush marker(SkColor4f color, float width = 16.0f);
Brush watercolor(SkColor4f color, float width = 22.0f);
Brush spray(SkColor4f color, float width = 18.0f);

/** Resolves stroke-wide random dynamics once. A live input stroke keeps the
 * returned value for its whole lifetime, so input event boundaries cannot
 * change opacity or the pressure envelope. */
Brush prepareStroke(Pen& pen, const Brush& tool);

/** Deposits @p tool along @p stroke. The pen's style and transform are
 *  restored after the mark; the transform still applies to the mark itself. */
void paint(Pen& pen, const Brush& tool, std::span<const Sample> stroke);

/** Deposits already sampled dabs. This is the executor seam shared by stored
 * paths, live stylus input, image tips and caller-defined tips. */
struct DepositOptions {
  bool start = true;
  bool end = true;
};

void deposit(Pen& pen, const Brush& tool, std::span<const Dab> dabs,
             DepositOptions options = {});

/** Straight and smoothed conveniences over the reusable path constructors. */
void line(Pen& pen, const Brush& tool, SkPoint from, SkPoint to,
          float startPressure = 1.0f, float endPressure = 1.0f);
void spline(Pen& pen, const Brush& tool, std::span<const Sample> controls,
            float curvature = 0.5f);

/** Traces a field at the brush's spacing and paints the resulting path. */
template <DirectionField Field>
void flowLine(Pen& pen, const Brush& tool, SkPoint start, float length,
              float seconds, const Field& field, float pressure = 1.0f) {
  paint(pen, tool,
        trace(start, length, tool.spacing, seconds, field, pressure));
}

}  // namespace sigil::draw::brush
