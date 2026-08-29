# SigilComposeTesting — the checks a test, a sketch's --verify pass or a
# gallery verification plate reads back a drawing with. Header-only, and its
# own target rather than more lines in SigilCompose: nothing that ships
# links it, so a paint loop cannot reach a point-sampled coverage scan by
# accident, and its `report()` may write into a feed without the library
# depending on one.
add_library(SigilComposeTesting INTERFACE)
target_include_directories(SigilComposeTesting INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/include)
target_link_libraries(SigilComposeTesting INTERFACE SigilCompose)
