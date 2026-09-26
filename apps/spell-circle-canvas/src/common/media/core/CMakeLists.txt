# The core leaf: the values every consumer of pictures holds — one Frame,
# the Timing it is asked at, the decoded Image document, Metadata, the
# one Format list and the filename question, the device frame and the
# one call that binds it, and the generic decode door. Skia only: nothing
# here reads a codec, so a consumer that draws pictures somebody else
# decoded links this alone.
find_package(unofficial-skia CONFIG REQUIRED)
sigil_library(SigilMediaCore
  SOURCES Device.cpp Formats.cpp Image.cpp Timing.cpp
  HEADERS Decode.h Format.h Frame.h Image.h Metadata.h
          ../advanced/Device.h ../advanced/Formats.h
  PUBLIC unofficial::skia::skia)

sigil_test(media_test
  SOURCES test/ImageTest.cpp
  LIBRARIES SigilMediaCore)
