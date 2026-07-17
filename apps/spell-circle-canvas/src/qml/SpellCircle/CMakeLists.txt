# SkiaSceneBackend.mm (real Skia Graphite backend) vs. SkiaSceneBackendStub.cpp
# (always-null stub) is a build-time swap on the same createSkiaSceneBackend()
# factory declared in include/SkiaSceneBackend.h — see SPELLCIRCLE_ENABLE_SKIA_CANVAS
# in the top-level CMakeLists.txt. SpellCircleRenderer never branches on which
# one it got.
if(SPELLCIRCLE_ENABLE_SKIA_CANVAS)
  set(SPELLCIRCLE_SKIA_SCENE_BACKEND_SOURCE SkiaSceneBackend.mm)
  set(SPELLCIRCLE_SKIA_SCENE_LABEL_SOURCES
      SceneLabels.cpp include/SceneLabels.h)
else()
  set(SPELLCIRCLE_SKIA_SCENE_BACKEND_SOURCE SkiaSceneBackendStub.cpp)
  set(SPELLCIRCLE_SKIA_SCENE_LABEL_SOURCES)
endif()

qt_add_qml_module(SpellCircleCanvas
    URI SpellCircle.Canvas
    VERSION 1.0
    SOURCES
      SyphonBridge.mm include/SyphonBridge.h
      include/TexturePublisher.h
      SpellCircle.cpp include/SpellCircle.h
      SpellCircleRenderer.cpp include/SpellCircleRenderer.h
      QCanvasPainterSceneBackend.cpp include/QCanvasPainterSceneBackend.h
      include/CanvasSceneBackend.h
      include/SkiaSceneBackend.h
      ${SPELLCIRCLE_SKIA_SCENE_BACKEND_SOURCE}
      ${SPELLCIRCLE_SKIA_SCENE_LABEL_SOURCES}
)

target_link_libraries(SpellCircleCanvas
    PUBLIC
      Qt6::Quick
      Qt6::CanvasPainter
    PRIVATE
      spdlog::spdlog
      SpellCircleModels # SpellCircleModel/GraphicsConfig (carries network)
      QtCanvasTextPath
      EnTT::EnTT
      Qt::GuiPrivate
      ${FOUNDATION_FRAMEWORK}
      ${SYPHON_FRAMEWORK}
      ${CORE_GRAPHICS_FRAMEWORK}
)

# SpellCircleSkia (the generic Skia/Metal GPU plumbing under src/platform/skia)
# only exists when SPELLCIRCLE_ENABLE_SKIA_CANVAS is ON. TextFlow (src/text)
# provides the animated text layer drawn by SkiaSceneBackend.mm.
if(SPELLCIRCLE_ENABLE_SKIA_CANVAS)
  # TextFlowQt carries TextFlow transitively; TextFlowPorts provides the
  # system font manager used by the render-thread FontContext. (No direct
  # absl links: TextFlow's public headers no longer expose abseil types.)
  target_link_libraries(SpellCircleCanvas PRIVATE SpellCircleSkia TextFlowQt
    TextFlowPorts)
endif()

target_include_directories(SpellCircleCanvas PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/include
)
