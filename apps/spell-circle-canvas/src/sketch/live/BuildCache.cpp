#include "BuildCache.h"

#include <src/core/SkMD5.h>
#include <unistd.h>

#include <atomic>
#include <cstdlib>
#include <fstream>
#include <iterator>

namespace sigil::sketch {

std::string buildDigest(std::string_view bytes) {
  SkMD5 digest;
  digest.write(bytes.data(), bytes.size());
  return digest.finish().toLowercaseHexString().c_str();
}

std::filesystem::path buildCacheDirectory() {
  if (const char* override = std::getenv("SIGIL_SKETCH_CACHE")) return override;
#ifdef __APPLE__
  if (const char* home = std::getenv("HOME"))
    return std::filesystem::path(home) / "Library/Caches/SigilSketch/builds";
#else
  if (const char* cache = std::getenv("XDG_CACHE_HOME"))
    return std::filesystem::path(cache) / "SigilSketch/builds";
  if (const char* home = std::getenv("HOME"))
    return std::filesystem::path(home) / ".cache/SigilSketch/builds";
#endif
  return {};
}

bool restoreBuild(const std::filesystem::path& cache, const std::string& key,
                  const std::filesystem::path& output) {
  if (cache.empty() || key.empty()) return false;
  std::ifstream input(cache / (key + ".bin"), std::ios::binary);
  std::string digest;
  if (!std::getline(input, digest) || digest.size() != 32) return false;
  const std::string bytes(std::istreambuf_iterator<char>(input), {});
  if (input.bad() || buildDigest(bytes) != digest) return false;
  std::ofstream restored(output, std::ios::binary | std::ios::trunc);
  restored.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  restored.close();
  return !restored.fail();
}

void storeBuild(const std::filesystem::path& cache, const std::string& key,
                const std::filesystem::path& library) {
  if (cache.empty() || key.empty()) return;
  std::error_code error;
  std::filesystem::create_directories(cache, error);
  if (error) return;
  static std::atomic<unsigned> serial{0};
  const auto pending = cache / (key + "." + std::to_string(getpid()) + "." +
                                std::to_string(++serial) + ".tmp");
  std::ifstream input(library, std::ios::binary);
  if (!input) return;
  const std::string bytes(std::istreambuf_iterator<char>(input), {});
  if (input.bad()) return;
  std::ofstream output(pending, std::ios::binary | std::ios::trunc);
  output << buildDigest(bytes) << '\n';
  output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  output.close();
  if (output.fail()) {
    std::filesystem::remove(pending, error);
    return;
  }
  std::filesystem::rename(pending, cache / (key + ".bin"), error);
  if (error) std::filesystem::remove(pending, error);
}

}  // namespace sigil::sketch
