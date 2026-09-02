/** @file
 * The swept primitive: the profiles a sweep carries, the dispatch a rail
 * and a profile become, the built-in executor that runs the kernel's own
 * generated C++ over it, and the topology that turns ring vertices into
 * a Mesh.
 *
 * NOTHING HERE COMPUTES A RING VERTEX. That arithmetic is the kernel's,
 * the same source a device executor's SPIR-V came out of, so what is
 * written here is the packing, the quads, the caps and the geometric
 * averaging — none of which is arithmetic two backends could disagree
 * about.
 */

#include "sigilgeometry/mesh/pop/Sweep.h"

#include <sigilslang/Sweep.spv.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>

#include "sigilgeometry/mesh/pop/Spirv.h"

/** THE KERNEL ITSELF, as the build's C++ emitter names it. Its two
 *  opaque parameters are the group range and the global bindings, whose
 *  layouts are mirrored below. */
extern "C" void sigilSweepKernel(void* varying, void* entryPointParams,
                                 void* globalParams);

namespace sigil::geometry::mesh::curve {

namespace kernel {

namespace {

/** THE TWO PRELUDE TYPES the kernel's entry point takes.
 *
 *  The compiler's own C++ output declares these; they are mirrored here
 *  rather than reached for, because the generated file is a build
 *  artefact and a header of it is not one of its outputs. Both are fixed
 *  by the emitter: a group range as two triples, and a buffer as a
 *  pointer and a count. */
struct VaryingInput {
  uint32_t startGroup[3];
  uint32_t endGroup[3];
};

struct Buffer {
  void* data = nullptr;
  size_t count = 0;
};

/** The kernel's global parameters, member for member as it declares
 *  them: the argument block first, then one binding per lane. */
struct Globals {
  Args args;
  Buffer railPosition;
  Buffer railNormal;
  Buffer railBinormal;
  Buffer profile;
  Buffer outPosition;
  Buffer outNormal;
};

}  // namespace

void run(const Dispatch& dispatch, glm::vec4* positions, glm::vec4* normals) {
  const size_t count = dispatch.vertices();
  if (count == 0 || !positions || !normals) return;
  Globals globals;
  globals.args = dispatch.args;
  // The kernel only reads the rail and the profile, so they may be
  // handed over as they stand.
  const auto reading = [](const std::vector<glm::vec4>& values) {
    return Buffer{const_cast<glm::vec4*>(values.data()), values.size()};
  };
  globals.railPosition = reading(dispatch.railPosition);
  globals.railNormal = reading(dispatch.railNormal);
  globals.railBinormal = reading(dispatch.railBinormal);
  globals.profile = reading(dispatch.profile);
  globals.outPosition = {positions, count};
  globals.outNormal = {normals, count};

  VaryingInput varying{
      {0, 0, 0}, {(uint32_t)((count + kGroupSize - 1) / kGroupSize), 1, 1}};
  sigilSweepKernel(&varying, nullptr, &globals);
}

std::span<const uint32_t> spirv() {
  static const std::vector<uint32_t> module = noContraction(
      {slangmodule::Sweep::kSpirv, sizeof(slangmodule::Sweep::kSpirv) /
                                       sizeof(slangmodule::Sweep::kSpirv[0])});
  return {module.data(), module.size()};
}

}  // namespace kernel

namespace profile {

path::Polyline circle(int sides) {
  path::Polyline out;
  sides = std::max(sides, 3);
  out.points.reserve((size_t)sides + 1);
  // The seam point is emitted twice, at 0 and at a full turn, so the
  // swept ring's u reaches 1 rather than folding back to vertex zero.
  for (int s = 0; s <= sides; ++s) {
    const float a = (float)s / (float)sides * 2.0f * (float)M_PI;
    // y-down: -cos puts the first point on the frame's normal.
    out.points.emplace_back(std::sin(a), -std::cos(a));
  }
  return out;
}

path::Polyline line() {
  path::Polyline out;
  out.points = {{-0.5f, 0.0f}, {0.5f, 0.0f}};
  return out;
}

path::Polyline fromPath(const SkPath& outline, float tolerance) {
  const std::vector<path::Polyline> contours =
      path::flatten(outline, tolerance);
  if (contours.empty()) return {};
  return contours.front();
}

}  // namespace profile

namespace {

/** The built-in executor. It holds nothing, so every instance is the
 *  same value — which is what lets two default option sets compare
 *  equal. */
struct HostExecutor : SweepExecutor {
  // Stateless: every instance is the same value. (A defaulted
  // comparison cannot say so — the abstract base it derives from has
  // none.)
  bool operator==(const HostExecutor&) const { return true; }

  std::string name() const override { return "host"; }

  void rings(const kernel::Dispatch& work, glm::vec4* positions,
             glm::vec4* normals) const override {
    kernel::run(work, positions, normals);
  }
};

}  // namespace

SweepRuntime SweepRuntime::cpu() {
  static const SweepRuntime kCpu{HostExecutor{}};
  return kCpu;
}

bool describe(const std::vector<Frame3>& rail, const path::Polyline& profile,
              const SweepOptions& options, kernel::Dispatch* out) {
  if (!out) return false;
  const uint32_t ring = (uint32_t)profile.points.size();
  if (rail.size() < 2 || ring < 2) return false;

  kernel::Dispatch work;
  // A closed profile wraps back onto its first point; an open one ends,
  // so it spans one quad fewer and its u reaches 1.
  const uint32_t span = profile.closed ? ring : ring - 1;
  work.args.code = {(uint32_t)rail.size(), ring,
                    options.normals == SweepOptions::Normals::Radial ? 1u : 0u,
                    span};

  work.railPosition.reserve(rail.size());
  work.railNormal.reserve(rail.size());
  work.railBinormal.reserve(rail.size());
  for (const Frame3& f : rail) {
    // THE TAPER, EVALUATED HERE: it is an arbitrary function of t and
    // the only part of a ring's size a kernel could not answer.
    const float size =
        options.scale *
        (options.taper ? std::max(options.taper(f.t), 0.0f) : 1.0f);
    work.railPosition.emplace_back(f.position.x, f.position.y, f.position.z,
                                   size);
    work.railNormal.emplace_back(f.normal.x, f.normal.y, f.normal.z, f.t);
    work.railBinormal.emplace_back(f.binormal.x, f.binormal.y, f.binormal.z,
                                   0.0f);
  }
  work.profile.reserve(ring);
  for (const glm::vec2& p : profile.points)
    work.profile.emplace_back(p.x, p.y, 0.0f, 0.0f);

  *out = std::move(work);
  return true;
}

Mesh sweep(const std::vector<Frame3>& rail, const path::Polyline& profile,
           const SweepOptions& options) {
  Mesh out;
  kernel::Dispatch work;
  if (!describe(rail, profile, options, &work)) return out;

  const uint32_t ring = work.args.code.y;
  const uint32_t span = work.args.code.w;
  const auto next = [&](uint32_t k) {
    return profile.closed ? (k + 1) % ring : k + 1;
  };
  const bool geometric = options.normals == SweepOptions::Normals::Geometric;

  // THE RING VERTICES, on whichever executor the options carry. This is
  // the whole of what a device replaces; everything after it is integer
  // or a reduction over the triangles these vertices form.
  //
  // THE LANES ARE FOUR FLOATS WIDE, because that is the stride a device
  // binds, and a mesh's own lanes are three and two. Pouring them across
  // is what this seam costs, and it is the whole of what it costs — a
  // sweep formed here writes every vertex twice where a loop with no
  // seam under it wrote it once.
  const size_t vertices = work.vertices();
  std::vector<glm::vec4> positions(vertices);
  std::vector<glm::vec4> normals(vertices);
  const SweepRuntime& on =
      options.runtime ? options.runtime : SweepRuntime::cpu();
  on->rings(work, positions.data(), normals.data());

  out.positions.reserve(vertices);
  out.uvs.reserve(vertices);
  if (!geometric) out.normals.reserve(vertices);
  for (size_t i = 0; i < vertices; ++i) {
    out.positions.emplace_back(positions[i].x, positions[i].y, positions[i].z);
    // u rode the position's fourth float and v the normal's.
    out.uvs.emplace_back(positions[i].w, normals[i].w);
    if (!geometric)
      out.normals.emplace_back(normals[i].x, normals[i].y, normals[i].z);
  }

  const uint32_t rings = work.args.code.x;
  for (uint32_t i = 0; i + 1 < rings; ++i)
    for (uint32_t k = 0; k < span; ++k) {
      const uint32_t a = i * ring + k;
      const uint32_t b = i * ring + next(k);
      const uint32_t c = a + ring;
      const uint32_t d = b + ring;
      out.indices.insert(out.indices.end(), {a, b, d, a, d, c});
    }

  if (options.caps) {
    for (int end = 0; end < 2; ++end) {
      const Frame3& f = rail[end == 0 ? 0 : rail.size() - 1];
      const glm::vec3 n = end == 0 ? f.tangent * -1.0f : f.tangent;
      const uint32_t center = (uint32_t)out.positions.size();
      out.positions.push_back(f.position);
      if (!geometric) out.normals.push_back(n);
      out.uvs.emplace_back(0.5f, end == 0 ? 0.0f : 1.0f);
      const uint32_t ringStart = (end == 0 ? 0 : rings - 1) * ring;
      for (uint32_t k = 0; k < span; ++k) {
        const uint32_t a = ringStart + k;
        const uint32_t b = ringStart + next(k);
        if (end == 0)
          out.indices.insert(out.indices.end(), {center, b, a});
        else
          out.indices.insert(out.indices.end(), {center, a, b});
      }
    }
  }
  if (geometric) out.computeNormals();
  return out;
}

Mesh sweep(const Spline3& spline, const path::Polyline& profile,
           const SweepOptions& options) {
  SweepOptions railed = options;
  if (spline.closed) railed.caps = false;  // a loop has no ends to close
  return sweep(frames(spline, options.segments, options.up), profile, railed);
}

}  // namespace sigil::geometry::mesh::curve
