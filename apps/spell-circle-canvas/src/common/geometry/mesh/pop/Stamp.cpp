/** @file
 * The stamping operator: a cloud and a stamp packed into a dispatch, the
 * built-in executor that runs the kernel's own generated C++ over it,
 * and the topology that turns the formed vertices into a Mesh.
 *
 * NOTHING HERE COMPUTES A VERTEX. That arithmetic is the kernel's, the
 * same source a device executor's SPIR-V came out of, so what is written
 * here is the packing, the index runs, and which of the mesh's optional
 * lanes the result carries — none of which is arithmetic two backends
 * could disagree about.
 */

#include <sigilslang/Stamp.spv.h>

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "Parallel.h"
#include "sigilgeometry/mesh/pop/Points.h"
#include "sigilgeometry/mesh/pop/Spirv.h"

/** THE KERNEL ITSELF, as the build's C++ emitter names it. Its two
 *  opaque parameters are the group range and the global bindings, whose
 *  layouts are mirrored below. */
extern "C" void sigilStampKernel(void* varying, void* entryPointParams,
                                 void* globalParams);

namespace sigil::geometry::mesh::points {

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
  Buffer stampPosition;
  Buffer stampNormal;
  Buffer stampUv;
  Buffer stampColor;
  Buffer pointOrigin;
  Buffer pointDir;
  Buffer pointColor;
  Buffer pointTex;
  Buffer outPosition;
  Buffer outNormal;
  Buffer outColor;
};

}  // namespace

void run(const Dispatch& dispatch, glm::vec4* positions, glm::vec4* normals,
         glm::vec4* colors) {
  const size_t count = dispatch.vertices();
  if (count == 0 || !positions || !normals || !colors) return;
  Globals globals;
  globals.args = dispatch.args;
  // The kernel only reads the stamp and the points, so they may be
  // handed over as they stand.
  const auto reading = [](const std::vector<glm::vec4>& values) {
    return Buffer{const_cast<glm::vec4*>(values.data()), values.size()};
  };
  globals.stampPosition = reading(dispatch.stampPosition);
  globals.stampNormal = reading(dispatch.stampNormal);
  globals.stampUv = reading(dispatch.stampUv);
  globals.stampColor = reading(dispatch.stampColor);
  globals.pointOrigin = reading(dispatch.pointOrigin);
  globals.pointDir = reading(dispatch.pointDir);
  globals.pointColor = reading(dispatch.pointColor);
  globals.pointTex = reading(dispatch.pointTex);
  globals.outPosition = {positions, count};
  globals.outNormal = {normals, count};
  globals.outColor = {colors, count};

  const uint32_t groupCount = (uint32_t)((count + kGroupSize - 1) / kGroupSize);
  parallel::groups(groupCount, [&](uint32_t first, uint32_t last) {
    VaryingInput varying{{first, 0, 0}, {last, 1, 1}};
    sigilStampKernel(&varying, nullptr, &globals);
  });
}

std::span<const uint32_t> spirv() {
  static const std::vector<uint32_t> module = noContraction(
      {slangmodule::Stamp::kSpirv, sizeof(slangmodule::Stamp::kSpirv) /
                                       sizeof(slangmodule::Stamp::kSpirv[0])});
  return {module.data(), module.size()};
}

}  // namespace kernel

namespace {

/** The built-in executor. It holds nothing, so every instance is the
 *  same value — which is what lets two default option sets compare
 *  equal. */
struct HostExecutor : StampExecutor {
  // Stateless: every instance is the same value. (A defaulted
  // comparison cannot say so — the abstract base it derives from has
  // none.)
  bool operator==(const HostExecutor&) const { return true; }

  std::string name() const override { return "host"; }

  void vertices(const kernel::Dispatch& work, glm::vec4* positions,
                glm::vec4* normals, glm::vec4* colors) const override {
    kernel::run(work, positions, normals, colors);
  }
};

}  // namespace

StampRuntime StampRuntime::cpu() {
  static const StampRuntime kCpu{HostExecutor{}};
  return kCpu;
}

bool describe(const Cloud& cloud, const Mesh& stamp,
              const InstanceOptions& options, kernel::Dispatch* out) {
  if (!out) return false;
  const size_t points = cloud.size();
  const size_t verts = stamp.vertexCount();
  if (points == 0 || verts == 0) return false;

  const std::vector<float>* scaleLane =
      options.scaleLane.empty() ? nullptr : cloud.scalarIf(options.scaleLane);
  const std::vector<glm::vec4>* tintLane =
      options.tintLane.empty() ? nullptr : cloud.colorIf(options.tintLane);
  const std::vector<glm::vec3>* orientLane =
      options.orientLane.empty() ? nullptr : cloud.vectorIf(options.orientLane);
  const std::vector<glm::vec4>* texLane = cloud.colorIf("Tex");

  kernel::Dispatch work;
  work.args.code = {(uint32_t)verts, (uint32_t)points,
                    orientLane ? kernel::kOriented : 0u,
                    (uint32_t)(verts * points)};
  work.args.up = {options.up.x, options.up.y, options.up.z, 0};

  work.stampPosition.reserve(verts);
  work.stampNormal.reserve(verts);
  work.stampUv.reserve(verts);
  work.stampColor.reserve(verts);
  for (size_t v = 0; v < verts; ++v) {
    const glm::vec3& p = stamp.positions[v];
    work.stampPosition.emplace_back(p.x, p.y, p.z, 0.0f);
    const glm::vec3 n =
        v < stamp.normals.size() ? stamp.normals[v] : glm::vec3{0, 0, 0};
    work.stampNormal.emplace_back(n.x, n.y, n.z, 0.0f);
    const glm::vec2 uv = v < stamp.uvs.size() ? stamp.uvs[v] : glm::vec2{0, 0};
    work.stampUv.emplace_back(uv.x, uv.y, 0.0f, 0.0f);
    work.stampColor.push_back(v < stamp.colors.size() ? stamp.colors[v]
                                                      : glm::vec4{1, 1, 1, 1});
  }

  work.pointOrigin.reserve(points);
  work.pointDir.reserve(points);
  work.pointColor.reserve(points);
  work.pointTex.reserve(points);
  for (size_t i = 0; i < points; ++i) {
    const glm::vec3& p = cloud.positions[i];
    const float scale =
        options.scale *
        (scaleLane && i < scaleLane->size() ? (*scaleLane)[i] : 1.0f);
    work.pointOrigin.emplace_back(p.x, p.y, p.z, scale);
    const glm::vec3 dir = orientLane && i < orientLane->size()
                              ? (*orientLane)[i]
                              : glm::vec3{0, 0, 1};
    work.pointDir.emplace_back(dir.x, dir.y, dir.z, 0.0f);
    work.pointColor.push_back(tintLane && i < tintLane->size()
                                  ? (*tintLane)[i]
                                  : glm::vec4{1, 1, 1, 1});
    // The texture window is the identity where the cloud carries none,
    // so the kernel remaps unconditionally and no branch decides it.
    work.pointTex.push_back(
        texLane && i < texLane->size() ? (*texLane)[i] : glm::vec4{0, 0, 1, 1});
  }

  *out = std::move(work);
  return true;
}

Mesh instance(const Cloud& cloud, const Mesh& stamp,
              const InstanceOptions& options) {
  Mesh out;
  kernel::Dispatch work;
  if (!describe(cloud, stamp, options, &work)) return out;

  // THE VERTICES, on whichever executor the options carry. This is the
  // whole of what a device replaces; everything after it is integer.
  //
  // THE LANES ARE FOUR FLOATS WIDE, because that is the stride a device
  // binds, and a mesh's own lanes are three and two. Pouring them across
  // is what this seam costs, and it is the whole of what it costs.
  const size_t vertices = work.vertices();
  std::vector<glm::vec4> positions(vertices);
  std::vector<glm::vec4> normals(vertices);
  std::vector<glm::vec4> colors(vertices);
  const StampRuntime& on =
      options.runtime ? options.runtime : StampRuntime::cpu();
  on->vertices(work, positions.data(), normals.data(), colors.data());

  // WHICH OPTIONAL LANES THE RESULT CARRIES is the stamp's answer and
  // not the kernel's: a lane is present on a mesh when it is sized to
  // the positions, and consumers read that as the presence bit. A stamp
  // with no normals therefore forms none, however the kernel filled its
  // own.
  const size_t stampVerts = stamp.vertexCount();
  const bool normalled = stamp.normals.size() == stampVerts;
  const bool textured = stamp.uvs.size() == stampVerts;
  const bool tinted = !options.tintLane.empty() || !stamp.colors.empty();

  out.positions.reserve(vertices);
  if (normalled) out.normals.reserve(vertices);
  if (textured) out.uvs.reserve(vertices);
  if (tinted) out.colors.reserve(vertices);
  for (size_t i = 0; i < vertices; ++i) {
    out.positions.emplace_back(positions[i].x, positions[i].y, positions[i].z);
    if (normalled)
      out.normals.emplace_back(normals[i].x, normals[i].y, normals[i].z);
    // u rode the position's fourth float and v the normal's.
    if (textured) out.uvs.emplace_back(positions[i].w, normals[i].w);
    if (tinted) out.colors.push_back(colors[i]);
  }

  out.indices.reserve(cloud.size() * stamp.indices.size());
  for (size_t i = 0; i < cloud.size(); ++i) {
    const uint32_t base = (uint32_t)(i * stampVerts);
    for (uint32_t idx : stamp.indices) out.indices.push_back(base + idx);
  }
  return out;
}

Mesh quads(const Cloud& cloud, float width, float height,
           const InstanceOptions& options) {
  return instance(cloud, mesh::quad(width, height), options);
}

}  // namespace sigil::geometry::mesh::points
