# SigilComposeShape — silhouettes and placement.
#
# The stock silhouette catalog (`shapes::`, as generators, parametric
# curves, corner treatments and per-edge extraction), the layout catalog
# (`layouts::`) and the routers that place children along a path. Links
# Core publicly: every value here is an Element or a Shape.
add_library(SigilComposeShape STATIC
  Shapes.cpp
  Routers.cpp
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/shape/Corners.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/shape/Curves.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/shape/Edges.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/shape/Generators.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/shape/Layouts.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/shape/Routers.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/shape/Shape.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/shape/Shapes.h
)
target_compile_features(SigilComposeShape PUBLIC cxx_std_20)
target_link_libraries(SigilComposeShape PUBLIC SigilComposeCore)

# Silhouettes, layouts, routers, rails and travel along a path.
_compose_test_binary(compose_shape_test
  LIBRARIES SigilComposeShape
  SOURCES
    test/ComposeTestContentShape.cpp
    test/ComposeTestKernelShape.cpp
    test/ComposeTestLinesShape.cpp
)

# Comparable silhouettes, hit testing and blended shaped leaves.
_compose_bench(compose_shape_bench
  LIBRARIES SigilComposeShape
  SOURCES bench/ComposeShapeBench.cpp)
