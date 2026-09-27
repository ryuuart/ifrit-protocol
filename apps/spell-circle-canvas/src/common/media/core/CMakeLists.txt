# The core leaf: the values every consumer of pictures holds — one Frame,
# the Timing it is asked at, the decoded Image document, Metadata, the
# one Format list and the filename question, the device frame and the
# one call that binds it, the generic decode door, and PixelSource, the
# one seam pixels cross into another library. Skia only: nothing
# here reads a codec, so a consumer that draws pictures somebody else
# decoded links this alone. Its headers speak Picture and glm sizes; Skia
# is named only in advanced/Skia.h, the one door a caller includes by name.
find_package(unofficial-skia CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
sigil_library(SigilMediaCore
  SOURCES Device.cpp Formats.cpp Image.cpp Picture.cpp PixelSource.cpp Timing.cpp
  HEADERS Decode.h Format.h Frame.h Image.h Metadata.h Picture.h PixelSource.h
          ../advanced/Device.h ../advanced/Formats.h ../advanced/Skia.h
          ../advanced/Source.h
  PUBLIC unofficial::skia::skia glm::glm)

sigil_test(media_test
  SOURCES test/ImageTest.cpp test/PixelSourceTest.cpp
  LIBRARIES SigilMediaCore)
