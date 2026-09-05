# The write feature: a stage built from the values a scene is made of and
# saved in the format the file's extension asks for. Links the geometry
# features it takes values from, SigilMaterialKit for the surface recipe
# whose params and map slots become a preview surface, SigilWorldLight for
# the emitters and SigilWorldElement for the environment map a dome light
# carries; OpenUSD, the image encoder that turns a texture into bytes and
# the sink that lands them beside the stage are private to it.
sigil_library(SigilUsdWrite
  SOURCES Camera.cpp Environment.cpp GfMatrix.h Lights.cpp Material.cpp
          Mesh.cpp Stamps.cpp Writer.cpp WriterImpl.h
  HEADERS Writer.h
  PUBLIC SigilMaterialKit SigilWorldElement SigilWorldLight SigilGeometryMesh
         SigilGeometryMeshPop SigilGeometryMeshCamera unofficial::skia::skia
  PRIVATE SigilImageEncode SigilIOSource usd usdGeom usdShade usdLux
          usdUtils sdf tf gf vt Boost::unordered)

# Links the library it exercises, the runtime probe it skips on, and USD
# itself to inspect the authored stage.
sigil_test(usd_write_test
  SOURCES test/WriteTest.cpp
  LABELS usd
  LIBRARIES SigilUsdWrite SigilUsdRuntime SigilGeometryKit usd usdGeom
            usdShade sdf tf vt)

sigil_bench(usd_write_bench
  SOURCES bench/WriteBench.cpp
  LIBRARIES SigilUsdWrite SigilUsdRuntime SigilGeometryKit)
