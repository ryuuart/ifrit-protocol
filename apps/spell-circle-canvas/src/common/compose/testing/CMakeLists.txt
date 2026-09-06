# SigilComposeTesting — the checks a test, a sketch's --verify pass or a
# gallery verification plate reads back a drawing with. Header-only, and its
# own target rather than more lines in SigilCompose: nothing that ships
# links it, so a paint loop cannot reach a point-sampled coverage scan by
# accident, and its `report()` may write into a feed without the library
# depending on one. The checks themselves are SigilMeasure's; this target
# adds the geometry readers and the feed report over them. Its headers
# sit under an include root of their own, so only a target that links it
# can reach them.
add_library(SigilComposeTesting INTERFACE)
target_include_directories(SigilComposeTesting INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/include)
target_link_libraries(SigilComposeTesting INTERFACE SigilComposeCore
  SigilMeasure)

# The checks over their own indexes: that a coverage scan and a width
# audit answer over an indexed figure exactly what they answer asked
# piece by piece and edge by edge, and that neither loses a defect to its
# index. Links the kernel and this target alone — a check reads geometry
# and pixels, and nothing above the kernel is needed to hand it either.
sigil_test(compose_test
  SOURCES test/ComposeTestChecks.cpp
  LIBRARIES SigilComposeCore SigilComposeTesting SigilWeavePorts)

# What a verification pass costs, in the two terms it grows in: the
# lattice a coverage scan samples on, and the steps a band is built from.
sigil_bench(compose_bench
  SOURCES bench/ComposeChecksBench.cpp
  LIBRARIES SigilComposeCore SigilComposeTesting SigilWeavePorts)
