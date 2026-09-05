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
  HEADERS ../Material.h Combine.h FrameData.h Leaf.h Material.h Params.h
          Program.h Recipe.h Target.h UniformBlock.h
  PUBLIC SigilMaterialColor SigilMotionValues glm::glm Boost::pfr
         Boost::container
  PRIVATE Boost::unordered TBB::tbb)
# The three stacking bodies are compiled into the archive, so the value
# model carries them with no way to read a file.
sigil_shader_sources(SigilMaterialCore
  DIR ${_shaders} NAMESPACE sigil::material)

# Links only the library it exercises, so a link edge that pulled a
# renderer in would show up as a build failure here. The shader
# directory is named for the case that asks whether the embedded table
# holds every file in it.
sigil_test(material_core_test
  SOURCES test/CoreTest.cpp
  SUPPORT_DIRS ${SIGIL_SHADER_GENERATED_DIR}
  DEFINITIONS SIGIL_MATERIAL_CORE_SHADER_DIR="${_shaders}"
  LIBRARIES SigilMaterialCore)

sigil_bench(material_core_bench
  SOURCES bench/CoreBench.cpp
  LIBRARIES SigilMaterialCore)
