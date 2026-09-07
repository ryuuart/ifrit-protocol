# The graph feature: the order a frame's passes run in, derived from
# what they declared they read and write; the surfaces two resources
# share when their lives do not overlap; the barriers between them; and
# how each pass's selection is realised. It reads declarations and draws
# nothing, so it links the frame feature and nothing else.
find_package(boost_container CONFIG REQUIRED)
sigil_library(SigilWorldGraph
  SOURCES Barriers.cpp Claims.cpp Order.cpp Plan.cpp Realise.cpp
          Resources.cpp Build.h
  HEADERS Plan.h
  PUBLIC SigilWorldFrame
  PRIVATE Boost::container)

sigil_test(world_test
  SOURCES test/GraphTest.cpp
  LIBRARIES SigilWorldGraph)

sigil_bench(world_bench
  SOURCES bench/GraphBench.cpp
  LIBRARIES SigilWorldGraph)
