/** @file
 * The recording format: the line a file opens with, the frame one
 * arrival is written as, and the read that answers every whole frame
 * the file holds.
 */

#include "sigilio/hub/Recording.h"

#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <string_view>

#include "sigilio/source/Source.h"

namespace sigil::io {

namespace {

/** The line every recording opens with. A file that does not is not one
 *  of these, whatever it is named. */
constexpr std::string_view kHeader = "sigil-feed-recording 1\n";

}  // namespace

RecordingWriter::RecordingWriter(const std::filesystem::path& path)
    : m_stream(path, std::ios::binary | std::ios::trunc) {
  if (m_stream) m_stream.write(kHeader.data(), (std::streamsize)kHeader.size());
}

bool RecordingWriter::append(const Arrival& arrival) {
  if (!m_stream) return false;
  const size_t size = arrival.bytes ? arrival.bytes->bytes.size() : 0;
  // A frame states its length in 32 bits. A message that does not fit
  // is not written at all: a length that had been cut down to size
  // would make every frame after it unreadable.
  if (size > std::numeric_limits<uint32_t>::max()) return false;
  const uint32_t length = (uint32_t)size;
  m_stream.write(reinterpret_cast<const char*>(&arrival.at), sizeof(double));
  m_stream.write(reinterpret_cast<const char*>(&length), sizeof(length));
  if (length)
    m_stream.write(reinterpret_cast<const char*>(arrival.bytes->bytes.data()),
                   (std::streamsize)length);
  // Every frame is on the disk by the time this answers: a recording is
  // read while it is still being written, and by the run that follows
  // one that was killed.
  m_stream.flush();
  return (bool)m_stream;
}

bool RecordingWriter::good() const { return m_stream.good(); }

std::optional<std::vector<Arrival>> readRecording(
    const std::filesystem::path& path) {
  const std::optional<Bytes> file = readBytes(path);
  if (!file) return std::nullopt;
  const std::span<const std::byte> bytes(file->bytes);
  if (bytes.size() < kHeader.size() ||
      std::memcmp(bytes.data(), kHeader.data(), kHeader.size()) != 0)
    return std::nullopt;

  std::vector<Arrival> arrivals;
  size_t offset = kHeader.size();
  // A frame is its time, its length, and that many bytes. The first one
  // that is not all there ends the read: a writer killed partway
  // through a frame leaves every whole frame before it readable.
  while (offset + sizeof(double) + sizeof(uint32_t) <= bytes.size()) {
    double at = 0;
    std::memcpy(&at, bytes.data() + offset, sizeof(at));
    offset += sizeof(at);
    uint32_t length = 0;
    std::memcpy(&length, bytes.data() + offset, sizeof(length));
    offset += sizeof(length);
    if (bytes.size() - offset < length) break;
    auto message = std::make_shared<Bytes>();
    message->bytes.assign(bytes.begin() + (ptrdiff_t)offset,
                          bytes.begin() + (ptrdiff_t)(offset + length));
    offset += length;
    // The generation counts what one FEED has taken, which is not the
    // recording's to know, so the frames are numbered as they are read.
    arrivals.push_back({arrivals.size() + 1, at, std::move(message)});
  }
  return arrivals;
}

}  // namespace sigil::io
