# weave_demo — headless PNG panels for library-only surfaces; the
# interactive/animated showcases live in the gallery example (the former
# duplicated scenes were consolidated there).
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
target_link_libraries(weave_demo PRIVATE SigilWeave SigilWeaveShaders
                                            SigilWeavePorts SigilWeaveKit)
