/** @file
 * The Alembic reader: Ogawa archives read through the Alembic library
 * from bytes the caller owns, the hierarchy walked depth-first with the
 * transform stack carried along, every polygon mesh and every point
 * set at the nearest stored sample becoming a Part with its
 * arbGeomParams as named lanes.
 */

#include <Alembic/Abc/All.h>
#include <Alembic/AbcCoreFactory/All.h>
#include <Alembic/AbcCoreOgawa/All.h>
#include <Alembic/AbcGeom/All.h>

#include <cstring>
#include <glm/gtc/type_ptr.hpp>
#include <istream>
#include <streambuf>
#include <string>

#include "Internal.h"
#include "sigilgeometry/codec/Decode.h"

namespace sigil::geometry::import {

using detail::finishPart;

namespace {

// --- Alembic (Ogawa) ------------------------------------------------------
// The vfx cache format, read through the Alembic library — Ogawa core
// only (the optional HDF5 backend stays out; legacy archives come back
// nullopt). Every IPolyMesh in the hierarchy becomes a Part with its
// xform stack baked (the glTF posture); every IPoints a faceless Part
// (the PLY point-cloud posture). arbGeomParams land as named lanes.
// Sampling takes the NEAREST stored sample — no interpolation.

namespace Abc = Alembic::Abc;
namespace AbcA = Alembic::AbcCoreAbstract;
namespace AbcGeom = Alembic::AbcGeom;

/** A read-only streambuf over bytes the CALLER owns — no copy. Ogawa
 *  reads its archive lazily and by SEEK (a header, then jumps to group
 *  offsets), so an istream is the only shape the factory accepts; what
 *  it does not need is a second copy of the whole cache, which
 *  istringstream forces and which doubles peak memory on the caches
 *  this format exists for.
 *
 *  The get area IS the caller's buffer, so the only override that has
 *  to do real work is the seek pair (std::streambuf's own seekoff
 *  fails); sgetc/sbumpc/sgetn already read straight out of it.
 *
 *  LIFETIME: the buffer, the stream over it, and the caller's bytes all
 *  have to outlive the IArchive, because the archive keeps reading
 *  after getArchive() returns. Enforced by declaration ORDER in the one
 *  function that builds them — buf, then stream, then archive, all in
 *  the same scope, so destruction unwinds archive first. Do not hoist
 *  the archive out of that scope or hand it back to a caller. */
class NonOwningStreamBuf final : public std::streambuf {
 public:
  NonOwningStreamBuf(const void* bytes, size_t size) {
    // setg's non-const signature is a formality: nothing here writes,
    // and no overflow/pbackfail path can reach the pointers.
    char* begin = const_cast<char*>(static_cast<const char*>(bytes));
    setg(begin, begin, begin + size);
  }

 protected:
  std::streamsize showmanyc() override { return egptr() - gptr(); }

  pos_type seekoff(off_type off, std::ios_base::seekdir dir,
                   std::ios_base::openmode which) override {
    if (!(which & std::ios_base::in)) return pos_type(off_type(-1));
    const off_type size = egptr() - eback();
    off_type target = off;
    if (dir == std::ios_base::cur)
      target += gptr() - eback();
    else if (dir == std::ios_base::end)
      target += size;
    // A seek PAST the end is a failure, not a clamp: Ogawa uses a
    // short read to detect truncation, and clamping would hand it a
    // valid-looking position into a file that has no such bytes.
    if (target < 0 || target > size) return pos_type(off_type(-1));
    setg(eback(), eback() + target, egptr());
    return pos_type(target);
  }

  pos_type seekpos(pos_type pos, std::ios_base::openmode which) override {
    return seekoff(off_type(pos), std::ios_base::beg, which);
  }
};

/** XformSample matrices are row-vector M44d; glm's mat4 is column-
 *  vector. Reading the 16 doubles in memory order through make_mat4
 *  IS the row->column conversion — no explicit transpose — so the
 *  walk composes world * local like any glm stack. */
glm::mat4 toGlm(const Abc::M44d& m) {
  return glm::mat4(glm::make_mat4(&m[0][0]));
}

/** Where an output vertex came from — the indices the geom-param
 *  scopes select by. */
struct AbcVertexSource {
  int32_t point = 0;   ///< index into the sample's positions
  int32_t corner = 0;  ///< index into faceIndices, FILE order
  int32_t face = 0;    ///< the face the vertex was first seen on
};

/** One part's sampling domain: a source per output vertex plus the
 *  counts each scope validates against. corners == 0 marks a faceless
 *  cloud — only constant and per-point scopes apply there. */
struct AbcDomain {
  std::vector<AbcVertexSource> sources;
  size_t points = 0;
  size_t corners = 0;
  size_t faces = 0;
};

/** Expand one geom param and hand each output vertex its value; the
 *  scope picks the source index — 0 for constant, the face for
 *  uniform, the point for varying/vertex, the corner for facevarying.
 *  Deliberate simplification: uniform values broadcast per-face
 *  THROUGH the dedup, so a vertex shared across faces keeps its first
 *  face's value. False when the scope cannot apply or the array runs
 *  short — the caller drops the lane rather than half-fill it. */
template <typename Param, typename Sink>
bool unpackAbcParam(const Param& param, const Abc::ISampleSelector& at,
                    const AbcDomain& domain, Sink&& sink) {
  typename Param::Sample sample = param.getExpandedValue(at);
  const auto values = sample.getVals();
  if (!values) return false;
  const AbcGeom::GeometryScope scope = param.getScope();
  const bool faceless = domain.corners == 0;
  size_t need = 0;
  switch (scope) {
    case AbcGeom::kConstantScope:
      need = 1;
      break;
    case AbcGeom::kUniformScope:
      if (faceless) return false;
      need = domain.faces;
      break;
    case AbcGeom::kVaryingScope:
    case AbcGeom::kVertexScope:
      need = domain.points;
      break;
    case AbcGeom::kFacevaryingScope:
      if (faceless) return false;
      need = domain.corners;
      break;
    default:
      return false;
  }
  if (need == 0 || values->size() < need) return false;
  for (size_t i = 0; i < domain.sources.size(); ++i) {
    const AbcVertexSource& v = domain.sources[i];
    const size_t index = scope == AbcGeom::kConstantScope  ? 0
                         : scope == AbcGeom::kUniformScope ? (size_t)v.face
                         : scope == AbcGeom::kFacevaryingScope
                             ? (size_t)v.corner
                             : (size_t)v.point;
    sink(i, (*values)[index]);
  }
  return true;
}

/** arbGeomParams into named lanes, routed by width like glTF/PLY:
 *  1 -> scalars (ints cast raw — ids stay ids), 3 -> vectors, 2 and
 *  4 -> colors (vec4; V2f zero-pads, C3f gets alpha 1). Names arrive
 *  verbatim — no Houdini renaming. */
void importAbcLanes(const Abc::ICompoundProperty& params,
                    const Abc::ISampleSelector& at, const AbcDomain& domain,
                    Part& part) {
  if (!params.valid()) return;
  const size_t n = domain.sources.size();
  for (size_t i = 0; i < params.getNumProperties(); ++i) {
    const AbcA::PropertyHeader& header = params.getPropertyHeader(i);
    const std::string& name = header.getName();
    const auto scalars = [&](const auto& param) {
      std::vector<float> lane(n, 0.0f);
      if (unpackAbcParam(param, at, domain, [&lane](size_t v, auto value) {
            lane[v] = (float)value;  // raw — ids stay ids
          }))
        part.scalarLanes[name] = std::move(lane);
    };
    const auto vectors = [&](const auto& param) {
      std::vector<glm::vec3> lane(n, glm::vec3{0});
      if (unpackAbcParam(param, at, domain,
                         [&lane](size_t v, const Imath::V3f& value) {
                           lane[v] = {value.x, value.y, value.z};
                         }))
        part.vectorLanes[name] = std::move(lane);
    };
    if (AbcGeom::IFloatGeomParam::matches(header))
      scalars(AbcGeom::IFloatGeomParam(params, name));
    else if (AbcGeom::IDoubleGeomParam::matches(header))
      scalars(AbcGeom::IDoubleGeomParam(params, name));
    else if (AbcGeom::IInt32GeomParam::matches(header))
      scalars(AbcGeom::IInt32GeomParam(params, name));
    else if (AbcGeom::IUInt32GeomParam::matches(header))
      scalars(AbcGeom::IUInt32GeomParam(params, name));
    else if (AbcGeom::IV3fGeomParam::matches(header))
      vectors(AbcGeom::IV3fGeomParam(params, name));
    else if (AbcGeom::IP3fGeomParam::matches(header))
      vectors(AbcGeom::IP3fGeomParam(params, name));
    else if (AbcGeom::IN3fGeomParam::matches(header))
      vectors(AbcGeom::IN3fGeomParam(params, name));
    else if (AbcGeom::IC3fGeomParam::matches(header)) {
      std::vector<glm::vec4> lane(n, glm::vec4{0});
      if (unpackAbcParam(AbcGeom::IC3fGeomParam(params, name), at, domain,
                         [&lane](size_t v, const Imath::C3f& value) {
                           lane[v] = {value.x, value.y, value.z, 1};
                         }))
        part.colorLanes[name] = std::move(lane);
    } else if (AbcGeom::IC4fGeomParam::matches(header)) {
      std::vector<glm::vec4> lane(n, glm::vec4{0});
      if (unpackAbcParam(AbcGeom::IC4fGeomParam(params, name), at, domain,
                         [&lane](size_t v, const Imath::C4f& value) {
                           lane[v] = {value.r, value.g, value.b, value.a};
                         }))
        part.colorLanes[name] = std::move(lane);
    } else if (AbcGeom::IV2fGeomParam::matches(header)) {
      // Width 2 rides the color lane zero-padded — the routing rule.
      std::vector<glm::vec4> lane(n, glm::vec4{0});
      if (unpackAbcParam(AbcGeom::IV2fGeomParam(params, name), at, domain,
                         [&lane](size_t v, const Imath::V2f& value) {
                           lane[v] = {value.x, value.y, 0, 0};
                         }))
        part.colorLanes[name] = std::move(lane);
    }
  }
}

/** One IPolyMesh at one time into a Part: n-gons fan-triangulated
 *  after reversing each face's corners (Alembic winds CLOCKWISE, Mesh
 *  is CCW), vertices deduplicated OBJ-style on (point, uv, normal)
 *  sources. Deliberate simplification: custom facevarying lanes ride
 *  that same key, so corners agreeing on point+uv+normal share one
 *  lane slot. */
void importAbcMesh(AbcGeom::IPolyMesh object, const glm::mat4& world,
                   const Abc::ISampleSelector& at, Model& out) {
  AbcGeom::IPolyMeshSchema& schema = object.getSchema();
  AbcGeom::IPolyMeshSchema::Sample sample;
  schema.get(sample, at);
  const Abc::P3fArraySamplePtr positions = sample.getPositions();
  const Abc::Int32ArraySamplePtr faceIndices = sample.getFaceIndices();
  const Abc::Int32ArraySamplePtr faceCounts = sample.getFaceCounts();
  // The quiet policy hands back empty samples, never throws.
  if (!positions || !faceIndices || !faceCounts) return;
  const size_t numPoints = positions->size();
  const size_t numCorners = faceIndices->size();

  // UV/normal sources: -1 absent, else per-point or per-corner by the
  // declared scope (kVertex/kVarying carry one value per POINT here).
  const auto classify = [&](AbcGeom::GeometryScope scope, size_t count) {
    if ((scope == AbcGeom::kVertexScope || scope == AbcGeom::kVaryingScope) &&
        count >= numPoints)
      return 0;
    if (scope == AbcGeom::kFacevaryingScope && count >= numCorners) return 1;
    return -1;
  };
  Abc::V2fArraySamplePtr uvs;
  int uvMode = -1;
  if (AbcGeom::IV2fGeomParam param = schema.getUVsParam(); param.valid()) {
    uvs = param.getExpandedValue(at).getVals();
    if (uvs) uvMode = classify(param.getScope(), uvs->size());
  }
  Abc::N3fArraySamplePtr normals;
  int normalMode = -1;
  if (AbcGeom::IN3fGeomParam param = schema.getNormalsParam(); param.valid()) {
    normals = param.getExpandedValue(at).getVals();
    if (normals) normalMode = classify(param.getScope(), normals->size());
  }

  Part part;
  part.name = object.getName();
  Mesh& mesh = part.mesh;
  AbcDomain domain;
  domain.points = numPoints;
  domain.corners = numCorners;
  domain.faces = faceCounts->size();

  std::map<std::array<int32_t, 3>, uint32_t> seen;
  std::vector<uint32_t> ring;
  size_t cursor = 0;
  for (size_t face = 0; face < faceCounts->size(); ++face) {
    const int32_t count = (*faceCounts)[face];
    if (count < 0 || cursor + (size_t)count > numCorners)
      return;  // inconsistent topology — drop the whole part
    if (count < 3) {
      cursor += (size_t)count;
      continue;
    }
    ring.clear();
    for (int32_t j = 0; j < count; ++j) {
      // Reverse the face's corner list for the CW -> CCW flip;
      // `corner` keeps the FILE's index — facevarying arrays are
      // addressed by it.
      const size_t corner = cursor + (size_t)(count - 1 - j);
      const int32_t point = (*faceIndices)[corner];
      if (point < 0 || (size_t)point >= numPoints) return;
      const int32_t uvSource = uvMode < 0    ? -1
                               : uvMode == 0 ? point
                                             : (int32_t)corner;
      const int32_t normalSource = normalMode < 0    ? -1
                                   : normalMode == 0 ? point
                                                     : (int32_t)corner;
      const std::array<int32_t, 3> key = {point, uvSource, normalSource};
      auto [it, inserted] = seen.emplace(key, (uint32_t)mesh.positions.size());
      if (inserted) {
        const Imath::V3f& p = (*positions)[point];
        mesh.positions.push_back({p.x, p.y, p.z});
        if (normalSource >= 0) {
          const Imath::V3f& nrm = (*normals)[normalSource];
          mesh.normals.push_back({nrm.x, nrm.y, nrm.z});
        }
        if (uvSource >= 0) {
          const Imath::V2f& uv = (*uvs)[uvSource];
          // Alembic uv origin is bottom-left; Mesh is IMAGE
          // convention (top-left), so v flips — the OBJ rule.
          mesh.uvs.push_back({uv.x, 1 - uv.y});
        } else {
          mesh.uvs.push_back({0, 0});
        }
        domain.sources.push_back({point, (int32_t)corner, (int32_t)face});
      }
      ring.push_back(it->second);
    }
    for (size_t j = 1; j + 1 < ring.size(); ++j)
      mesh.indices.insert(mesh.indices.end(), {ring[0], ring[j], ring[j + 1]});
    cursor += (size_t)count;
  }

  // Conventional member: velocities land verbatim as a lane.
  if (const Abc::V3fArraySamplePtr velocities = sample.getVelocities();
      velocities && velocities->size() >= numPoints) {
    std::vector<glm::vec3>& lane = part.vectorLanes["velocity"];
    lane.resize(domain.sources.size());
    for (size_t i = 0; i < domain.sources.size(); ++i) {
      const Imath::V3f& v = (*velocities)[domain.sources[i].point];
      lane[i] = {v.x, v.y, v.z};
    }
  }
  importAbcLanes(schema.getArbGeomParams(), at, domain, part);

  finishPart(part, normalMode >= 0);
  part.mesh.transform(world);  // bake AFTER finishing — lanes verbatim
  if (!part.mesh.indices.empty()) out.parts.push_back(std::move(part));
}

/** One IPoints at one time into a faceless Part — the PLY point-cloud
 *  posture: empty indices, asCloud() the natural consumer. ids and
 *  widths keep their file names ("id" casts uint64 to float, exact
 *  only to 2^24 — a stated trade); velocities land as "velocity". */
void importAbcPoints(AbcGeom::IPoints object, const glm::mat4& world,
                     const Abc::ISampleSelector& at, Model& out) {
  AbcGeom::IPointsSchema& schema = object.getSchema();
  AbcGeom::IPointsSchema::Sample sample;
  schema.get(sample, at);
  const Abc::P3fArraySamplePtr positions = sample.getPositions();
  if (!positions || positions->size() == 0) return;
  const size_t n = positions->size();

  Part part;
  part.name = object.getName();
  Mesh& mesh = part.mesh;
  mesh.positions.resize(n);
  AbcDomain domain;
  domain.points = n;  // corners/faces stay 0: faceless
  domain.sources.resize(n);
  for (size_t i = 0; i < n; ++i) {
    const Imath::V3f& p = (*positions)[i];
    mesh.positions[i] = {p.x, p.y, p.z};
    domain.sources[i].point = (int32_t)i;
  }

  if (const Abc::UInt64ArraySamplePtr ids = sample.getIds();
      ids && ids->size() >= n) {
    std::vector<float>& lane = part.scalarLanes["id"];
    lane.resize(n);
    for (size_t i = 0; i < n; ++i) lane[i] = (float)(*ids)[i];
  }
  if (const Abc::V3fArraySamplePtr velocities = sample.getVelocities();
      velocities && velocities->size() >= n) {
    std::vector<glm::vec3>& lane = part.vectorLanes["velocity"];
    lane.resize(n);
    for (size_t i = 0; i < n; ++i) {
      const Imath::V3f& v = (*velocities)[i];
      lane[i] = {v.x, v.y, v.z};
    }
  }
  if (AbcGeom::IFloatGeomParam widths = schema.getWidthsParam();
      widths.valid()) {
    std::vector<float> lane(n, 0.0f);
    if (unpackAbcParam(widths, at, domain,
                       [&lane](size_t v, float value) { lane[v] = value; }))
      part.scalarLanes["width"] = std::move(lane);
  }
  importAbcLanes(schema.getArbGeomParams(), at, domain, part);

  finishPart(part, false);  // faceless: same posture as importPly
  part.mesh.transform(world);
  out.parts.push_back(std::move(part));
}

/** Depth-first over the hierarchy, xform stack carried as glm and
 *  baked at the leaves — the glTF posture. Honors each xform's
 *  inherits flag; unknown object types recurse transparently. */
void walkAlembic(const Abc::IObject& parent, const glm::mat4& world,
                 const Abc::ISampleSelector& at, Model& out) {
  for (size_t i = 0; i < parent.getNumChildren(); ++i) {
    const AbcA::ObjectHeader& header = parent.getChildHeader(i);
    if (AbcGeom::IXform::matches(header)) {
      AbcGeom::IXform xform(parent, header.getName());
      AbcGeom::XformSample xs = xform.getSchema().getValue(at);
      const glm::mat4 local = toGlm(xs.getMatrix());
      walkAlembic(xform, xs.getInheritsXforms() ? world * local : local, at,
                  out);
    } else if (AbcGeom::IPolyMesh::matches(header)) {
      importAbcMesh(AbcGeom::IPolyMesh(parent, header.getName()), world, at,
                    out);
    } else if (AbcGeom::IPoints::matches(header)) {
      importAbcPoints(AbcGeom::IPoints(parent, header.getName()), world, at,
                      out);
    } else {
      walkAlembic(Abc::IObject(parent, header.getName()), world, at, out);
    }
  }
}

}  // namespace

std::optional<Model> alembic(const void* bytes, size_t size,
                             const AlembicOptions& options) {
  if (!bytes || size == 0) return std::nullopt;
  // The library can still throw on malformed input despite the quiet
  // policy below — one net turns every failure into nullopt.
  try {
    // The buffer and the stream must outlive the archive; all three
    // stay stack-local IN THIS ORDER, so unwinding destroys the archive
    // first, and the archive is never returned or stored. The buffer is
    // non-owning — the caller's bytes are read in place, never copied.
    NonOwningStreamBuf buf(bytes, size);
    std::istream stream(&buf);
    Alembic::AbcCoreFactory::IFactory factory;
    factory.setPolicy(Alembic::Abc::ErrorHandler::kQuietNoopPolicy);
    Alembic::AbcCoreFactory::IFactory::CoreType core =
        Alembic::AbcCoreFactory::IFactory::kUnknown;
    std::vector<std::istream*> streams = {&stream};
    Alembic::Abc::IArchive archive = factory.getArchive(streams, core);
    // HDF5-cored archives need the backend we deliberately don't
    // build — they land here as kUnknown/invalid, honest nullopt.
    if (!archive.valid()) return std::nullopt;
    const Abc::ISampleSelector at(options.time,
                                  Abc::ISampleSelector::kNearIndex);
    Model out;
    walkAlembic(archive.getTop(), glm::mat4(1.0f), at, out);
    if (out.parts.empty()) return std::nullopt;
    return out;
  } catch (...) {
    return std::nullopt;
  }
}

}  // namespace sigil::geometry::import
