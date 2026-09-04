# The core feature: the value model with no renderer in it. glm for the
# vector types a params struct holds, SigilMotion for the animatable a
# float field binds to — so a shader uniform takes the same bind() chain
# a node transform does, and the same rule decides whether it is moving —
# Boost.PFR (header-only, private: no consumer needs it to read the
# model) for the reflection that turns a params struct into a layout and
# its declarations. No Skia, no Slang: compilers arrive from
# the backend features through registerCompiler().
find_package(boost_pfr CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(TBB CONFIG REQUIRED)
set(_shaders ${CMAKE_CURRENT_LIST_DIR}/shaders)
add_library(SigilMaterialCore STATIC
  Combine.cpp
  ${_shaders}/OverAdd.sksl
  ${_shaders}/OverAdd.slang
  ${_shaders}/OverMix.sksl
  ${_shaders}/OverMix.slang
  ${_shaders}/OverMultiply.sksl
  ${_shaders}/OverMultiply.slang
  ${_shaders}/OverPrelude.slang
  Material.cpp
  Params.cpp
  Program.cpp
  Recipe.cpp
  ${SIGIL_MATERIAL_INCLUDE_DIR}/sigilmaterial/Material.h
  ${SIGIL_MATERIAL_INCLUDE_DIR}/sigilmaterial/core/Combine.h
  ${SIGIL_MATERIAL_INCLUDE_DIR}/sigilmaterial/core/FrameData.h
  ${SIGIL_MATERIAL_INCLUDE_DIR}/sigilmaterial/core/Leaf.h
  ${SIGIL_MATERIAL_INCLUDE_DIR}/sigilmaterial/core/Material.h
  ${SIGIL_MATERIAL_INCLUDE_DIR}/sigilmaterial/core/Params.h
  ${SIGIL_MATERIAL_INCLUDE_DIR}/sigilmaterial/core/Program.h
  ${SIGIL_MATERIAL_INCLUDE_DIR}/sigilmaterial/core/Recipe.h
  ${SIGIL_MATERIAL_INCLUDE_DIR}/sigilmaterial/core/Target.h
  ${SIGIL_MATERIAL_INCLUDE_DIR}/sigilmaterial/core/UniformBlock.h
)
target_compile_features(SigilMaterialCore PUBLIC cxx_std_20)
target_include_directories(SigilMaterialCore PUBLIC ${SIGIL_MATERIAL_INCLUDE_DIR})
target_compile_definitions(SigilMaterialCore
  PRIVATE SIGIL_MATERIAL_CORE_SHADER_DIR="${_shaders}")
# Params.h includes <boost/pfr.hpp>, so the include path must reach every
# consumer that instantiates schema<P>(); the target is header-only, and
# nothing links. SigilMaterialColor is the colour value a params struct
# holds, included by the same header, and glm the vectors beside it. No
# header here includes SigilGeometry, so no target of it is named: the
# outline a normals pass is drawn over arrives as pixels.
target_link_libraries(SigilMaterialCore
  PUBLIC SigilMaterialColor SigilMotionValues glm::glm Boost::pfr
         Boost::container)
target_link_libraries(SigilMaterialCore PRIVATE SigilIOHub Boost::unordered)
target_link_libraries(SigilMaterialCore PRIVATE TBB::tbb)

# Links only the library it exercises, so a link edge that pulled a
# renderer in would show up as a build failure here.
add_executable(material_core_test test/CoreTest.cpp)
target_link_libraries(material_core_test PRIVATE SigilMaterialCore
  GTest::gtest_main)
add_test(NAME material_core_test COMMAND material_core_test)

# Google Benchmark (use a Release build), linking only what it measures.
# Not a test: it builds through the `benches` target and runs through
# scripts/bench_ledger.py.
add_executable(material_core_bench bench/CoreBench.cpp)
target_link_libraries(material_core_bench PRIVATE SigilMaterialCore
  benchmark::benchmark)
add_dependencies(benches material_core_bench)
