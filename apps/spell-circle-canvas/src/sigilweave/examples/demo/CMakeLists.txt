# weave_demo — headless PNG panels for library-only surfaces; the
# interactive and animated showcases are the gallery example's.
find_package(boost_container CONFIG REQUIRED)
add_executable(weave_demo
  weave_demo.cpp
  DemoSupport.cpp DemoSupport.h
  DemoScenes.h
  SceneExtreme.cpp
  SceneTypography.cpp
  SceneBabel.cpp
  SceneFeatures.cpp
  SceneCjk.cpp
  SceneShapes.cpp
  SceneFallback.cpp
  SceneNewFeatures.cpp
)
target_include_directories(weave_demo PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../shared)
target_link_libraries(weave_demo PRIVATE SigilWeave
                                            SigilMaterialKit SigilMaterialSkia
                                            SigilWeavePorts SigilWeaveKit
                                            SigilImageEncode SigilIOSource
                                            SigilMeasure
                                            Boost::container)
