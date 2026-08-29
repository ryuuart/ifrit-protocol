# SigilUsd — the world's data in and out of USD (OpenUSD, core only).
# A leaf: it links SigilWorld and SigilGeometry; nothing links it but its
# test, its demo hooks and whoever wants a stage.
find_package(pxr CONFIG REQUIRED)

add_library(SigilUsd STATIC
  Usd.cpp
  include/sigilusd/Usd.h
)
target_compile_features(SigilUsd PUBLIC cxx_std_20)
target_include_directories(SigilUsd PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)
target_link_libraries(SigilUsd
  PUBLIC SigilWorld SigilGeometry unofficial::skia::skia
  PRIVATE usd usdGeom usdShade usdLux sdf tf gf vt)

add_executable(usd_test test/UsdTest.cpp)
target_link_libraries(usd_test PRIVATE SigilUsd GTest::gtest_main)
add_test(NAME usd_test COMMAND usd_test)

# Registers this library with the `docs` target (see cmake/Docs.cmake).
# The README is the site's front page, so the generated pages open on the
# same document that is canon for the library.
sigil_add_docs(
  NAME SigilUsd
  BRIEF "The world's data written to and read back from USD stages"
  INPUT
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/README.md
  MAINPAGE ${CMAKE_CURRENT_SOURCE_DIR}/README.md
  STRIP
        ${CMAKE_CURRENT_SOURCE_DIR}/include)
