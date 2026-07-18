# TextFlowGallery — a standalone Qt Quick executable that exercises the
# TextFlow engine interactively: scene switcher, live text editing, font and
# style controls. Its QQuickRhiItem renderer supports a Skia CPU raster path
# everywhere and adds the shared Metal/Graphite plumbing when enabled.
qt_add_executable(TextFlowGallery
  src/gallery_main.cpp
)

set_target_properties(TextFlowGallery PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER "dev.sigil.TextFlowGallery"
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    # WIN32_EXECUTABLE TRUE
)

set_source_files_properties(
  qml/Main.qml
  PROPERTIES QT_RESOURCE_ALIAS Main.qml
)
set_source_files_properties(
  qml/GallerySidebar.qml
  PROPERTIES QT_RESOURCE_ALIAS GallerySidebar.qml
)
set_source_files_properties(
  qml/SceneParameterControl.qml
  PROPERTIES QT_RESOURCE_ALIAS SceneParameterControl.qml
)
set_source_files_properties(
  qml/EffectsControls.qml
  PROPERTIES QT_RESOURCE_ALIAS EffectsControls.qml
)

qt_add_qml_module(TextFlowGallery
  URI TextFlow.Gallery
  VERSION 1.0
  QML_FILES
    qml/Main.qml
    qml/GallerySidebar.qml
    qml/SceneParameterControl.qml
    qml/EffectsControls.qml
  SOURCES
    src/GalleryView.cpp include/GalleryView.h
    include/GalleryScenes.h
    src/SceneRegistry.cpp include/SceneRegistry.h
    src/scenes/SceneSupport.cpp src/scenes/SceneSupport.h
    src/scenes/EffectsParts.h
    src/scenes/ExclusionsScene.cpp
    src/scenes/KnuthPlassScene.cpp
    src/scenes/LoopScene.cpp
    src/scenes/RainScene.cpp
    src/scenes/RippleScene.cpp
    src/scenes/TerminalScene.cpp
    src/scenes/VerticalScene.cpp
    src/scenes/HyperScriptsScene.cpp
    src/scenes/EffectsScene.cpp
    src/scenes/EffectsStressScene.cpp
    src/scenes/LoudShadersScene.cpp
    src/scenes/MarkersScene.cpp
    src/scenes/SlotsScene.cpp
    src/scenes/OverflowScene.cpp
)

target_include_directories(TextFlowGallery PRIVATE include)

target_link_libraries(TextFlowGallery
  PRIVATE
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::GuiPrivate # QRhi API (rhi/qrhi.h) used by the QQuickRhiItem renderer
    TextFlowQt # carries TextFlow transitively
    TextFlowKit # rebuild/layout guards, glyph buckets, labels (kit/README.md)
    TextFlowShaders # PaintShaders presets used by the effect scenes
    TextFlowPorts # systemFontManager()
)

# The Graphite GPU path reuses the generic Skia/Metal plumbing under
# src/common/skia (SkiaGraphiteContext + SkiaOffscreenSurface); without it
# the gallery still runs on Skia's CPU raster backend with a texture upload.
if(SPELLCIRCLE_ENABLE_SKIA_CANVAS)
  target_link_libraries(TextFlowGallery PRIVATE SpellCircleSkiaQt)
  target_compile_definitions(TextFlowGallery PRIVATE TEXTFLOW_GALLERY_GPU)
endif()
