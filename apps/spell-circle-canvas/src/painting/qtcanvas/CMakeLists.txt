add_library(QtCanvasTextPath STATIC
  QTextPathPainter.cpp
  include/QTextPathPainter.h
)

target_include_directories(QtCanvasTextPath
  PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
)

target_link_libraries(QtCanvasTextPath
  PUBLIC
    Qt6::CanvasPainter
    Qt6::Gui
)
