#pragma once

/** @file
 * The parametric curves — open silhouettes evaluated in the unit
 * frame: the raw and keyed `parametric`, Lissajous, harmonograph, rose,
 * spiral and trochoid.
 */

#include <include/core/SkPathBuilder.h>

#include <cstdint>

#include <string>

#include "sigilgeometry/kit/Generators.h"
#include "sigilgeometry/path/Polyline.h"
#include "sigilgeometry/path/Skia.h"

namespace sigil::geometry::shapes {

// ---------------------------------------------------------------------------
// Parametric curves
//
// Everything above generates a closed SHAPE from parameters. These
// generate a curve DEFINED by a parameter — Lissajous, harmonograph,
// rose, epitrochoid, spirograph, orbit trace, phase portrait.
//
// They evaluate in a UNIT frame centred on the box — x and y in [-1, 1] —
// and are then scaled onto the node's half-extents, so a curve keeps its
// proportions when the box changes and an amplitude means the same thing
// everywhere.
//
// The named families are comparable values like every other generator.
// The raw `parametric(fn, …)` holds YOUR callable, which cannot compare —
// key it (`parametric("orbit-a", fn, …)`) to make it a value: the key plus
// the sampling parameters become its identity, on the author's contract
// that one key means one function.

/** Samples @p f over t ∈ [t0, t1] into a polyline. @p f returns UNIT
 *  coordinates (±1 spans the box); @p samples is the segment count, and
 *  @p close joins the last point back to the first.
 *
 *      .shape(shapes::parametric([](float t) {
 *        return SkPoint{std::cos(3 * t), std::sin(2 * t)};
 *      }, 0, 2 * SK_FloatPI, 720))
 *
 *  UNKEYED: the callable is the whole identity and it cannot compare, so
 *  a node shaped by this re-records every render() — the escape hatch.
 *  The keyed overload below is the prunable spelling. */
struct Parametric {
  std::function<SkPoint(float)> f;
  float t0 = 0.0f;
  float t1 = 1.0f;
  int samples = 512;
  bool close = false;
  SkPath path(SkSize s) const {
    return detail::samplePolyline(f, t0, t1, samples, close, s);
  }
  SkPath operator()(SkSize s) const { return path(s); }
};

inline Parametric parametric(std::function<SkPoint(float)> f, float t0,
                             float t1, int samples = 512, bool close = false) {
  return Parametric{std::move(f), t0, t1, samples, close};
}

/** The KEYED parametric: comparable by (key, t0, t1, samples, close), so
 *  the node prunes. The key is the FUNCTION's identity — the author's
 *  contract is that one key always names one curve; reusing a key for a
 *  different `f` silently keeps whichever recorded first. Change the key
 *  (or fold the changing number into a parameter of a named family
 *  below) when the curve changes. */
struct KeyedParametric {
  std::string key;
  std::function<SkPoint(float)> f;
  float t0 = 0.0f;
  float t1 = 1.0f;
  int samples = 512;
  bool close = false;
  bool operator==(const KeyedParametric& o) const {
    return key == o.key && t0 == o.t0 && t1 == o.t1 && samples == o.samples &&
           close == o.close;
  }
  SkPath path(SkSize s) const {
    return detail::samplePolyline(f, t0, t1, samples, close, s);
  }
  SkPath operator()(SkSize s) const { return path(s); }
};

inline KeyedParametric parametric(std::string_view key,
                                  std::function<SkPoint(float)> f, float t0,
                                  float t1, int samples = 512,
                                  bool close = false) {
  return KeyedParametric{std::string(key), std::move(f), t0, t1,
                         samples,          close};
}

/** Lissajous figure: x = sin(a·t + δ), y = sin(b·t). The ratio a:b picks
 *  the family (1:1 an ellipse, 3:2 the classic pretzel, 5:4 a tight
 *  weave) and δ its phase — the same two numbers a physical harmonograph
 *  is set to. `turns` is how many 2π the parameter runs for; the curve
 *  closes when a:b is rational and `turns` covers the period. */
struct Lissajous {
  float a = 3.0f;
  float b = 2.0f;
  float deltaDeg = 0.0f;
  float turns = 1.0f;
  int samples = 720;
  bool operator==(const Lissajous&) const = default;
  SkPath path(SkSize s) const;
  SkPath operator()(SkSize s) const { return path(s); }
};

inline Lissajous lissajous(float a, float b, float deltaDeg = 0.0f,
                           float turns = 1.0f, int samples = 720) {
  return Lissajous{a, b, deltaDeg, turns, samples};
}

/** A harmonograph: a Lissajous whose amplitudes DECAY, which is what
 *  makes a real pen-and-pendulum figure spiral inward instead of
 *  retracing one closed rosette. @p damping is the exponential rate per
 *  unit t; @p precession spins the whole figure as it draws (the rotating
 *  turntable under John Whitney's pendulum). */
struct Harmonograph {
  float a = 3.0f;
  float b = 2.0f;
  float deltaDeg = 0.0f;
  float damping = 0.05f;
  float precession = 0.0f;
  float turns = 6.0f;
  int samples = 2000;
  bool operator==(const Harmonograph&) const = default;
  SkPath path(SkSize s) const;
  SkPath operator()(SkSize s) const { return path(s); }
};

inline Harmonograph harmonograph(float a, float b, float deltaDeg = 0.0f,
                                 float damping = 0.05f, float precession = 0.0f,
                                 float turns = 6.0f, int samples = 2000) {
  return Harmonograph{a, b, deltaDeg, damping, precession, turns, samples};
}

/** Rose (rhodonea) r = cos(k·θ). Integer @p k gives k petals when k is
 *  odd and 2k when even; rational k gives the multi-lobed forms. */
struct Rose {
  float k = 3.0f;
  float turns = 1.0f;
  int samples = 720;
  bool operator==(const Rose&) const = default;
  SkPath path(SkSize s) const;
  SkPath operator()(SkSize s) const { return path(s); }
};

inline Rose rose(float k, float turns = 1.0f, int samples = 720) {
  return Rose{k, turns, samples};
}

/** Spiral from the centre outward. @p logarithmic switches Archimedean
 *  (even spacing — a clock spring, a record groove) for logarithmic
 *  (constant angle — a nautilus, a galaxy arm). */
struct Spiral {
  float turns = 3.0f;
  bool logarithmic = false;
  float growth = 0.25f;
  int samples = 720;
  bool operator==(const Spiral&) const = default;
  SkPath path(SkSize s) const;
  SkPath operator()(SkSize s) const { return path(s); }
};

inline Spiral spiral(float turns = 3.0f, bool logarithmic = false,
                     float growth = 0.25f, int samples = 720) {
  return Spiral{turns, logarithmic, growth, samples};
}

/** Epitrochoid / hypotrochoid — the spirograph pair. A circle of radius
 *  @p r rolls around one of radius @p R (outside for an epitrochoid,
 *  inside when @p inside), with the pen @p d from its centre. Everything
 *  is normalised so the figure fills the box. */
struct Trochoid {
  float R = 5.0f;
  float r = 3.0f;
  float d = 5.0f;
  bool inside = false;
  float turns = 1.0f;
  int samples = 1440;
  bool operator==(const Trochoid&) const = default;
  SkPath path(SkSize s) const;
  SkPath operator()(SkSize s) const { return path(s); }
};

inline Trochoid trochoid(float R, float r, float d, bool inside = false,
                         float turns = 1.0f, int samples = 1440) {
  return Trochoid{R, r, d, inside, turns, samples};
}

}  // namespace sigil::geometry::shapes
