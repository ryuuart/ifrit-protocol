#pragma once

/** @file
 * The SWEPT OPERATOR: one 2D profile carried along a rail into a Mesh.
 * A circle makes a tube, a two-point line makes a ribbon, any flattened
 * outline makes an extrusion.
 *
 * It sits with the point operators because it is one: GPU-focused mesh
 * work described as a value, performed by an executor, with a device
 * executor beside the host one dispatching the same arithmetic.
 *
 * A sweep is two things, and only one of them is arithmetic. The RING
 * VERTICES are a pure function of one frame, one profile point and the
 * size the profile is scaled to there — that is what an executor
 * performs, and what a device replaces. The TOPOLOGY around them is
 * integer: which vertices a quad joins, the fan that closes an end, and
 * the averaging that forms geometric normals from the triangles once
 * they exist. The topology is the same wherever the vertices were
 * formed, so it is written once here and never twice.
 *
 * The ring arithmetic itself is written ONCE, in
 * `mesh/pop/kernels/Sweep.slang`, and the build compiles that source twice:
 * to C++, which the built-in executor calls, and to SPIR-V, which a
 * runtime that owns a device dispatches. Neither side re-derives a
 * formula, which is what lets two tiers be held to bit identity rather
 * than to a tolerance.
 */

#include <include/core/SkPath.h>
#include <sigilcore/comparable/Erased.h>

#include <cstdint>
#include <functional>
#include <glm/glm.hpp>
#include <span>
#include <string>
#include <vector>

#include "sigilgeometry/mesh/Mesh.h"
#include "sigilgeometry/mesh/curve/Curve.h"
#include "sigilgeometry/mesh/curve/Frame.h"
#include "sigilgeometry/path/Polyline.h"

namespace sigil::geometry::device {
class Device;
}  // namespace sigil::geometry::device

namespace sigil::geometry::mesh::pop {

/** The cross-sections `sweep()` carries. A profile is a plain
 *  `path::Polyline` in Skia's y-down 2D space — x runs along the
 *  frame's binormal, y against its normal, the convention
 *  `mesh::extrude()` uses — so any outline reaches a sweep by being
 *  flattened. The two below are UNIT shapes: `SweepOptions::scale`
 *  sizes them, which is why a radius or a width is not a parameter
 *  here. */
namespace profile {

/** The unit circle, one point per @p sides step around it plus the
 *  seam point DUPLICATED — the contour is open and its first and last
 *  points coincide — because the swept surface's u must run 0 to 1
 *  across the seam instead of wrapping back onto vertex zero. */
path::Polyline circle(int sides = 12);

/** The unit-width segment across the frame's binormal: two points,
 *  open. Swept, it is a flat band — pair it with
 *  `SweepOptions::Normals::Frame` so the band faces its rail. */
path::Polyline line();

/** The first contour of @p outline, flattened. The door from the 2D
 *  shape vocabulary: a star, a squircle, an `ops::PathOp` recipe's
 *  result. A closed outline sweeps as a wrapped ring; an open one
 *  sweeps as a strip. */
path::Polyline fromPath(const SkPath& outline, float tolerance = 0.4f);

}  // namespace profile

/** Where each swept vertex's normal comes from. `SweepOptions::Normals`
 *  is the name to spell it by; it stands on its own here because the
 *  executor seam is declared before the options that carry one. */
enum class SweepNormals : uint8_t {
  Radial,     ///< the profile offset itself — a round profile's outward
  Frame,      ///< the rail's normal — a flat profile's facing
  Geometric,  ///< averaged from the formed triangles — any profile
};

/** THE RING ARITHMETIC AS ONE PIECE, and the description a run of it
 *  is. Everything a ring vertex is a function of, packed the way the
 *  kernel declares it: every buffer four floats wide, so the bytes the
 *  host fills are the bytes a device binds — there is no layout to
 *  report and none to guess. */
namespace kernel {

/** ONE SWEEP'S PARAMETERS, in the layout the kernel declares. */
struct Args {
  /** x: how many rings; y: how many points the profile has; z: 1 when a
   *  vertex normal is the profile's own offset and 0 when it is the
   *  rail's; w: how many spans u is divided across. */
  glm::uvec4 code{0, 0, 0, 0};
};

/** ONE SWEEP, READY TO RUN: the rings as the kernel reads them, and the
 *  profile beside them.
 *
 *  The SIZE each ring scales its profile by is here rather than in the
 *  arguments because it is per ring, and it is a HOST answer: a taper is
 *  an arbitrary function of t, so it is evaluated once per ring on the
 *  side that can call it and carried across as a number. */
struct Dispatch {
  Args args;
  /** Per ring: xyz the frame's position, w the size. */
  std::vector<glm::vec4> railPosition;
  /** Per ring: xyz the frame's normal, w the frame's t. */
  std::vector<glm::vec4> railNormal;
  /** Per ring: xyz the frame's binormal. */
  std::vector<glm::vec4> railBinormal;
  /** Per profile point: xy the contour point. */
  std::vector<glm::vec4> profile;

  /** How many vertices a run of this writes into each output lane. */
  [[nodiscard]] size_t vertices() const {
    return (size_t)args.code.x * (size_t)args.code.y;
  }
};

/** THE HOST RUN: the kernel's own generated C++ over @p dispatch. Both
 *  @p positions and @p normals address at least `dispatch.vertices()`
 *  values, and each spends all four floats: a position carries u in its
 *  fourth and a normal carries v in its. Two lanes rather than three,
 *  because a uv is two numbers and both of those floats were spare. */
void run(const Dispatch& dispatch, glm::vec4* positions, glm::vec4* normals);

/** THE KERNEL AS A DEVICE RUNS IT: the SPIR-V this build compiled from
 *  the same source `run` came out of, with one `NoContraction`
 *  decoration per arithmetic result — see `mesh/Spirv.h` for why that
 *  decoration is not optional. The words stand for the life of the
 *  process. */
std::span<const uint32_t> spirv();

/** How many vertices one dispatched group covers. It is the kernel's own
 *  `numthreads`, and the kernel drops the lanes past the vertex count
 *  itself, so a count that is not a multiple of it needs no second
 *  path. */
inline constexpr uint32_t kGroupSize = 64;

}  // namespace kernel

/** The one step a sweep runs through: the ring vertices. An
 *  implementation owns whatever device it needs; the dispatch carries
 *  none, so a description built for one executor runs on any of them. */
class SweepExecutor {
 public:
  virtual ~SweepExecutor() = default;

  /** What this executor is called, in a message that names it. */
  [[nodiscard]] virtual std::string name() const = 0;

  /** The ring vertices @p work describes, into two lanes each at least
   *  `work.vertices()` long, in ring-major order: ring 0's profile
   *  points first, then ring 1's. A position carries u in its fourth
   *  float and a normal carries v in its. */
  virtual void rings(const kernel::Dispatch& work, glm::vec4* positions,
                     glm::vec4* normals) const = 0;
};

/** The executor a sweep's rings are formed on, carried as a comparable
 *  value. */
class SweepRuntime : public core::Erased<SweepExecutor> {
 public:
  using core::Erased<SweepExecutor>::Erased;
  SweepRuntime() = default;
  SweepRuntime(core::Erased<SweepExecutor> erased)  // NOLINT: a Runtime IS
                                                    // its value
      : core::Erased<SweepExecutor>(std::move(erased)) {}

  /** The built-in executor: the kernel's generated C++ over the rings,
   *  on the host. Every call returns the same value, so two default
   *  option sets compare equal. */
  static SweepRuntime cpu();
};

/** How a profile rides a rail. */
struct SweepOptions {
  /** Where each swept vertex's normal comes from. */
  using Normals = SweepNormals;

  int segments = 96;  ///< rings along the spline, when a spline is given
  float scale = 1;    ///< the profile's size
  /** Size over t (multiplies `scale`); null = constant. */
  std::function<float(float t)> taper;
  glm::vec3 up = {0, 1, 0};  ///< seeds the rail's first normal
  Normals normals = Normals::Radial;
  /** Close the rail's two ends with a fan to the ring's centre. The
   *  fan assumes a convex profile, and a closed spline has no ends,
   *  so the spline overload drops it for one. */
  bool caps = false;
  /** Who forms the ring vertices. The default is the built-in host
   *  executor; assigning another one is the whole of switching
   *  runtimes, and the topology, the caps and the geometric normals are
   *  the same either way. */
  SweepRuntime runtime = SweepRuntime::cpu();
};

/** @p rail carrying @p profile under @p options, as a dispatch. False —
 *  leaving @p out untouched — when there is nothing to sweep: fewer than
 *  two rings, or fewer than two profile points. The taper is evaluated
 *  here, once per ring. */
bool describe(const std::vector<curve::Frame3>& rail,
              const path::Polyline& profile, const SweepOptions& options,
              kernel::Dispatch* out);

/**
 * THE DEVICE EXECUTOR, beside the CPU one: the `SweepRuntime` that forms
 * a sweep's rings on @p device.
 *
 * WHAT RUNS WHERE. The RING VERTICES — the per-vertex arithmetic, which
 * is the whole of what a sweep computes — are one compute dispatch over
 * the rail and the profile, read back once at the end. Everything else a
 * sweep is made of stays on the host and is not a second piece of
 * arithmetic: the quads that join four ring vertices and the fan that
 * closes an end are integer, the taper is an arbitrary host function
 * evaluated once per ring, and a geometric normal is a reduction over
 * triangles that do not exist until the vertices do.
 *
 * THE TWO TIERS ARE HELD TO BIT IDENTITY, not to a distance. That is
 * possible because the ring arithmetic is one piece of Slang compiled
 * twice under a float model pinned at both ends: the generated C++ is
 * compiled with contraction off, the SPIR-V carries one `NoContraction`
 * decoration per arithmetic result, and the device's driver is told not
 * to relax its floating point before the instance exists.
 *
 * Two runtimes made by one call to this compare equal; two separate
 * calls do not, because they hold separate device state. Defined only
 * where this library was built with a device feature.
 */
SweepRuntime sweepDeviceRuntime(::sigil::geometry::device::Device& device);

/** Carry @p profile along @p rail into one Mesh. THE swept primitive:
 *  a circle profile forms a tube, a two-point line forms a ribbon or
 *  a banner, a flattened outline forms an extrusion along the curve.
 *  Each ring is the profile placed on one frame — x along the
 *  binormal, y against the normal — scaled by `scale` times `taper`
 *  at that frame's t. u runs across the profile and v is the frame's
 *  t. The ring vertices are formed on `options.runtime`; every executor
 *  writes exactly these vertices from the same rail. */
Mesh sweep(const std::vector<curve::Frame3>& rail,
           const path::Polyline& profile, const SweepOptions& options = {});

/** The same sweep over a spline, whose parallel-transport frames are
 *  the rail (`segments` of them, seeded by `up`). A closed spline has
 *  no ends, so it drops `caps`. */
Mesh sweep(const curve::Spline3& spline, const path::Polyline& profile,
           const SweepOptions& options = {});

}  // namespace sigil::geometry::mesh::pop
