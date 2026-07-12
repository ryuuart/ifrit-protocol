add_subdirectory(network)
add_subdirectory(platform)
add_subdirectory(text)
add_subdirectory(qml)

qt_add_executable(SpellCircle
    main.cpp
)

qt_add_qml_module(SpellCircle
    URI qtquick1
    VERSION 1.0
    QML_FILES
        Main.qml
        SquareButton.qml
        SettingsWindow.qml
        FontFamilyField.qml
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
