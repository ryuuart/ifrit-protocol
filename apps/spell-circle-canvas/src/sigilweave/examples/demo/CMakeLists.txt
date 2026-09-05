# weave_demo — headless PNG panels for library-only surfaces; the
# interactive and animated showcases are the gallery example's.
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
target_link_libraries(weave_demo PRIVATE SigilWeave
                                            SigilMaterialKit SigilMaterialSkia
                                            SigilWeavePorts SigilWeaveKit
                                            SigilImageEncode SigilIOSource
                                            Boost::container)
