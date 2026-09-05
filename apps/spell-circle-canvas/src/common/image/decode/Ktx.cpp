/** @file
 * The KTX backend: a KTX 1 or KTX 2 container read directly from its
 * header and level table — the base level of a 2D image, or the six
 * faces of a cube map stacked into one column — as ChannelData, and the
 * probe over the same header. Uncompressed texel formats only: a
 * block-compressed or supercompressed file is refused, since a texture
 * an engine decodes on the device is not an image this library can
 * hand to a canvas.
 */

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

#include "Backends.h"

namespace sigil::image::backend {

namespace {

constexpr std::array<uint8_t, 12> kKtx1Identifier = {
    0xAB, 0x4B, 0x54, 0x58, 0x20, 0x31, 0x31, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A};
constexpr std::array<uint8_t, 12> kKtx2Identifier = {
    0xAB, 0x4B, 0x54, 0x58, 0x20, 0x32, 0x30, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A};

bool startsWith(const std::byte* bytes, size_t size,
                const std::array<uint8_t, 12>& identifier) {
  return size >= identifier.size() &&
         std::memcmp(bytes, identifier.data(), identifier.size()) == 0;
}

/** A little-endian reader over the file's bytes that refuses to read past
 *  the end rather than trusting the header's own sizes. */
class Reader {
 public:
  Reader(const std::byte* bytes, size_t size) : m_bytes(bytes), m_size(size) {}
  bool ok() const { return m_ok; }
  size_t position() const { return m_at; }
  void seek(size_t at) {
    if (at > m_size) m_ok = false;
    m_at = std::min(at, m_size);
  }
  uint32_t u32() {
    uint32_t v = 0;
    take(&v, sizeof v);
    return v;
  }
  uint64_t u64() {
    uint64_t v = 0;
    take(&v, sizeof v);
    return v;
  }
  /** @p count bytes at the current position, or null when they are not
   *  all inside the file. Does not advance. */
  const std::byte* view(size_t count) const {
    if (count > m_size - m_at) return nullptr;
    return m_bytes + m_at;
  }

 private:
  void take(void* out, size_t count) {
    if (count > m_size - m_at) {
      m_ok = false;
      m_at = m_size;
      return;
    }
    std::memcpy(out, m_bytes + m_at, count);
    m_at += count;
  }
  const std::byte* m_bytes;
  size_t m_size;
  size_t m_at = 0;
  bool m_ok = true;
};

/** How one texel is laid out: the channel count and order, and the
 *  number type. Every format this reader takes is one of these. */
struct Texel {
  enum class Type { Unorm8, Half, Float };
  Type type = Type::Unorm8;
  int channels = 0;
  bool bgr = false;  // blue first in memory, as the B8G8R8[A8] formats are
  size_t bytes() const {
    const size_t per = type == Type::Unorm8 ? 1 : type == Type::Half ? 2 : 4;
    return per * (size_t)channels;
  }
};

/** KTX 1 names its texel by OpenGL's type and format enums. */
std::optional<Texel> texelOfGl(uint32_t glType, uint32_t glFormat) {
  Texel t;
  switch (glType) {
    case 0x1401: t.type = Texel::Type::Unorm8; break;  // GL_UNSIGNED_BYTE
    case 0x140B: t.type = Texel::Type::Half; break;    // GL_HALF_FLOAT
    case 0x1406: t.type = Texel::Type::Float; break;   // GL_FLOAT
    default: return std::nullopt;  // a packed or compressed type
  }
  switch (glFormat) {
    case 0x1903:  // GL_RED
    case 0x1909:  // GL_LUMINANCE
    case 0x1906:  // GL_ALPHA
      t.channels = 1;
      break;
    case 0x8227: t.channels = 2; break;  // GL_RG
    case 0x1907: t.channels = 3; break;  // GL_RGB
    case 0x1908: t.channels = 4; break;  // GL_RGBA
    case 0x80E0: t.channels = 3; t.bgr = true; break;  // GL_BGR
    case 0x80E1: t.channels = 4; t.bgr = true; break;  // GL_BGRA
    default: return std::nullopt;
  }
  return t;
}

/** KTX 2 names its texel by Vulkan's format enum. */
std::optional<Texel> texelOfVk(uint32_t vkFormat) {
  struct Entry {
    uint32_t format;
    Texel texel;
  };
  static constexpr Entry kTable[] = {
      {9, {Texel::Type::Unorm8, 1, false}},     // R8_UNORM
      {15, {Texel::Type::Unorm8, 1, false}},    // R8_SRGB
      {16, {Texel::Type::Unorm8, 2, false}},    // R8G8_UNORM
      {22, {Texel::Type::Unorm8, 2, false}},    // R8G8_SRGB
      {23, {Texel::Type::Unorm8, 3, false}},    // R8G8B8_UNORM
      {29, {Texel::Type::Unorm8, 3, false}},    // R8G8B8_SRGB
      {30, {Texel::Type::Unorm8, 3, true}},     // B8G8R8_UNORM
      {36, {Texel::Type::Unorm8, 3, true}},     // B8G8R8_SRGB
      {37, {Texel::Type::Unorm8, 4, false}},    // R8G8B8A8_UNORM
      {43, {Texel::Type::Unorm8, 4, false}},    // R8G8B8A8_SRGB
      {44, {Texel::Type::Unorm8, 4, true}},     // B8G8R8A8_UNORM
      {50, {Texel::Type::Unorm8, 4, true}},     // B8G8R8A8_SRGB
      {76, {Texel::Type::Half, 1, false}},      // R16_SFLOAT
      {83, {Texel::Type::Half, 2, false}},      // R16G16_SFLOAT
      {90, {Texel::Type::Half, 3, false}},      // R16G16B16_SFLOAT
      {97, {Texel::Type::Half, 4, false}},      // R16G16B16A16_SFLOAT
      {100, {Texel::Type::Float, 1, false}},    // R32_SFLOAT
      {103, {Texel::Type::Float, 2, false}},    // R32G32_SFLOAT
      {106, {Texel::Type::Float, 3, false}},    // R32G32B32_SFLOAT
      {109, {Texel::Type::Float, 4, false}},    // R32G32B32A32_SFLOAT
  };
  for (const Entry& entry : kTable)
    if (entry.format == vkFormat) return entry.texel;
  return std::nullopt;
}

/** IEEE half to float, the two special exponents included. */
float halfToFloat(uint16_t h) {
  const uint32_t sign = (uint32_t)(h & 0x8000u) << 16;
  const uint32_t exponent = (h >> 10) & 0x1Fu;
  const uint32_t mantissa = h & 0x3FFu;
  uint32_t bits;
  if (exponent == 0) {
    if (mantissa == 0) {
      bits = sign;
    } else {  // subnormal: renormalise
      uint32_t m = mantissa;
      int e = -1;
      do {
        ++e;
        m <<= 1;
      } while ((m & 0x400u) == 0);
      bits = sign | ((uint32_t)(127 - 15 - e) << 23) | ((m & 0x3FFu) << 13);
    }
  } else if (exponent == 0x1F) {
    bits = sign | 0x7F800000u | (mantissa << 13);
  } else {
    bits = sign | ((exponent + 127 - 15) << 23) | (mantissa << 13);
  }
  float f;
  std::memcpy(&f, &bits, sizeof f);
  return f;
}

/** The base level and how it is laid out: the faces stacked into one
 *  column when there are six, so the value that reads a cube sheet by
 *  its aspect ratio takes it as it takes the same file through any
 *  other reader. Rows are @p rowBytes apart and faces @p faceBytes. */
struct Level {
  int width = 0;
  int height = 0;
  int faces = 1;
  Texel texel;
  size_t rowBytes = 0;
  size_t faceBytes = 0;
  const std::byte* data = nullptr;
};

ChannelData channelsOf(const Level& level) {
  ChannelData out;
  out.width = level.width;
  out.height = level.height * level.faces;
  out.floatingPoint = level.texel.type != Texel::Type::Unorm8;
  static constexpr const char* kNames[4] = {"R", "G", "B", "A"};
  for (int c = 0; c < level.texel.channels; ++c) out.names.push_back(kNames[c]);
  const size_t channels = (size_t)level.texel.channels;
  out.data.resize((size_t)out.width * out.height * channels);
  float* dst = out.data.data();
  for (int face = 0; face < level.faces; ++face) {
    for (int y = 0; y < level.height; ++y) {
      const std::byte* row =
          level.data + (size_t)face * level.faceBytes + (size_t)y * level.rowBytes;
      for (int x = 0; x < level.width; ++x) {
        const std::byte* px = row + (size_t)x * level.texel.bytes();
        float value[4] = {0, 0, 0, 1};
        for (size_t c = 0; c < channels; ++c) {
          switch (level.texel.type) {
            case Texel::Type::Unorm8:
              value[c] = (float)(uint8_t)px[c] / 255.0f;
              break;
            case Texel::Type::Half: {
              uint16_t h;
              std::memcpy(&h, px + c * 2, 2);
              value[c] = halfToFloat(h);
              break;
            }
            case Texel::Type::Float:
              std::memcpy(&value[c], px + c * 4, 4);
              break;
          }
        }
        if (level.texel.bgr) std::swap(value[0], value[2]);
        // An 8-bit texel is straight alpha; the LDR contract is
        // premultiplied, as the Skia codecs deliver it.
        if (level.texel.type == Texel::Type::Unorm8 && channels == 4)
          for (size_t c = 0; c < 3; ++c) value[c] *= value[3];
        for (size_t c = 0; c < channels; ++c) *dst++ = value[c];
      }
    }
  }
  return out;
}

/** The base level of a KTX 1 file. Each face is followed by padding to
 *  four bytes and rows are aligned to four bytes, as the container
 *  states; only the first array element and depth slice are read. */
std::optional<Level> ktx1BaseLevel(const std::byte* bytes, size_t size) {
  Reader in(bytes, size);
  in.seek(kKtx1Identifier.size());
  const uint32_t endianness = in.u32();
  if (endianness != 0x04030201u) return std::nullopt;  // big-endian: not taken
  const uint32_t glType = in.u32();
  (void)in.u32();  // glTypeSize
  const uint32_t glFormat = in.u32();
  (void)in.u32();  // glInternalFormat
  (void)in.u32();  // glBaseInternalFormat
  const uint32_t width = in.u32();
  const uint32_t height = in.u32();
  const uint32_t depth = in.u32();
  const uint32_t arrayElements = in.u32();
  const uint32_t faces = in.u32();
  (void)in.u32();  // numberOfMipmapLevels
  const uint32_t keyValueBytes = in.u32();
  if (!in.ok()) return std::nullopt;
  const std::optional<Texel> texel = texelOfGl(glType, glFormat);
  if (!texel || width == 0 || height == 0 || depth > 1 || arrayElements > 1 ||
      (faces != 1 && faces != 6))
    return std::nullopt;
  in.seek(in.position() + keyValueBytes);
  const uint32_t imageSize = in.u32();
  if (!in.ok()) return std::nullopt;
  Level level;
  level.width = (int)width;
  level.height = (int)height;
  level.faces = (int)faces;
  level.texel = *texel;
  level.rowBytes = ((size_t)width * texel->bytes() + 3) & ~(size_t)3;
  const size_t faceBytes = level.rowBytes * height;
  if (imageSize < faceBytes) return std::nullopt;
  level.faceBytes = (faceBytes + 3) & ~(size_t)3;  // cubePadding
  const size_t needed = level.faceBytes * (faces - 1) + faceBytes;
  level.data = in.view(needed);
  if (!level.data) return std::nullopt;
  return level;
}

/** The base level of a KTX 2 file: found through the level index, faces
 *  packed one after another with no padding, rows unpadded. A
 *  supercompressed file (Basis, zstd, zlib) is refused. */
std::optional<Level> ktx2BaseLevel(const std::byte* bytes, size_t size) {
  Reader in(bytes, size);
  in.seek(kKtx2Identifier.size());
  const uint32_t vkFormat = in.u32();
  (void)in.u32();  // typeSize
  const uint32_t width = in.u32();
  const uint32_t height = in.u32();
  const uint32_t depth = in.u32();
  const uint32_t layers = in.u32();
  const uint32_t faces = in.u32();
  const uint32_t levels = in.u32();
  const uint32_t supercompression = in.u32();
  if (!in.ok()) return std::nullopt;
  const std::optional<Texel> texel = texelOfVk(vkFormat);
  if (!texel || width == 0 || height == 0 || depth > 1 || layers > 1 ||
      (faces != 1 && faces != 6) || supercompression != 0)
    return std::nullopt;
  // The index: dfd, kvd and sgd offsets and lengths, then the level table
  // whose first entry is the base level.
  in.seek(in.position() + 4 * 4 + 2 * 8);
  (void)levels;  // 0 means one level and no chain; the base is entry 0 either way
  const uint64_t byteOffset = in.u64();
  const uint64_t byteLength = in.u64();
  (void)in.u64();  // uncompressedByteLength
  if (!in.ok()) return std::nullopt;
  Level level;
  level.width = (int)width;
  level.height = (int)height;
  level.faces = (int)faces;
  level.texel = *texel;
  level.rowBytes = (size_t)width * texel->bytes();
  level.faceBytes = level.rowBytes * height;
  const size_t needed = level.faceBytes * faces;
  if (byteLength < needed || byteOffset > size) return std::nullopt;
  in.seek((size_t)byteOffset);
  level.data = in.view(needed);
  if (!level.data) return std::nullopt;
  return level;
}

std::optional<Level> baseLevel(const std::byte* bytes, size_t size) {
  if (startsWith(bytes, size, kKtx1Identifier)) return ktx1BaseLevel(bytes, size);
  if (startsWith(bytes, size, kKtx2Identifier)) return ktx2BaseLevel(bytes, size);
  return std::nullopt;
}

}  // namespace

bool looksLikeKtx(const std::byte* bytes, size_t size) {
  return startsWith(bytes, size, kKtx1Identifier) ||
         startsWith(bytes, size, kKtx2Identifier);
}

std::optional<ChannelData> decodeChannelsWithKtx(const std::byte* bytes,
                                                 size_t size) {
  const std::optional<Level> level = baseLevel(bytes, size);
  if (!level) return std::nullopt;
  return channelsOf(*level);
}

std::optional<ImageProbe> probeWithKtx(const std::byte* bytes, size_t size) {
  const std::optional<Level> level = baseLevel(bytes, size);
  if (!level) return std::nullopt;
  ImageProbe info;
  info.format = startsWith(bytes, size, kKtx1Identifier) ? "ktx" : "ktx2";
  info.width = level->width;
  info.height = level->height * level->faces;
  info.channels = level->texel.channels;
  info.floatingPoint = level->texel.type != Texel::Type::Unorm8;
  static constexpr const char* kNames[4] = {"R", "G", "B", "A"};
  for (int c = 0; c < level->texel.channels; ++c)
    info.channelNames.push_back(kNames[c]);
  return info;
}

}  // namespace sigil::image::backend
