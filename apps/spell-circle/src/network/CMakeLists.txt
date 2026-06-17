qt_add_library(SpellCircleNetwork STATIC
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
