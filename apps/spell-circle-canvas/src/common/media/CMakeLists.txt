# SigilMedia — pictures, still and moving, as one library over two
# sub-libraries: the image sub-library (stills and animations decoded
# whole, their channel planes, still encoders) and the video sub-library
# (clips decoded around a playhead over FFmpeg, the decode pool, the movie
# encoder), over one core every consumer holds (Frame, Timing, Image,
# Metadata, Format, the device frame), with the coverage field and the
# pixel difference beside them. One flat `sigil::media` vocabulary; one
# feature library per directory, linked by what a consumer uses.
# `SigilMedia` is the umbrella: an interface over every feature.
#
# The committed fixtures every test and bench reads sit in test/assets/
# (one still per format plus a 3-frame red/green/blue animation for each
# animated format, regenerable with PIL + avifenc, and a VP8 clip with
# alpha) and reach a binary through the SIGIL_TEST_ASSET_DIR compile
# definition; the helpers the image cases read a fixture by are
# test/Pixels.h.
#
# The Skia codec set is guaranteed by the skia vcpkg features declared in
# vcpkg.json (png/jpeg/webp are port defaults; avif is opted in there).
# The port's avif feature carries libavif[dav1d], because a libavif built
# with no AV1 codec parses an AVIF container and decodes no frame out of
# it.
sigil_library_root(SigilMedia
  BRIEF "Pictures, still and moving: decode, encode, frames and the one pixel seam"
  DOCS ${CMAKE_CURRENT_SOURCE_DIR}/IMAGE.md
       ${CMAKE_CURRENT_SOURCE_DIR}/VIDEO.md)

# Dependencies point down this list.
add_subdirectory(core)
add_subdirectory(image)
add_subdirectory(video)
add_subdirectory(resource)
add_subdirectory(field)
add_subdirectory(difference)

sigil_library(SigilMedia
  INTERFACE SigilMediaImage SigilMediaVideo SigilMediaResource
            SigilMediaField SigilMediaDifference)

# Every API name the prose spells, compiled against the headers that own
# it. SigilIO is scanned and linked for the hub the walkthroughs load
# through and save to; the dependency names in the boundary section are
# CMake's and OpenImageIO's own, which no include root here carries.
sigil_doc_probes(media
  READMES ${CMAKE_CURRENT_SOURCE_DIR}/README.md
          ${CMAKE_CURRENT_SOURCE_DIR}/IMAGE.md
          ${CMAKE_CURRENT_SOURCE_DIR}/VIDEO.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/VALUES.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/functions/embeddedImages.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/types/Channels.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/types/DistanceField.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/types/Image.md
  INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/../io/include/sigilio
  PRELUDES "<sigilio/hub/Hub.h>"
           "<sigilio/advanced/Decoding.h>"
  EXCLUDE "unofficial::skia::skia=a CMake target name, not a C++ name"
          "material::Texture=SigilMaterial stands on this library, so a name its README gives a caller is compiled by SigilMaterial's probe"
          "compose::image=SigilCompose stands on this library, so a name its README gives a caller is compiled by SigilCompose's probe"
          "OIIO::ImageOutput=OpenImageIO's own, named as the dependency it is: its headers are not scanned"
          "OIIO::ImageInput=OpenImageIO's own, named as the dependency it is: its headers are not scanned"
  FLOORS 45,54,21,49,1
  LIBRARIES SigilMedia SigilIOHub)
