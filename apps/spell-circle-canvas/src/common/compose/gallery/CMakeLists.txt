# ComposeGallery — Qt Quick app in the WeaveGallery mold: QML
# sidebar over the scene registry (every STRESS_TESTS.md gallery item,
# tagged), clock controls, and a live frame-metrics panel.
# `--headless <outdir>` prints the per-scene FPS table and captures.
qt_add_executable(ComposeGallery
  gallery_main.cpp
  GalleryScenes.h
)
qt_add_qml_module(ComposeGallery
  URI Compose.Gallery
  VERSION 1.0
  QML_FILES qml/Main.qml
  SOURCES ComposeGalleryView.cpp ComposeGalleryView.h
)
set_source_files_properties(qml/Main.qml
  PROPERTIES QT_RESOURCE_ALIAS Main.qml)
target_include_directories(ComposeGallery PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(ComposeGallery PRIVATE
  SigilCompose SigilWeavePorts Qt6::Gui Qt6::GuiPrivate Qt6::Quick
  Qt6::QuickControls2 EnTT::EnTT)

# Draw directly into QQuickRhiItem's texture with the shared Skia Graphite
# context when the active QRhi backend is supported. Other builds keep the
# explicit CPU-raster/upload fallback in ComposeGalleryView.cpp.
if(TARGET SpellCircleSkiaQt)
  target_link_libraries(ComposeGallery PRIVATE SpellCircleSkiaQt)
  target_compile_definitions(ComposeGallery PRIVATE SIGILCOMPOSE_GALLERY_GPU)
endif()

add_executable(compose_gallery_test
  test/GalleryTest.cpp
)
target_include_directories(compose_gallery_test PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(compose_gallery_test PRIVATE
  SigilCompose SigilWeavePorts GTest::gtest_main EnTT::EnTT)
add_test(NAME compose_gallery_test COMMAND compose_gallery_test)
