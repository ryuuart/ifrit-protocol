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
  SigilCompose SigilWeavePorts Qt6::Gui Qt6::Quick EnTT::EnTT)

