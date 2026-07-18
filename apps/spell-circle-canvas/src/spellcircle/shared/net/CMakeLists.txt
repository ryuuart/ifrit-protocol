# The Qt-free transport core: one ASIO-based UDP receiver shared by every
# receiver frontend (the Qt SpellCircleNetwork wrapper and the native macOS
# SpellCircleMacBridge). Keep this library free of Qt, AppKit, and app
# concerns — it is pure datagram transport.
find_package(asio CONFIG REQUIRED)

add_library(SpellCircleNet STATIC
  UdpReceiver.cpp include/UdpReceiver.h
)

target_include_directories(SpellCircleNet
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(SpellCircleNet PUBLIC asio::asio)
# Standalone mode: no Boost — both apps embed this without extra deps.
target_compile_definitions(SpellCircleNet PUBLIC ASIO_STANDALONE)
