# SigilComposeKit — the KIT tier as its own library.
#
# The tier rule (DESIGN.md "Kernel, util, extensions") says the kit plugs
# public seams and changes no kernel semantics. This target makes that
# STRUCTURAL rather than a convention: it links SigilCompose, so it sees
# every PUBLIC header, and nothing adds the compose source root to its
# include path — so ComposeInternal.h and ComposeRuntime.h are unreachable
# from kit code. BoundaryProbe.cpp is the negative control for exactly that
# (built on demand, expected to fail; see the file).
add_library(SigilComposeKit STATIC
  Kit.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../include/sigilcompose/kit/Divisions.h
  ${CMAKE_CURRENT_LIST_DIR}/../include/sigilcompose/kit/Frame.h
  ${CMAKE_CURRENT_LIST_DIR}/../include/sigilcompose/kit/Kit.h
  ${CMAKE_CURRENT_LIST_DIR}/../include/sigilcompose/kit/Legibility.h
  ${CMAKE_CURRENT_LIST_DIR}/../include/sigilcompose/kit/PixelType.h
  ${CMAKE_CURRENT_LIST_DIR}/../include/sigilcompose/kit/Strokes.h
)

target_compile_features(SigilComposeKit PUBLIC cxx_std_20)

# PUBLIC (not PRIVATE): anything that links the kit gets compose's public
# surface with it, because every kit value is expressed in that vocabulary.
target_link_libraries(SigilComposeKit PUBLIC SigilCompose)

# The negative control. EXCLUDE_FROM_ALL so the ordinary build never sees
# it, and deliberately NOT an add_test(): a compile-failure check cannot
# live inside a gtest binary, and wiring it as a ctest would put a build
# error on the dashboard as a pass. Run it by hand when the boundary is in
# question — it must fail with "'ComposeInternal.h' file not found".
add_executable(compose_kit_boundary_probe EXCLUDE_FROM_ALL BoundaryProbe.cpp)
target_link_libraries(compose_kit_boundary_probe PRIVATE SigilComposeKit)
