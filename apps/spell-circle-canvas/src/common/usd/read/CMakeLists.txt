# The read feature: a stage poured into geometry's decode::Model, with
# its emitters and cameras read back as values. Links the codec feature
# the Model comes from, SigilWorldElement and SigilWorldLight and the geometry
# camera those values are; OpenUSD is private to it.
sigil_library(SigilUsdRead
  SOURCES Camera.cpp Environment.cpp GfMatrix.h Instancer.cpp Lights.cpp
          Material.cpp Mesh.cpp Primvar.h ReadContext.h Reader.cpp
  HEADERS Reader.h
  PUBLIC SigilGeometryMeshCodec SigilGeometryMesh SigilGeometryMeshCamera
         SigilWorldElement SigilWorldLight
  PRIVATE usd usdGeom usdShade usdLux ar sdf tf gf vt)

# Links the library it exercises, the runtime probe it skips on, and the
# writer whose stages it round-trips; the committed fixtures under this
# feature's test/assets/ reach it through SIGIL_TEST_ASSET_DIR.
sigil_test(usd_read_test
  SOURCES test/ReadTest.cpp
  LABELS usd
  LIBRARIES SigilUsdRead SigilUsdWrite SigilUsdRuntime SigilGeometryKit
  ASSET_DIR ${CMAKE_CURRENT_LIST_DIR}/test/assets)

sigil_bench(usd_read_bench
  SOURCES bench/ReadBench.cpp
  LIBRARIES SigilUsdRead SigilUsdWrite SigilUsdRuntime SigilGeometryKit)
