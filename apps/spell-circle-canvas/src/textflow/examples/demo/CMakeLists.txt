# textflow_demo — headless PNG panels for library-only surfaces; the
# interactive/animated showcases live in the gallery example (the former
# duplicated scenes were consolidated there).
add_executable(textflow_demo
  textflow_demo.cpp
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
target_link_libraries(textflow_demo PRIVATE TextFlow TextFlowShaders
                                            TextFlowPorts TextFlowKit)
