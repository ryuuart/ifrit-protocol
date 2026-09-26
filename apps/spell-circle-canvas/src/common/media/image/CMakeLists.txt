# The image sub-library: stills and animations decoded whole, their float
# planes, and pixels encoded back out as a still format. Decode and encode
# stand beside each other, so a consumer that only writes pictures pulls
# in no codec it will not call.
add_subdirectory(decode)
add_subdirectory(encode)

sigil_library(SigilMediaImage
  INTERFACE SigilMediaImageDecode SigilMediaImageEncode)
