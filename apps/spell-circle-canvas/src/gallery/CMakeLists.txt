# TextFlowGallery — a standalone Qt Quick executable that exercises the
# TextFlow engine interactively: scene switcher, live text editing, font and
# style controls. Renders with Skia's CPU raster backend into a
# QQuickPaintedItem, so it needs none of the app's Metal/Graphite plumbing —
# it exists to keep the SpellCircle scene backend clean while still having a
# living showcase of the engine.
qt_add_executable(TextFlowGallery
  gallery_main.cpp
)

qt_add_qml_module(TextFlowGallery
  URI TextFlow.Gallery
  VERSION 1.0
  QML_FILES
    Main.qml
  SOURCES
    GalleryView.cpp include/GalleryView.h
    GalleryScenes.cpp include/GalleryScenes.h
)

target_include_directories(TextFlowGallery PRIVATE include)

target_link_libraries(TextFlowGallery
  PRIVATE
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::GuiPrivate # QRhi API (rhi/qrhi.h) used by the QQuickRhiItem renderer
    TextFlow
    TextFlowQt
)

# The Graphite GPU path reuses the app's generic Skia/Metal plumbing
# (SkiaGraphiteContext + SkiaOffscreenSurface); without it the gallery
# still runs on Skia's CPU raster backend with a texture upload.
if(SPELLCIRCLE_ENABLE_SKIA_CANVAS)
  target_link_libraries(TextFlowGallery PRIVATE SpellCircleSkia)
  target_compile_definitions(TextFlowGallery PRIVATE TEXTFLOW_GALLERY_GPU)
endif()
