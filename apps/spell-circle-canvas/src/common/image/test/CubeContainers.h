#pragma once

/** @file
 * Six solid cube faces written into the three containers a cube map
 * ships in — an uncompressed RGBA8 DDS, a KTX 1 and a KTX 2 — byte by
 * byte from their published layouts, so a test owns its fixture and no
 * committed binary has to be trusted. Faces are in the +x -x +y -y +z
 * -z order every container names them in.
 */

#include <include/core/SkColor.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace sigil::image::test {

/** Six faces of one colour each, @p edge texels square. */
using CubeFaces = std::array<SkColor, 6>;

namespace detail {

inline void put32(std::vector<std::byte>& out, uint32_t v) {
  for (int i = 0; i < 4; ++i) out.push_back((std::byte)((v >> (8 * i)) & 0xFF));
}
inline void put64(std::vector<std::byte>& out, uint64_t v) {
  for (int i = 0; i < 8; ++i) out.push_back((std::byte)((v >> (8 * i)) & 0xFF));
}
/** One face as R, G, B, A bytes, row after row. */
inline void putFaceRgba(std::vector<std::byte>& out, SkColor c, int edge) {
  for (int i = 0; i < edge * edge; ++i) {
    out.push_back((std::byte)SkColorGetR(c));
    out.push_back((std::byte)SkColorGetG(c));
    out.push_back((std::byte)SkColorGetB(c));
    out.push_back((std::byte)SkColorGetA(c));
  }
}

}  // namespace detail

/** An uncompressed A8R8G8B8 DDS holding six faces and no mip chain: the
 *  magic, the 124-byte header with the cube-map caps, then each face's
 *  texels as a little-endian 0xAARRGGBB word. */
inline std::vector<std::byte> cubeDds(const CubeFaces& faces, int edge) {
  using detail::put32;
  std::vector<std::byte> out;
  for (char c : {'D', 'D', 'S', ' '}) out.push_back((std::byte)c);
  put32(out, 124);                                // dwSize
  put32(out, 0x1 | 0x2 | 0x4 | 0x8 | 0x1000);     // caps, height, width, pitch, pixelformat
  put32(out, (uint32_t)edge);                     // dwHeight
  put32(out, (uint32_t)edge);                     // dwWidth
  put32(out, (uint32_t)edge * 4);                 // dwPitchOrLinearSize
  put32(out, 0);                                  // dwDepth
  put32(out, 0);                                  // dwMipMapCount
  for (int i = 0; i < 11; ++i) put32(out, 0);     // dwReserved1
  put32(out, 32);                                 // ddspf.dwSize
  put32(out, 0x1 | 0x40);                         // alpha pixels, rgb
  put32(out, 0);                                  // dwFourCC
  put32(out, 32);                                 // dwRGBBitCount
  put32(out, 0x00FF0000);                         // R mask
  put32(out, 0x0000FF00);                         // G mask
  put32(out, 0x000000FF);                         // B mask
  put32(out, 0xFF000000);                         // A mask
  put32(out, 0x8 | 0x1000);                       // dwCaps: complex, texture
  put32(out, 0x200 | 0x400 | 0x800 | 0x1000 | 0x2000 | 0x4000 | 0x8000);  // cube, all six
  put32(out, 0);                                  // dwCaps3
  put32(out, 0);                                  // dwCaps4
  put32(out, 0);                                  // dwReserved2
  for (SkColor c : faces)
    for (int i = 0; i < edge * edge; ++i)
      put32(out, ((uint32_t)SkColorGetA(c) << 24) | ((uint32_t)SkColorGetR(c) << 16) |
                     ((uint32_t)SkColorGetG(c) << 8) | (uint32_t)SkColorGetB(c));
  return out;
}

/** A KTX 1 holding six RGBA8 faces at one level: the identifier, the
 *  thirteen header words, no key-value data, one imageSize word, then
 *  the faces (RGBA8 rows are already four-byte aligned, so no padding). */
inline std::vector<std::byte> cubeKtx1(const CubeFaces& faces, int edge) {
  using detail::put32;
  std::vector<std::byte> out;
  for (uint8_t b : {0xAB, 0x4B, 0x54, 0x58, 0x20, 0x31, 0x31, 0xBB, 0x0D, 0x0A,
                    0x1A, 0x0A})
    out.push_back((std::byte)b);
  put32(out, 0x04030201);        // endianness: little
  put32(out, 0x1401);            // glType GL_UNSIGNED_BYTE
  put32(out, 1);                 // glTypeSize
  put32(out, 0x1908);            // glFormat GL_RGBA
  put32(out, 0x8058);            // glInternalFormat GL_RGBA8
  put32(out, 0x1908);            // glBaseInternalFormat GL_RGBA
  put32(out, (uint32_t)edge);    // pixelWidth
  put32(out, (uint32_t)edge);    // pixelHeight
  put32(out, 0);                 // pixelDepth
  put32(out, 0);                 // numberOfArrayElements
  put32(out, 6);                 // numberOfFaces
  put32(out, 1);                 // numberOfMipmapLevels
  put32(out, 0);                 // bytesOfKeyValueData
  put32(out, (uint32_t)(edge * edge * 4));  // imageSize: one face
  for (SkColor c : faces) detail::putFaceRgba(out, c, edge);
  return out;
}

/** A KTX 2 holding six R8G8B8A8_UNORM faces at one level: the
 *  identifier, the nine header words, the index (no dfd, kvd or sgd),
 *  a one-entry level table pointing past itself, then the faces packed. */
inline std::vector<std::byte> cubeKtx2(const CubeFaces& faces, int edge) {
  using detail::put32;
  using detail::put64;
  std::vector<std::byte> out;
  for (uint8_t b : {0xAB, 0x4B, 0x54, 0x58, 0x20, 0x32, 0x30, 0xBB, 0x0D, 0x0A,
                    0x1A, 0x0A})
    out.push_back((std::byte)b);
  put32(out, 37);                // vkFormat VK_FORMAT_R8G8B8A8_UNORM
  put32(out, 1);                 // typeSize
  put32(out, (uint32_t)edge);    // pixelWidth
  put32(out, (uint32_t)edge);    // pixelHeight
  put32(out, 0);                 // pixelDepth
  put32(out, 0);                 // layerCount
  put32(out, 6);                 // faceCount
  put32(out, 1);                 // levelCount
  put32(out, 0);                 // supercompressionScheme: none
  const uint64_t levelBytes = (uint64_t)edge * edge * 4 * 6;
  const uint64_t dataOffset = 12 + 9 * 4 + 4 * 4 + 2 * 8 + 3 * 8;
  put32(out, 0);                 // dfdByteOffset
  put32(out, 0);                 // dfdByteLength
  put32(out, 0);                 // kvdByteOffset
  put32(out, 0);                 // kvdByteLength
  put64(out, 0);                 // sgdByteOffset
  put64(out, 0);                 // sgdByteLength
  put64(out, dataOffset);        // level 0 byteOffset
  put64(out, levelBytes);        // level 0 byteLength
  put64(out, levelBytes);        // level 0 uncompressedByteLength
  for (SkColor c : faces) detail::putFaceRgba(out, c, edge);
  return out;
}

}  // namespace sigil::image::test
