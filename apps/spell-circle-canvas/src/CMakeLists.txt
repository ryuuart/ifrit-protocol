add_subdirectory(network)
add_subdirectory(model)
add_subdirectory(platform)
add_subdirectory(text)
add_subdirectory(painting)
add_subdirectory(qml)
add_subdirectory(gallery)

qt_add_executable(SpellCircle
    main.cpp
)

# Keep application-specific QML organized by concern while exposing its types
# at the executable module root. Reusable controls live in Ifrit.Ui.
set_source_files_properties(
    qml/app/Main.qml
    PROPERTIES QT_RESOURCE_ALIAS Main.qml
)
set_source_files_properties(
    qml/app/SettingsWindow.qml
    PROPERTIES QT_RESOURCE_ALIAS SettingsWindow.qml
)
set_source_files_properties(
    qml/app/components/LogSidebar.qml
    PROPERTIES QT_RESOURCE_ALIAS LogSidebar.qml
)
set_source_files_properties(
    qml/app/components/PreviewPane.qml
    PROPERTIES QT_RESOURCE_ALIAS PreviewPane.qml
)
set_source_files_properties(
    qml/app/components/GraphicsSettingsForm.qml
    PROPERTIES QT_RESOURCE_ALIAS GraphicsSettingsForm.qml
)

qt_add_qml_module(SpellCircle
    URI SpellCircle.App
    VERSION 1.0
    QML_FILES
        qml/app/Main.qml
        qml/app/SettingsWindow.qml
        qml/app/components/LogSidebar.qml
        qml/app/components/PreviewPane.qml
        qml/app/components/GraphicsSettingsForm.qml
)

set_target_properties(SpellCircle PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER "dev.sigil.SpellCircle"
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    # WIN32_EXECUTABLE TRUE
)

target_link_libraries(SpellCircle
    PRIVATE
      Qt6::Quick
      Qt6::QuickControls2
      Qt6::QuickLayouts
      Qt6::CanvasPainter
      spdlog::spdlog
      SpellCircleCanvas
      SpellCircleModels
)

if(APPLE)
  target_link_libraries(SpellCircle PRIVATE SpellCircleAppNap)
endif()
