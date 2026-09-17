# BOTH ENDS OF ONE DOOR: one abstract publisher over the graphics API's
# own texture and command-buffer handles, one abstract subscription over
# the same kind of handle coming the other way, and one factory each that
# answers with whatever this build can carry a frame over.
#
# IT KNOWS NO WINDOW, NO TOOLKIT AND NO DRAWING. The handles cross the
# seam as opaque pointers, so a host that owns its device and a host
# drawing inside someone else's window reach the same publisher, and the
# headers compile wherever this repository compiles. Syphon is the macOS
# protocol at both ends; Spout supplies Direct3D11 publication where its
# SDK is configured. Unsupported combinations return null.
set(SIGIL_PUBLISH_SOURCES Publisher.cpp Subscription.cpp)
if(APPLE)
  find_package(unofficial-syphon CONFIG REQUIRED)
  list(APPEND SIGIL_PUBLISH_SOURCES
       SyphonPublisher.mm SyphonPublisher.h
       SyphonSubscription.mm SyphonSubscription.h)
elseif(WIN32)
  find_package(Spout2 CONFIG QUIET)
  if(TARGET Spout2::SpoutDX)
    list(APPEND SIGIL_PUBLISH_SOURCES SpoutPublisher.cpp SpoutPublisher.h)
  endif()
endif()

sigil_library(SigilIOPublish
  SOURCES ${SIGIL_PUBLISH_SOURCES}
  HEADERS Publisher.h Subscription.h
  FRAMEWORKS Foundation Metal)

# The server owns the reference alloc/init handed it and releases it
# explicitly, and so does the client behind the subscription — which owns
# every frame it hands out until the next one is asked for — so this
# target is deliberately not compiled with automatic reference counting.
if(APPLE)
  target_link_libraries(SigilIOPublish PRIVATE unofficial::syphon::syphon)
elseif(TARGET Spout2::SpoutDX)
  target_link_libraries(SigilIOPublish PRIVATE Spout2::SpoutDX d3d11)
  target_compile_definitions(SigilIOPublish PRIVATE SIGIL_PUBLISH_SPOUT)
endif()

# Factory refusals join the resource library's test binary.
sigil_test(io_test
  SOURCES test/PublisherTest.cpp test/SubscriptionTest.cpp
  LIBRARIES SigilIOPublish)
