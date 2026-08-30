// geometry_import_bench — the model readers over bytes already in
// memory, so what is measured is parsing and mesh assembly and never the
// disk: OBJ text and a GLB container by triangle count, and the Houdini
// .geo reader by point count. Run a Release build; Debug numbers say
// nothing.

#include <benchmark/benchmark.h>
#include <sigilgeometry/Import.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

using namespace sigil::geometry;

namespace {

template <typename T>
void appendRaw(std::vector<std::byte>& out, const T& value) {
  const auto* p = reinterpret_cast<const std::byte*>(&value);
  out.insert(out.end(), p, p + sizeof(T));
}

/** An n x n height-field grid as OBJ text: positions, normals, uvs and
 *  quad faces (each fan-triangulated on read). */
std::string gridObj(int n) {
  std::string obj;
  obj.reserve((size_t)n * n * 80);
  for (int y = 0; y < n; ++y)
    for (int x = 0; x < n; ++x) {
      obj += "v ";
      obj += std::to_string(x);
      obj += ' ';
      obj += std::to_string((x * y) % 7);
      obj += ' ';
      obj += std::to_string(y);
      obj += '\n';
      obj += "vn 0 1 0\n";
      obj += "vt ";
      obj += std::to_string((float)x / n);
      obj += ' ';
      obj += std::to_string((float)y / n);
      obj += '\n';
    }
  for (int y = 0; y + 1 < n; ++y)
    for (int x = 0; x + 1 < n; ++x) {
      const int a = y * n + x + 1, b = a + 1, c = a + n + 1, d = a + n;
      auto v = [](int i) {
        const std::string s = std::to_string(i);
        std::string triplet = s;
        triplet += '/';
        triplet += s;
        triplet += '/';
        triplet += s;
        return triplet;
      };
      obj += "f ";
      for (int corner : {a, b, c, d}) {
        obj += v(corner);
        obj += corner == d ? '\n' : ' ';
      }
    }
  return obj;
}

/** The same grid as a GLB: one node, one primitive, float positions and
 *  32-bit indices in the embedded buffer. */
std::vector<std::byte> gridGlb(int n) {
  std::vector<std::byte> bin;
  for (int y = 0; y < n; ++y)
    for (int x = 0; x < n; ++x) {
      appendRaw(bin, (float)x);
      appendRaw(bin, (float)((x * y) % 7));
      appendRaw(bin, (float)y);
    }
  const uint32_t positionBytes = (uint32_t)bin.size();
  uint32_t indexCount = 0;
  for (int y = 0; y + 1 < n; ++y)
    for (int x = 0; x + 1 < n; ++x) {
      const uint32_t a = (uint32_t)(y * n + x), b = a + 1,
                     c = a + (uint32_t)n + 1, d = a + (uint32_t)n;
      for (uint32_t i : {a, b, c, a, c, d}) appendRaw(bin, i);
      indexCount += 6;
    }
  const uint32_t indexBytes = (uint32_t)bin.size() - positionBytes;
  while (bin.size() % 4) bin.push_back(std::byte{0});

  std::string json =
      "{\"asset\":{\"version\":\"2.0\"},\"scene\":0,\"scenes\":[{\"nodes\":[0]}"
      "],"
      "\"nodes\":[{\"mesh\":0}],\"meshes\":[{\"primitives\":[{\"attributes\":"
      "{\"POSITION\":0},\"indices\":1}]}],\"buffers\":[{\"byteLength\":" +
      std::to_string(bin.size()) +
      "}],\"bufferViews\":[{\"buffer\":0,\"byteOffset\":0,\"byteLength\":" +
      std::to_string(positionBytes) +
      "},{\"buffer\":0,\"byteOffset\":" + std::to_string(positionBytes) +
      ",\"byteLength\":" + std::to_string(indexBytes) +
      "}],\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":" +
      std::to_string(n * n) + ",\"type\":\"VEC3\",\"min\":[0,0,0],\"max\":[" +
      std::to_string(n - 1) + ",6," + std::to_string(n - 1) +
      "]},{\"bufferView\":1,\"componentType\":5125,\"count\":" +
      std::to_string(indexCount) + ",\"type\":\"SCALAR\"}]}";
  while (json.size() % 4) json.push_back(' ');

  std::vector<std::byte> out;
  appendRaw(out, (uint32_t)0x46546C67);  // "glTF"
  appendRaw(out, (uint32_t)2);
  appendRaw(out, (uint32_t)(12 + 8 + json.size() + 8 + bin.size()));
  appendRaw(out, (uint32_t)json.size());
  appendRaw(out, (uint32_t)0x4E4F534A);  // "JSON"
  for (char c : json) out.push_back((std::byte)c);
  appendRaw(out, (uint32_t)bin.size());
  appendRaw(out, (uint32_t)0x004E4942);  // "BIN"
  out.insert(out.end(), bin.begin(), bin.end());
  return out;
}

void countTriangles(benchmark::State& state, const import::Model& model,
                    size_t bytes) {
  state.counters["triangles/s"] =
      benchmark::Counter((double)model.triangleCount(),
                         benchmark::Counter::kIsIterationInvariantRate);
  state.SetBytesProcessed(state.iterations() * (int64_t)bytes);
  state.SetComplexityN((int64_t)model.triangleCount());
}

void BM_ImportObj(benchmark::State& state) {
  const std::string obj = gridObj((int)state.range(0));
  std::optional<import::Model> model;
  for ([[maybe_unused]] auto iteration : state) {
    model = import::model(obj.data(), obj.size(), "grid.obj");
    benchmark::DoNotOptimize(model);
  }
  if (!model) {
    state.SkipWithError("OBJ did not parse");
    return;
  }
  countTriangles(state, *model, obj.size());
}
BENCHMARK(BM_ImportObj)
    ->RangeMultiplier(4)
    ->Range(8, 512)
    ->Unit(benchmark::kMicrosecond)
    ->Complexity(benchmark::oN);

void BM_ImportGlb(benchmark::State& state) {
  const std::vector<std::byte> glb = gridGlb((int)state.range(0));
  std::optional<import::Model> model;
  for ([[maybe_unused]] auto iteration : state) {
    model = import::model(glb.data(), glb.size(), "grid.glb");
    benchmark::DoNotOptimize(model);
  }
  if (!model) {
    state.SkipWithError("GLB did not parse");
    return;
  }
  countTriangles(state, *model, glb.size());
}
BENCHMARK(BM_ImportGlb)
    ->RangeMultiplier(4)
    ->Range(8, 512)
    ->Unit(benchmark::kMicrosecond)
    ->Complexity(benchmark::oN);

/** A .geo file of @p points particles, written the way Houdini does,
 *  parsed back: the JSON reader and the paged decode. */
void BM_ImportGeo_Points(benchmark::State& state) {
  const int n = (int)state.range(0);
  std::string geo =
      "[\"fileversion\",\"20.5.278\",\"pointcount\"," + std::to_string(n) +
      ",\"vertexcount\",0,\"primitivecount\",0,"
      "\"topology\",[\"pointref\",[\"indices\",[]]],"
      "\"attributes\",[\"pointattributes\",[[[\"scope\",\"public\",\"type\","
      "\"numeric\",\"name\",\"P\",\"options\",{}],[\"size\",3,\"storage\","
      "\"fpreal32\",\"values\",[\"size\",3,\"storage\",\"fpreal32\","
      "\"packing\","
      "[3],\"pagesize\",1024,\"constantpageflags\",[[";
  const int pages = (n + 1023) / 1024;
  for (int p = 0; p < pages; ++p) geo += p ? ",false" : "false";
  geo += "]],\"rawpagedata\",[";
  for (int i = 0; i < n; ++i) {
    if (i) geo += ',';
    geo += std::to_string(i % 100) + ",0.5," + std::to_string(i % 7);
  }
  geo += "]]]]]],\"primitives\",[]]";
  for ([[maybe_unused]] auto iteration : state)
    benchmark::DoNotOptimize(import::model(geo.data(), geo.size(), "p.geo"));
  state.counters["points/s"] =
      benchmark::Counter(n, benchmark::Counter::kIsIterationInvariantRate);
  state.SetBytesProcessed(state.iterations() * (int64_t)geo.size());
  state.SetComplexityN(n);
}
BENCHMARK(BM_ImportGeo_Points)
    ->Arg(10000)
    ->Arg(100000)
    ->Unit(benchmark::kMicrosecond)
    ->Complexity(benchmark::oN);

}  // namespace

BENCHMARK_MAIN();
