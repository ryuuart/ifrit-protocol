sigil_library_root(SigilPublish
  BRIEF "Native inter-application texture publication and subscription")

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

sigil_library(SigilPublish
  SOURCES ${SIGIL_PUBLISH_SOURCES}
  HEADERS Publisher.h Subscription.h
  FRAMEWORKS Foundation Metal)

# The server owns the reference alloc/init handed it and releases it
# explicitly, and so does the client behind the subscription — which owns
# every frame it hands out until the next one is asked for — so this
# target is deliberately not compiled with automatic reference counting.
if(APPLE)
  target_link_libraries(SigilPublish PRIVATE unofficial::syphon::syphon)
elseif(TARGET Spout2::SpoutDX)
  target_link_libraries(SigilPublish PRIVATE Spout2::SpoutDX d3d11)
  target_compile_definitions(SigilPublish PRIVATE SIGIL_PUBLISH_SPOUT)
endif()

# Factory refusals need no graphics device. Device-backed delivery and
# capture cases join this same test binary on macOS below.
sigil_test(publish_test
  SOURCES test/PublisherTest.cpp test/SubscriptionTest.cpp
  LIBRARIES SigilPublish)

# RECEIVER: THE OTHER SIDE OF THE SAME DOOR — the subscriber, so what a
# publisher offers can be looked at and written down without a third
# application in the middle. AppKit and Metal, no Qt and no sketch: it
# knows a name, a window and a file. It is an application rather than part
# of the archive above because nothing that publishes needs it, and it
# stands here rather than beside Sketchbook because the two ends of one
# protocol are one subject.
if(APPLE)
  sigil_frameworks(SIGIL_PUBLISH_RECEIVER_FRAMEWORKS
                   Foundation AppKit Metal MetalKit QuartzCore)
  # Every one of these talks to Objective-C and every one of them is
  # handed references it owns — a frame image arrives at +1 — so they are
  # compiled with automatic reference counting. The command line is plain
  # C++ and is left out of it.
  set(SIGIL_PUBLISH_RECEIVER_ARC_SOURCES
    tools/receiver/Capture.mm tools/receiver/Grab.mm tools/receiver/main.mm
    tools/receiver/Servers.mm tools/receiver/Window.mm)
  add_executable(Receiver MACOSX_BUNDLE
    ${SIGIL_PUBLISH_RECEIVER_ARC_SOURCES}
    tools/receiver/Arguments.cpp
    tools/receiver/Arguments.h tools/receiver/Capture.h tools/receiver/Grab.h
    tools/receiver/Servers.h tools/receiver/Window.h)
  set_source_files_properties(${SIGIL_PUBLISH_RECEIVER_ARC_SOURCES}
    TARGET_DIRECTORY Receiver PROPERTIES COMPILE_OPTIONS -fobjc-arc)
  # A real .app: a bare Mach-O gets no bundle identity, so macOS gives the
  # window the executable's name and treats activation as a plain process.
  # Every verb still works through the binary inside it
  # (bin/<config>/Receiver.app/Contents/MacOS).
  set_target_properties(Receiver PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER "dev.sigil.Receiver"
    MACOSX_BUNDLE_BUNDLE_NAME "Receiver"
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING
      ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE)
  target_include_directories(Receiver PRIVATE ${CMAKE_CURRENT_LIST_DIR})
  # THE SUBSCRIPTION IS THE ARCHIVE'S, not this application's: what it
  # means to hold a publication by its name is one subject, and the
  # window and the grab here are two readers of it. Beside it, the
  # encoder — a frame that has to become a file is pixels in and bytes
  # out, and where the bytes go is the byte sink's half. Syphon itself is
  # still named because the listing asks the directory directly.
  target_link_libraries(Receiver PRIVATE
    SigilPublish SigilImageEncode SigilIOSource
    unofficial::syphon::syphon ${SIGIL_PUBLISH_RECEIVER_FRAMEWORKS})

  # What the receiver can be asked with nothing publishing: the command
  # line, and the write, over a texture the case draws itself. The write
  # wants a device, so its suite carries the label a machine without one
  # leaves out.
  sigil_test(publish_test
    ARC
    SOURCES test/ReceiverTest.mm tools/receiver/Arguments.cpp tools/receiver/Capture.mm
    LIBRARIES SigilImageEncode SigilImageDecode SigilIOSource
              ${SIGIL_PUBLISH_RECEIVER_FRAMEWORKS}
    SUPPORT_DIRS ${CMAKE_CURRENT_LIST_DIR}/tools
    SUITES ReceiverCapture PublishDelivery LABELS gpu)
endif()

# The standalone seam owns its documentation and validation.
sigil_doc_probes(publish
  READMES ${CMAKE_CURRENT_SOURCE_DIR}/README.md
  FLOORS 3,2,7,6,0
  LIBRARIES SigilPublish)
