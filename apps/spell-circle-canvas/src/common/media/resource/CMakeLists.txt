# The resource leaf: this library on a hub. `registerDecoders` is a
# template over the hub, so nothing here links a resource library; the
# probe a hub asks tries the image route and then a video's container,
# which is why this stands above both sub-libraries rather than in either.
sigil_library(SigilMediaResource
  SOURCES Resource.cpp
  HEADERS ../advanced/Resource.h
  PUBLIC SigilMediaImageDecode SigilMediaVideoDecode)
