# TextFlowGallery — a standalone Qt Quick executable that exercises the
# TextFlow engine interactively: scene switcher, live text editing, font and
# style controls. Its QQuickRhiItem renderer supports a Skia CPU raster path
# everywhere and adds the shared Metal/Graphite plumbing when enabled.
qt_add_executable(TextFlowGallery
  gallery_main.cpp
)

set_target_properties(TextFlowGallery PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER "dev.sigil.TextFlowGallery"
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    # WIN32_EXECUTABLE TRUE
)

set_source_files_properties(
  ../qml/gallery/Main.qml
  PROPERTIES QT_RESOURCE_ALIAS Main.qml
)
set_source_files_properties(
  ../qml/gallery/GallerySidebar.qml
  PROPERTIES QT_RESOURCE_ALIAS GallerySidebar.qml
)
set_source_files_properties(
  ../qml/gallery/SceneParameterControl.qml
  PROPERTIES QT_RESOURCE_ALIAS SceneParameterControl.qml
)
set_source_files_properties(
  ../qml/gallery/EffectsControls.qml
  PROPERTIES QT_RESOURCE_ALIAS EffectsControls.qml
)

qt_add_qml_module(TextFlowGallery
  URI TextFlow.Gallery
  VERSION 1.0
  QML_FILES
    ../qml/gallery/Main.qml
    ../qml/gallery/GallerySidebar.qml
    ../qml/gallery/SceneParameterControl.qml
    ../qml/gallery/EffectsControls.qml
  SOURCES
    GalleryView.cpp include/GalleryView.h
    include/GalleryScenes.h
    SceneRegistry.cpp include/SceneRegistry.h
    scenes/SceneSupport.cpp scenes/SceneSupport.h
    scenes/EffectsParts.h
    scenes/ExclusionsScene.cpp
    scenes/KnuthPlassScene.cpp
    scenes/LoopScene.cpp
    scenes/RainScene.cpp
    scenes/RippleScene.cpp
    scenes/VerticalScene.cpp
    scenes/HyperScriptsScene.cpp
    scenes/EffectsScene.cpp
    scenes/EffectsStressScene.cpp
    scenes/LoudShadersScene.cpp
    scenes/MarkersScene.cpp
    scenes/SlotsScene.cpp
    scenes/OverflowScene.cpp
)

target_include_directories(TextFlowGallery PRIVATE include)

target_link_libraries(TextFlowGallery
  PRIVATE
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::GuiPrivate # QRhi API (rhi/qrhi.h) used by the QQuickRhiItem renderer
    TextFlowQt # carries TextFlow transitively
    TextFlowShaders # PaintShaders presets used by the effect scenes
    TextFlowPorts # systemFontManager()
)

# The Graphite GPU path reuses the app's generic Skia/Metal plumbing
# (SkiaGraphiteContext + SkiaOffscreenSurface); without it the gallery
# still runs on Skia's CPU raster backend with a texture upload.
if(SPELLCIRCLE_ENABLE_SKIA_CANVAS)
  target_link_libraries(TextFlowGallery PRIVATE SpellCircleSkia)
  target_compile_definitions(TextFlowGallery PRIVATE TEXTFLOW_GALLERY_GPU)
endif()
