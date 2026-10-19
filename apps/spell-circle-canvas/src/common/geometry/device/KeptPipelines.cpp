/** @file
 * Reading and writing the framed pipeline cache.
 */

#include "KeptPipelines.h"

#include <Common/interface/HashUtils.hpp>

#include <atomic>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <random>
#include <string>
#include <system_error>

namespace sigil::geometry::device {

namespace {

// workaround: the driver checks only that kept bytes name its own device
// and rebuilds every shader library they hold as the device is made, and
// under MoltenVK a library source Metal cannot accept aborts the process
// inside Metal rather than failing the cache. So the bytes the driver is
// handed are exactly the bytes a driver handed back: they are kept framed
// by a mark, their length and a hash of them, and a file whose frame does
// not hold is refused whole before the driver sees it.
struct Frame {
  char mark[8] = {'S', 'i', 'g', 'i', 'l', 'P', 'S', 'O'};
  std::uint64_t size = 0;
  std::uint64_t hash = 0;
};

std::uint64_t hashOf(std::span<const char> bytes) {
  return Diligent::ComputeHashRaw(bytes.data(), bytes.size());
}

}  // namespace

std::vector<char> readKeptPipelines(const std::filesystem::path& file) {
  std::vector<char> whole;
  if (std::ifstream in{file, std::ios::binary})
    whole.assign(std::istreambuf_iterator<char>(in),
                 std::istreambuf_iterator<char>());
  Frame frame;
  if (whole.size() < sizeof frame) return {};
  const Frame expected;
  std::memcpy(&frame, whole.data(), sizeof frame);
  if (std::memcmp(frame.mark, expected.mark, sizeof frame.mark) != 0 ||
      frame.size != whole.size() - sizeof frame)
    return {};
  std::vector<char> kept(whole.begin() + sizeof frame, whole.end());
  if (hashOf(kept) != frame.hash) return {};
  return kept;
}

bool writeKeptPipelines(const std::filesystem::path& file,
                        std::span<const char> bytes) {
  if (file.empty()) return false;
  std::error_code error;
  std::filesystem::create_directories(file.parent_path(), error);
  static const unsigned process = std::random_device{}();
  static std::atomic<unsigned> written{0};
  std::filesystem::path partial = file;
  partial += ".partial-" + std::to_string(process) + "-" +
             std::to_string(written.fetch_add(1));
  Frame frame;
  frame.size = bytes.size();
  frame.hash = hashOf(bytes);
  {
    std::ofstream out(partial, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out.write(reinterpret_cast<const char*>(&frame), sizeof frame);
    out.write(bytes.data(), (std::streamsize)bytes.size());
    if (!out) {
      out.close();
      std::filesystem::remove(partial, error);
      return false;
    }
  }
  std::filesystem::rename(partial, file, error);
  if (!error) return true;
  std::filesystem::remove(partial, error);
  return false;
}

}  // namespace sigil::geometry::device
