# The ocio feature: OpenColorIO view transforms baked to materials over
# the texture feature. OpenColorIO is already in the dependency
# closure (transitively via OpenImageIO), so this adds no new vcpkg
# dependency; where the package is absent the feature still builds, with
# `available()` false and every factory answering the empty LUT material,
# so a consumer links one name on every machine. The definition is PUBLIC
# so a consumer can #ifdef around the transforms.
find_package(OpenColorIO CONFIG QUIET)
set(_shaders ${CMAKE_CURRENT_LIST_DIR}/shaders)
sigil_library(SigilMaterialOcio
  SOURCES LutRecipe.cpp ${_shaders}/Lut3d.sksl ${_shaders}/Response1d.sksl
  HEADERS Ocio.h
  PUBLIC SigilMaterialTexture
  PRIVATE SigilIOHub)
target_compile_definitions(SigilMaterialOcio
  PRIVATE SIGIL_MATERIAL_OCIO_SHADER_DIR="${_shaders}")
if(TARGET OpenColorIO::OpenColorIO)
  target_sources(SigilMaterialOcio PRIVATE Ocio.cpp)
  target_link_libraries(SigilMaterialOcio PRIVATE OpenColorIO::OpenColorIO)
  target_compile_definitions(SigilMaterialOcio PUBLIC SIGILMATERIAL_ENABLE_OCIO)
else()
  target_sources(SigilMaterialOcio PRIVATE Unavailable.cpp)
  message(WARNING
    "OpenColorIO not found — <sigilmaterial/ocio/Ocio.h> answers unavailable")
endif()

# The test shades through the Skia backend, and skips where the
# transforms are unavailable.
sigil_test(material_ocio_test
  SOURCES test/OcioTest.cpp
  LIBRARIES SigilMaterialOcio SigilMaterialSkia)

sigil_bench(material_ocio_bench
  SOURCES bench/OcioBench.cpp
  LIBRARIES SigilMaterialOcio SigilMaterialSkia)
