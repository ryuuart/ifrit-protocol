add_library(SpellCircleNetwork STATIC
    NetworkManager.h NetworkManager.cpp
    FeedModel.h FeedModel.cpp
    SpellCircle_generated.h
)

target_link_libraries(SpellCircleNetwork
    PUBLIC
      Qt6::Network
    PRIVATE
      spdlog::spdlog
      flatbuffers::flatbuffers
)

target_include_directories(SpellCircleNetwork
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..
)
