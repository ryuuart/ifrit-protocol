qt_add_library(SpellCircleNetwork SHARED
  NetworkManager.cpp include/NetworkManager.h
  FeedModel.cpp include/FeedModel.h
)

target_link_libraries(SpellCircleNetwork
    PUBLIC
      Qt6::Network
    PRIVATE
      spdlog::spdlog
      flatbuffers::flatbuffers
)

target_include_directories(SpellCircleNetwork
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

qt_add_qml_module(SpellCircleModels
    URI SpellCircle.Models
    VERSION 1.0
    SOURCES
      Models.cpp include/Models.h
)

target_link_libraries(SpellCircleModels
  PUBLIC
    SpellCircleNetwork
  PRIVATE
    spdlog::spdlog
)
