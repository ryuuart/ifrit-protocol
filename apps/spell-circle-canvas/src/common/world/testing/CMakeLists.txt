# The testing feature: the study harness — a scene stepped from zero to
# its declared moment on the CPU, photographed, and written as a plate.
# It links the retained side and Skia's encoder and nothing else, so a
# study is a scene and not a second renderer.
add_library(SigilWorldTesting STATIC
  Runner.cpp
  Study.cpp
  ${SIGIL_WORLD_INCLUDE_DIR}/sigilworld/testing/Study.h
)
target_compile_features(SigilWorldTesting PUBLIC cxx_std_20)
target_include_directories(SigilWorldTesting PUBLIC ${SIGIL_WORLD_INCLUDE_DIR})
target_link_libraries(SigilWorldTesting PUBLIC SigilWorldScene)

# The studies themselves, and the binary that renders them. They are
# sources of the executable rather than of a library: a study joins by
# being named in studies/Studies.h and listed here, so what the registry
# holds is readable in two files instead of depending on which static
# initializers a linker kept.
add_executable(world_studies
  studies/Registry.cpp
  studies/first_light.cpp
  studies/world_studies_main.cpp
  studies/Studies.h
)
target_link_libraries(world_studies PRIVATE SigilWorldTesting
  SigilMaterialCore)
