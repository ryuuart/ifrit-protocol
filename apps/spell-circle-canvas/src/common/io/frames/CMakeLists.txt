# BOTH ENDS OF ONE DOOR: one abstract publisher over the graphics API's
# own texture and command-buffer handles, one abstract subscription over
# the same kind of handle coming the other way, and the hub's publish()
# and subscribe(), defined here, which answer with whatever this build can
# carry a frame over.
#
# IT KNOWS NO WINDOW AND NO TOOLKIT. The handles cross the seam as opaque
# pointers, so a host that owns its device and a host drawing inside
# someone else's window reach the same publisher, and the headers compile
# wherever this repository compiles; the one drawing done here turns an
# arrived frame the right way up. Syphon is the macOS
# protocol at both ends; Spout supplies Direct3D11 publication where its
# SDK is configured. Unsupported combinations return null.
set(SIGIL_FRAMES_SOURCES Publisher.cpp Subscription.cpp Protocol.h Received.h)
if(APPLE)
  find_package(unofficial-syphon CONFIG REQUIRED)
  list(APPEND SIGIL_FRAMES_SOURCES
       SyphonPublisher.mm SyphonPublisher.h
       SyphonSubscription.mm SyphonSubscription.h Received.mm)
elseif(WIN32)
  find_package(Spout2 CONFIG QUIET)
  if(TARGET Spout2::SpoutDX)
    list(APPEND SIGIL_FRAMES_SOURCES SpoutPublisher.cpp SpoutPublisher.h)
  endif()
  list(APPEND SIGIL_FRAMES_SOURCES ReceivedStub.cpp)
else()
  list(APPEND SIGIL_FRAMES_SOURCES ReceivedStub.cpp)
endif()

# A subscription answers as a picture source, so the frame it answers is
# SigilMedia's (public), and the wrap and read-back that make an arrived
# texture a picture are SigilSkia's (private).
sigil_library(SigilIOFrames
  SOURCES ${SIGIL_FRAMES_SOURCES}
  HEADERS Frame.h Publisher.h Subscription.h
  PUBLIC SigilIOSource SigilIOHub SigilMediaCore
  FRAMEWORKS Foundation Metal)
if(APPLE)
  target_link_libraries(SigilIOFrames PRIVATE SigilSkiaGraphite)
endif()

# The server owns the reference alloc/init handed it and releases it
# explicitly, and so does the client behind the subscription — which owns
# every frame it hands out until the next one is asked for — so this
# target is deliberately not compiled with automatic reference counting.
if(APPLE)
  target_link_libraries(SigilIOFrames PRIVATE unofficial::syphon::syphon)
elseif(TARGET Spout2::SpoutDX)
  target_link_libraries(SigilIOFrames PRIVATE Spout2::SpoutDX d3d11)
  target_compile_definitions(SigilIOFrames PRIVATE SIGIL_FRAMES_SPOUT)
endif()

# The refusals of publish and subscribe join the resource library's test
# binary.
sigil_test(io_test
  SOURCES test/PublisherTest.cpp test/SubscriptionTest.cpp
  LIBRARIES SigilIOFrames)
