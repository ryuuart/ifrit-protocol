#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace sigil::sketch {

// Skia supplies the byte digest; compilation policy stays in the live host.
std::string buildDigest(std::string_view bytes);
std::filesystem::path buildCacheDirectory();
bool restoreBuild(const std::filesystem::path& cache, const std::string& key,
                  const std::filesystem::path& output);
void storeBuild(const std::filesystem::path& cache, const std::string& key,
                const std::filesystem::path& library);

}  // namespace sigil::sketch
