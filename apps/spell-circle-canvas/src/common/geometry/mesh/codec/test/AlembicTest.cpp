/** @file
 * The Alembic reader, over archives this test writes in memory with the
 * O* API: a mesh and a point cloud with their lanes, the face-varying uvs
 * that must be flipped and deduplicated, and which time sample a read
 * selects.
 */

#include <Alembic/Abc/All.h>
#include <Alembic/AbcCoreOgawa/All.h>
#include <Alembic/AbcGeom/All.h>
#include <gtest/gtest.h>

#include <cmath>
#include <string>
#include <vector>

#include "sigilgeometry/mesh/Mesh.h"
#include "sigilgeometry/mesh/codec/Decode.h"

using namespace sigil::geometry;
using namespace sigil::geometry::mesh;

using codec::decode::Model;
using codec::decode::Part;

namespace {

/** An Ogawa archive built entirely in memory, so the Alembic tests need no
 *  fixture files. It contains:
 *   - "root", an xform translated +10 along x, holding "tri": a static
 *     clockwise triangle carrying a per-vertex-scope arbitrary geometry
 *     parameter;
 *   - "uvquad" and "uvweld": the SAME topology written twice, once with
 *     facevarying uvs and once with vertex-scope uvs, which are the two
 *     sides of the corner dedup;
 *   - "cloud": a top-level point cloud with two time samples at 24 fps, and
 *     ids written on the first sample only. */
std::string alembicArchiveBytes() {
  namespace Abc = Alembic::Abc;
  namespace AbcGeom = Alembic::AbcGeom;
  std::ostringstream out(std::ios::binary);
  {
    Abc::OArchive archive(
        Alembic::AbcCoreOgawa::WriteArchive()(&out, Abc::MetaData()));
    const uint32_t ts = archive.addTimeSampling(
        Alembic::AbcCoreAbstract::TimeSampling(1.0 / 24.0, 0.0));

    AbcGeom::OXform root(archive.getTop(), "root");
    AbcGeom::XformSample xs;
    xs.setTranslation(Abc::V3d(10, 0, 0));
    root.getSchema().set(xs);

    AbcGeom::OPolyMesh meshObj(root, "tri");  // static, under the xform
    const std::vector<Imath::V3f> pos = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
    const std::vector<int32_t> indices = {0, 2, 1};  // Alembic: clockwise
    const std::vector<int32_t> counts = {3};
    meshObj.getSchema().set(AbcGeom::OPolyMeshSchema::Sample(
        Abc::P3fArraySample(pos), Abc::Int32ArraySample(indices),
        Abc::Int32ArraySample(counts)));
    AbcGeom::OFloatGeomParam energy(meshObj.getSchema().getArbGeomParams(),
                                    "energy", false, AbcGeom::kVertexScope, 1);
    const std::vector<float> values = {0.25f, 0.5f, 0.75f};
    energy.set(AbcGeom::OFloatGeomParam::Sample(Abc::FloatArraySample(values),
                                                AbcGeom::kVertexScope));

    // Two triangles sharing the 0-2 diagonal of a unit square: 4 points and
    // 6 corners, written twice with identical topology and different uv
    // scopes so the dedup can be observed from both sides.
    const std::vector<Imath::V3f> qpos = {
        {0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}};
    const std::vector<int32_t> qidx = {0, 2, 1, 0, 3, 2};  // clockwise
    const std::vector<int32_t> qcounts = {3, 3};
    // FACEVARYING: one uv per corner. Corners 1 and 5 are the same
    // point with different uvs; corners 0 and 3 are the same point with
    // the SAME uv — and still do not weld, because the key is the
    // corner INDEX, not the value.
    AbcGeom::OPolyMesh uvObj(archive.getTop(), "uvquad");
    const std::vector<Imath::V2f> quv = {{0, 0}, {1, 1}, {1, 0},
                                         {0, 0}, {0, 1}, {0.25f, 0.75f}};
    uvObj.getSchema().set(AbcGeom::OPolyMeshSchema::Sample(
        Abc::P3fArraySample(qpos), Abc::Int32ArraySample(qidx),
        Abc::Int32ArraySample(qcounts),
        AbcGeom::OV2fGeomParam::Sample(Abc::V2fArraySample(quv),
                                       AbcGeom::kFacevaryingScope)));
    // VERTEX scope: one uv per point, so every corner of a point keys
    // the same and the six corners weld back down to four vertices.
    AbcGeom::OPolyMesh weldObj(archive.getTop(), "uvweld");
    const std::vector<Imath::V2f> wuv = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
    weldObj.getSchema().set(AbcGeom::OPolyMeshSchema::Sample(
        Abc::P3fArraySample(qpos), Abc::Int32ArraySample(qidx),
        Abc::Int32ArraySample(qcounts),
        AbcGeom::OV2fGeomParam::Sample(Abc::V2fArraySample(wuv),
                                       AbcGeom::kVertexScope)));

    AbcGeom::OPoints pointsObj(archive.getTop(), "cloud", ts);  // animated
    const std::vector<uint64_t> ids = {0, 1};
    const std::vector<Imath::V3f> frame0 = {{0, 0, 0}, {1, 0, 0}};
    pointsObj.getSchema().set(AbcGeom::OPointsSchema::Sample(
        Abc::P3fArraySample(frame0), Abc::UInt64ArraySample(ids)));
    const std::vector<Imath::V3f> frame1 = {{0, 1, 0}, {1, 1, 0}};
    pointsObj.getSchema().set(
        AbcGeom::OPointsSchema::Sample(Abc::P3fArraySample(frame1)));
  }  // The OArchive destructor is what finalizes the Ogawa stream, so the
     // scope must close before .str() is read — an earlier read returns a
     // truncated, unreadable archive.
  return std::move(out).str();
}

}  // namespace

TEST(ReadAlembic, AMeshAndACloudArriveWithTheirLanes) {
  const std::string bytes = alembicArchiveBytes();

  // As with GLB, the filename hint carries nothing useful here and the
  // leading Ogawa magic is what routes the bytes to the Alembic reader.
  auto model = codec::decode::model(bytes.data(), bytes.size(), "download");
  ASSERT_TRUE(model.has_value());
  ASSERT_EQ(model->parts.size(), 4u);  // tri, uvquad, uvweld, cloud
  const auto find = [&](std::string_view name) -> const Part* {
    for (const Part& part : model->parts)
      if (part.name == name) return &part;
    return nullptr;
  };
  const Part* tri = find("tri");
  const Part* cloud = find("cloud");
  ASSERT_NE(tri, nullptr);
  ASSERT_NE(cloud, nullptr);

  EXPECT_EQ(tri->mesh.triangleCount(), 1u);
  glm::vec3 lo, hi;
  tri->mesh.bounds(&lo, &hi);
  // Parent xforms are baked into the positions, as glTF node transforms are.
  EXPECT_FLOAT_EQ(lo.x, 10.0f);
  EXPECT_FLOAT_EQ(hi.x, 11.0f);
  // WINDING: Alembic faces are wound clockwise, this library's meshes
  // counter-clockwise, so the importer reverses each face. The fixture's
  // triangle therefore ends up with its derived normal along +z; leaving
  // the file's order alone would point it at -z and cull the whole model.
  ASSERT_EQ(tri->mesh.normals.size(), 3u);
  EXPECT_GT(tri->mesh.normals[0].z, 0.0f);
  // A vertex-scope arbitrary geometry parameter becomes a per-point scalar
  // lane, and each value has to follow its point through the corner dedup
  // and the winding reversal — both of which renumber vertices. Checked by
  // recomputing the value from the position it ended up on (the fixture's
  // values are 0.25 + 0.25x + 0.5y in local space) rather than by index,
  // since matching by index would pass even if the lane were shuffled.
  const auto energy = tri->scalarLanes.find("energy");
  ASSERT_NE(energy, tri->scalarLanes.end());
  ASSERT_EQ(energy->second.size(), 3u);
  for (size_t i = 0; i < 3; ++i) {
    const glm::vec3 local = tri->mesh.positions[i] - glm::vec3{10, 0, 0};
    EXPECT_NEAR(energy->second[i], 0.25f + 0.25f * local.x + 0.5f * local.y,
                1e-6f);
  }
  EXPECT_EQ(tri->asCloud().scalars.count("energy"), 1u);

  // An OPoints object imports as a part with no indices, its ids carried as
  // an ordinary scalar lane. With no time requested, the first sample is
  // the one read.
  EXPECT_TRUE(cloud->mesh.indices.empty());
  ASSERT_EQ(cloud->mesh.positions.size(), 2u);
  EXPECT_FLOAT_EQ(cloud->mesh.positions[0].y, 0.0f);
  const auto id = cloud->scalarLanes.find("id");
  ASSERT_NE(id, cloud->scalarLanes.end());
  ASSERT_EQ(id->second.size(), 2u);
  EXPECT_FLOAT_EQ(id->second[0], 0.0f);
  EXPECT_FLOAT_EQ(id->second[1], 1.0f);

  // Malformed input returns an empty optional rather than throwing or
  // aborting: the Alembic library signals errors by exception, and those
  // must not escape into a caller that is merely opening an untrusted file.
  const char garbage[] = "not an alembic archive at all";
  EXPECT_FALSE(codec::decode::alembic(garbage, sizeof(garbage)).has_value());
  EXPECT_FALSE(
      codec::decode::alembic(bytes.data(), bytes.size() / 2).has_value());
}

TEST(ReadAlembic, FaceVaryingUvsAreFlippedAndDeduplicated) {
  // Two conventions, one fixture.
  //
  // (1) UV ORIGIN: Alembic's is BOTTOM-left, this library's Mesh uses
  // top-left, so v is flipped on import. Get it wrong and every textured
  // Alembic model is upside down.
  //
  // (2) DEDUP: corners merge OBJ-style on (position, uv, normal) — corners
  // that agree on all three become one vertex, and a disagreement in uv
  // splits one point into two vertices so each can keep its own uv.
  const std::string bytes = alembicArchiveBytes();
  auto model = codec::decode::alembic(bytes.data(), bytes.size());
  ASSERT_TRUE(model.has_value());
  const auto part = [&](std::string_view name) -> const Part* {
    for (const Part& p : model->parts)
      if (p.name == name) return &p;
    return nullptr;
  };
  // uvs indexed by POSITION — insertion order is a detail of the walk.
  const auto uvsAt = [](const Mesh& m, glm::vec3 p) {
    std::vector<glm::vec2> found;
    for (size_t i = 0; i < m.positions.size(); ++i)
      if (glm::length(m.positions[i] - p) < 1e-6f) found.push_back(m.uvs[i]);
    std::sort(found.begin(), found.end(),
              [](glm::vec2 a, glm::vec2 b) { return a.x < b.x; });
    return found;
  };

  // --- FACEVARYING: the uv source is the CORNER, and the dedup keys on the
  // corner index rather than on the uv value, so every corner becomes its
  // own vertex — 6 corners give 6 vertices even where two of them happen to
  // carry an identical uv. Nothing downstream depends on the merge, so the
  // reader does not pay for comparing values.
  const Part* quad = part("uvquad");
  ASSERT_NE(quad, nullptr);
  const Mesh& fv = quad->mesh;
  ASSERT_EQ(fv.positions.size(), 6u);
  ASSERT_EQ(fv.uvs.size(), 6u);
  EXPECT_EQ(fv.triangleCount(), 2u);
  // The flip in one value: the file's uv (0,0) on the origin corner arrives
  // as (0,1). No reader that passed v through unchanged could produce that.
  const std::vector<glm::vec2> atOrigin = uvsAt(fv, {0, 0, 0});
  ASSERT_EQ(atOrigin.size(), 2u);  // corners 0 and 3, NOT welded
  for (const glm::vec2& uv : atOrigin) {
    EXPECT_FLOAT_EQ(uv.x, 0.0f);
    EXPECT_FLOAT_EQ(uv.y, 1.0f);
  }
  EXPECT_FLOAT_EQ(uvsAt(fv, {0, 1, 0}).at(0).y, 0.0f);  // file v=1 -> 0
  // The shared point at (1,1,0) carried two different uvs; both survive,
  // both flipped.
  const std::vector<glm::vec2> shared = uvsAt(fv, {1, 1, 0});
  ASSERT_EQ(shared.size(), 2u);
  EXPECT_FLOAT_EQ(shared[0].x, 0.25f);
  EXPECT_FLOAT_EQ(shared[0].y, 0.25f);  // file (0.25, 0.75)
  EXPECT_FLOAT_EQ(shared[1].x, 1.0f);
  EXPECT_FLOAT_EQ(shared[1].y, 0.0f);  // file (1, 1)

  // --- VERTEX scope: the uv source IS the point, so every corner of a
  // point keys the same and the six corners weld back down to four
  // vertices. Identical topology to the facevarying case, identical flip —
  // only the scope differs.
  const Part* weld = part("uvweld");
  ASSERT_NE(weld, nullptr);
  const Mesh& vw = weld->mesh;
  ASSERT_EQ(vw.positions.size(), 4u);  // the merge the facevarying case
  ASSERT_EQ(vw.uvs.size(), 4u);        // cannot make
  EXPECT_EQ(vw.triangleCount(), 2u);
  const std::vector<glm::vec2> weldOrigin = uvsAt(vw, {0, 0, 0});
  ASSERT_EQ(weldOrigin.size(), 1u);
  EXPECT_FLOAT_EQ(weldOrigin[0].x, 0.0f);
  EXPECT_FLOAT_EQ(weldOrigin[0].y, 1.0f);  // file (0,0) -> (0,1)
  const std::vector<glm::vec2> weldShared = uvsAt(vw, {1, 1, 0});
  ASSERT_EQ(weldShared.size(), 1u);
  EXPECT_FLOAT_EQ(weldShared[0].x, 1.0f);
  EXPECT_FLOAT_EQ(weldShared[0].y, 0.0f);  // file (1,1) -> (1,0)
}

TEST(ReadAlembic, AReadSelectsTheTimeSampleItWasAskedFor) {
  const std::string bytes = alembicArchiveBytes();
  const auto cloudY = [&](double time) -> float {
    auto model =
        codec::decode::alembic(bytes.data(), bytes.size(), {.time = time});
    if (!model) return -1.0f;
    for (const Part& part : model->parts)
      if (part.name == "cloud") return part.mesh.positions.at(0).y;
    return -1.0f;
  };
  // A requested time selects the NEAREST stored sample, not the one before
  // it: 0.6 of a frame in is closer to frame 1, so frame 1 is what is read.
  // Samples are never interpolated, so the values are always ones an
  // authoring tool actually wrote.
  EXPECT_FLOAT_EQ(cloudY(0), 0.0f);
  EXPECT_FLOAT_EQ(cloudY(1.0 / 24), 1.0f);
  EXPECT_FLOAT_EQ(cloudY(0.6 / 24), 1.0f);
}
