qt_add_qml_module(SpellCircleCanvas
    URI SpellCircle.Canvas
    VERSION 1.0
    # QML_FILES
    #   SpellCircle.qml
    SOURCES
      SpellCircle.cpp SpellCircle.h
      SpellCircleRenderer.cpp SpellCircleRenderer.h
)

target_link_libraries(SpellCircleCanvas
    PUBLIC
      Qt6::Quick
      Qt6::CanvasPainter
    PRIVATE
      spdlog::spdlog
      SpellCircleNetwork
)

target_include_directories(SpellCircleCanvas PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}
)
