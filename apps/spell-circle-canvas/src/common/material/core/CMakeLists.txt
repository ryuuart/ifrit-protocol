# The core feature: the value model with no renderer in it. glm for the
# vector types a params struct holds, SigilMotion for the animatable a
# float field binds to — so a shader uniform takes the same bind() chain
# a node transform does, and the same rule decides whether it is moving —
# Boost.PFR for the reflection that turns a params struct into a layout
# and its declarations. No Skia, no Slang: compilers arrive from the
# backend features through registerCompiler().
#
# Params.h includes <boost/pfr.hpp>, so the include path must reach every
# consumer that instantiates schema<P>(); the target is header-only, and
# nothing links. SigilMaterialColor is the colour value a params struct
# holds, included by the same header, and glm the vectors beside it. No
# header here includes SigilGeometry, so no target of it is named: the
# outline a normals pass is drawn over arrives as pixels.
find_package(boost_pfr CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(TBB CONFIG REQUIRED)
set(_shaders ${CMAKE_CURRENT_LIST_DIR}/shaders)
sigil_library(SigilMaterialCore
  SOURCES Combine.cpp Material.cpp Params.cpp Program.cpp Recipe.cpp
          ${_shaders}/OverAdd.sksl ${_shaders}/OverAdd.slang
          ${_shaders}/OverMix.sksl ${_shaders}/OverMix.slang
          ${_shaders}/OverMultiply.sksl ${_shaders}/OverMultiply.slang
          ${_shaders}/OverPrelude.slang
  HEADERS ../Material.h Combine.h FrameData.h Leaf.h Material.h Params.h
          Program.h Recipe.h Target.h UniformBlock.h
  PUBLIC SigilMaterialColor SigilMotionValues glm::glm Boost::pfr
         Boost::container
  PRIVATE SigilIOHub Boost::unordered TBB::tbb)
target_compile_definitions(SigilMaterialCore
  PRIVATE SIGIL_MATERIAL_CORE_SHADER_DIR="${_shaders}")

# Links only the library it exercises, so a link edge that pulled a
# renderer in would show up as a build failure here.
sigil_test(material_core_test
  SOURCES test/CoreTest.cpp
  LIBRARIES SigilMaterialCore)

sigil_bench(material_core_bench
  SOURCES bench/CoreBench.cpp
  LIBRARIES SigilMaterialCore)
