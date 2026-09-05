# WeaveGallery — a standalone Qt Quick executable that exercises the
# SigilWeave engine interactively: scene switcher, live text editing, font and
# style controls. Its QQuickRhiItem renderer draws through the shared
# Metal/Graphite plumbing, with a Skia CPU raster path beside it.
qt_add_executable(WeaveGallery
  src/gallery_main.cpp
)
sigil_qt_target(WeaveGallery)

set_target_properties(WeaveGallery PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER "dev.sigil.WeaveGallery"
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

qt_add_qml_module(WeaveGallery
  URI SigilWeave.Gallery
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

target_include_directories(WeaveGallery PRIVATE include
                                                 ${CMAKE_CURRENT_LIST_DIR}/../shared)

target_link_libraries(WeaveGallery
  PRIVATE
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::GuiPrivate # QRhi API (rhi/qrhi.h) used by the QQuickRhiItem renderer
    SigilWeave   # the whole engine: layout, choreography, the label cache
    SigilWeaveQt # QFont/QString bridging
    SigilWeaveKit # rebuild/layout guards, glyph buckets, labels
    SigilMaterialKit  # the text-paint presets the effect scenes shade
    SigilMaterialSkia # and the Skia backend that shades them
    SigilWeavePorts # systemFontManager()
    SigilMeasure # the stopwatch each scene times its layout with
    SigilCoreCache # the rebuild guard each scene keys its derived values on
    Boost::container
)

# The Graphite GPU path draws through the shared Skia/Metal plumbing under
# src/common/skia; the CPU raster path with a texture upload stands beside
# it.
target_link_libraries(WeaveGallery PRIVATE SigilSkiaQt)
target_compile_definitions(WeaveGallery PRIVATE TEXTFLOW_GALLERY_GPU)
