qt_add_qml_module(SpellCircleCanvas
    URI SpellCircle.Canvas
    VERSION 1.0
    # QML_FILES
    #   SpellCircle.qml
    SOURCES
      SyphonBridge.mm include/SyphonBridge.h
      SpellCircle.cpp include/SpellCircle.h
      SpellCircleRenderer.cpp include/SpellCircleRenderer.h
      TextPathPainter.cpp include/TextPathPainter.h
)

target_link_libraries(SpellCircleCanvas
    PUBLIC
      Qt6::Quick
      Qt6::CanvasPainter
    PRIVATE
      spdlog::spdlog
      SpellCircleNetwork
      Qt::GuiPrivate
      ${FOUNDATION_FRAMEWORK}
      ${SYPHON_FRAMEWORK}
      ${CORE_GRAPHICS_FRAMEWORK}
)

target_include_directories(SpellCircleCanvas PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/include
)
