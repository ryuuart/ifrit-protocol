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
  SOURCES LutRecipe.cpp
  HEADERS Ocio.h
  PUBLIC SigilMaterialTexture)
sigil_shader_sources(SigilMaterialOcio
  DIR ${_shaders} NAMESPACE sigil::material::ocio)
if(TARGET OpenColorIO::OpenColorIO)
  target_sources(SigilMaterialOcio PRIVATE Ocio.cpp)
  target_link_libraries(SigilMaterialOcio PRIVATE OpenColorIO::OpenColorIO)
  target_compile_definitions(SigilMaterialOcio PUBLIC SIGILMATERIAL_ENABLE_OCIO)
else()
  target_sources(SigilMaterialOcio PRIVATE Unavailable.cpp)
  message(WARNING
    "OpenColorIO not found — <sigilmaterial/ocio/Ocio.h> answers unavailable")
endif()

# test/OcioTest.cpp is compiled into material_test, at the library root,
# which carries the `ocio` label for it.

sigil_bench(material_ocio_bench
  SOURCES bench/OcioBench.cpp
  LIBRARIES SigilMaterialOcio SigilMaterialSkia)
