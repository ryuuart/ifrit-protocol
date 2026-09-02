#pragma once

/** @file
 * THE STAMPING ARITHMETIC AS ONE PIECE, and the description a run of it
 * is — TouchDesigner's Copy, Houdini's copy-to-points, the operator that
 * turns a point set into geometry.
 *
 * A stamping is two things, and only one of them is arithmetic. The
 * VERTICES are a pure function of one point's lanes and one vertex of
 * the stamp — that is what an executor performs, and what a device
 * replaces. The TOPOLOGY around them is integer: each point contributes
 * the stamp's own indices shifted by where its vertices begin. The
 * topology is the same wherever the vertices were formed, so it is
 * written once and never twice.
 *
 * The vertex arithmetic itself is written ONCE, in
 * `pop/kernels/Stamp.slang`, and the build compiles that source twice:
 * to C++, which the built-in executor calls, and to SPIR-V, which a
 * runtime that owns a device dispatches. Neither side re-derives a
 * formula, which is what lets two tiers be held to bit identity rather
 * than to a tolerance.
 *
 * Nothing here names a Cloud or a Mesh: a dispatch is plain lanes of
 * four floats, which is what a device binds. `Points.h` is where a cloud
 * and a stamp become one.
 */

#include <sigilcore/comparable/Erased.h>

#include <cstdint>
#include <glm/glm.hpp>
#include <span>
#include <string>
#include <vector>

namespace sigil::geometry::device {
class Device;
}  // namespace sigil::geometry::device

namespace sigil::geometry::mesh::points {

namespace kernel {

/** ONE STAMPING'S PARAMETERS, in the layout the kernel declares. */
struct Args {
  /** x: how many vertices one stamp has; y: how many points; z: flags;
   *  w: how many vertices in all. */
  glm::uvec4 code{0, 0, 0, 0};
  /** xyz the vector the orientation basis is seeded from. */
  glm::vec4 up{0, 1, 0, 0};
};

/** The flag bits `Args::code.z` carries. */
enum : uint32_t {
  /** A direction lane orients each stamp; without it every stamp keeps
   *  the axes it was authored on. */
  kOriented = 1u << 0u,
};

/** ONE STAMPING, READY TO RUN: the stamp's lanes and the points', each
 *  four floats wide because that is the stride a device binds.
 *
 *  A lane the cloud or the stamp does not carry is FILLED ONCE here with
 *  what it would have been read as — white for a tint, the identity
 *  window for a texture, one for a size — rather than asked about per
 *  vertex. What the mesh at the end does with the answer is a separate
 *  question: a stamp with no normals still has its normal lane formed,
 *  and the caller drops it. */
struct Dispatch {
  Args args;
  /** Per stamp vertex: xyz the position. */
  std::vector<glm::vec4> stampPosition;
  /** Per stamp vertex: xyz the normal. */
  std::vector<glm::vec4> stampNormal;
  /** Per stamp vertex: xy the uv. */
  std::vector<glm::vec4> stampUv;
  /** Per stamp vertex: the colour. */
  std::vector<glm::vec4> stampColor;
  /** Per point: xyz where it stands, w the size its stamp scales by. */
  std::vector<glm::vec4> pointOrigin;
  /** Per point: xyz the direction its stamp faces. */
  std::vector<glm::vec4> pointDir;
  /** Per point: the tint its stamp is coloured by. */
  std::vector<glm::vec4> pointColor;
  /** Per point: the texture window its stamp's uvs are remapped
   *  through. */
  std::vector<glm::vec4> pointTex;

  /** How many vertices a run of this writes into each output lane. */
  [[nodiscard]] size_t vertices() const { return (size_t)args.code.w; }
};

/** THE HOST RUN: the kernel's own generated C++ over @p dispatch. All
 *  three lanes address at least `dispatch.vertices()` values. A position
 *  carries u in its fourth float and a normal carries v in its — two
 *  lanes rather than three, because a uv is two numbers and both of
 *  those floats were spare. */
void run(const Dispatch& dispatch, glm::vec4* positions, glm::vec4* normals,
         glm::vec4* colors);

/** THE KERNEL AS A DEVICE RUNS IT: the SPIR-V this build compiled from
 *  the same source `run` came out of, with one `NoContraction`
 *  decoration per arithmetic result — see `pop/Spirv.h` for why that
 *  decoration is not optional. The words stand for the life of the
 *  process. */
std::span<const uint32_t> spirv();

/** How many vertices one dispatched group covers. It is the kernel's own
 *  `numthreads`, and the kernel drops the lanes past the vertex count
 *  itself, so a count that is not a multiple of it needs no second
 *  path. */
inline constexpr uint32_t kGroupSize = 64;

}  // namespace kernel

/** The one step a stamping runs through: the vertices. An implementation
 *  owns whatever device it needs; the dispatch carries none, so a
 *  description built for one executor runs on any of them. */
class StampExecutor {
 public:
  virtual ~StampExecutor() = default;

  /** What this executor is called, in a message that names it. */
  [[nodiscard]] virtual std::string name() const = 0;

  /** The vertices @p work describes, into three lanes each at least
   *  `work.vertices()` long, in point-major order: point 0's whole stamp
   *  first, then point 1's. */
  virtual void vertices(const kernel::Dispatch& work, glm::vec4* positions,
                        glm::vec4* normals, glm::vec4* colors) const = 0;
};

/** The executor a stamping's vertices are formed on, carried as a
 *  comparable value. */
class StampRuntime : public core::Erased<StampExecutor> {
 public:
  using core::Erased<StampExecutor>::Erased;
  StampRuntime() = default;
  StampRuntime(core::Erased<StampExecutor> erased)  // NOLINT: a Runtime IS
                                                    // its value
      : core::Erased<StampExecutor>(std::move(erased)) {}

  /** The built-in executor: the kernel's generated C++ over the
   *  vertices, on the host. Every call returns the same value, so two
   *  default option sets compare equal. */
  static StampRuntime cpu();
};

/**
 * THE DEVICE EXECUTOR, beside the CPU one: the `StampRuntime` that forms
 * a stamping's vertices on @p device.
 *
 * WHAT RUNS WHERE. The VERTICES — the per-vertex arithmetic, which is
 * the whole of what stamping computes — are one compute dispatch over
 * the stamp and the points, read back once at the end. The indices stay
 * on the host and are not a second piece of arithmetic: each point
 * contributes the stamp's own indices shifted by where its vertices
 * begin, which is integer.
 *
 * THE TWO TIERS ARE HELD TO BIT IDENTITY, not to a distance, for the
 * same reason the swept rings are: one piece of Slang compiled twice
 * under a float model pinned at both ends.
 *
 * Two runtimes made by one call to this compare equal; two separate
 * calls do not, because they hold separate device state. Defined only
 * where this library was built with a device feature.
 */
StampRuntime deviceRuntime(::sigil::geometry::device::Device& device);

}  // namespace sigil::geometry::mesh::points
