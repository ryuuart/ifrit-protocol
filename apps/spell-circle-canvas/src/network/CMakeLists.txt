# Wire transport only: the UDP receiver, the FlatBuffers schema
# (SpellCircle.fbs), and its committed generated header. The decoded scene
# model, configuration, and QML-facing model types live in src/model.
#
# SpellCircle_generated.h is regenerated manually from the schema — run
# scripts/regen_flatbuffers.sh from the repository root after editing
# SpellCircle.fbs.
qt_add_library(SpellCircleNetwork SHARED
  NetworkManager.cpp include/NetworkManager.h
)

target_link_libraries(SpellCircleNetwork
    PUBLIC
      Qt6::Network
      Qt6::Gui
    PRIVATE
      spdlog::spdlog
      flatbuffers::flatbuffers
      SpellCircleNet
)

target_include_directories(SpellCircleNetwork
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)
