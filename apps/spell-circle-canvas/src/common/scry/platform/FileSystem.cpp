/** @file
 * The two-root file system: resolution, existence, MIME types by
 * extension, and files opened whole into 16-byte-aligned buffers (the
 * alignment the ICU data file requires), with .imgsrc paths synthesized
 * rather than read.
 */

#include "FileSystem.h"

#include <Ultralight/Buffer.h>
#include <Ultralight/ImageSource.h>

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <utility>

#include "Logger.h"
#include "Utf8.h"

namespace sigil::scry {

namespace {

bool isImageSourcePath(const std::string& path) {
  return std::filesystem::path(path).extension() == ".imgsrc";
}

}  // namespace

std::string PrefixFileSystem::resolve(const ultralight::String& path) const {
  std::string p = toUtf8(path);
  constexpr std::string_view kResourcePrefix = "resources/";
  if (p.rfind(kResourcePrefix, 0) == 0)
    return m_resourceDir + "/" + p.substr(kResourcePrefix.size());
  if (!p.empty() && p.front() == '/') return p;
  return m_baseDir + "/" + p;
}

bool PrefixFileSystem::FileExists(const ultralight::String& path) {
#ifdef IFRIT_WEB_TRACE_FS
  std::fprintf(stderr, "[fs] FileExists(%s)\n", toUtf8(path).c_str());
#endif
  if (isImageSourcePath(toUtf8(path))) return true;
  std::error_code ec;
  return std::filesystem::is_regular_file(resolve(path), ec);
}

ultralight::String PrefixFileSystem::GetFileMimeType(
    const ultralight::String& path) {
  std::string ext = std::filesystem::path(toUtf8(path)).extension().string();
  for (char& c : ext)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

  static const std::pair<std::string_view, std::string_view> kMimeTypes[] = {
      {".html", "text/html"},
      {".htm", "text/html"},
      {".css", "text/css"},
      {".js", "application/javascript"},
      {".mjs", "application/javascript"},
      {".json", "application/json"},
      {".txt", "text/plain"},
      {".svg", "image/svg+xml"},
      {".png", "image/png"},
      {".jpg", "image/jpeg"},
      {".jpeg", "image/jpeg"},
      {".gif", "image/gif"},
      {".webp", "image/webp"},
      {".avif", "image/avif"},
      {".ico", "image/x-icon"},
      {".woff", "font/woff"},
      {".woff2", "font/woff2"},
      {".ttf", "font/ttf"},
      {".otf", "font/otf"},
      {".wasm", "application/wasm"},
      {".pem", "application/x-pem-file"},
      {".dat", "application/octet-stream"},
      {".imgsrc", "image/imgsrc"},
  };
  for (const auto& [extension, mime] : kMimeTypes)
    if (ext == extension) return ultralight::String(mime.data(), mime.size());
  return "application/unknown";
}

ultralight::String PrefixFileSystem::GetFileCharset(const ultralight::String&) {
  return "utf-8";
}

ultralight::RefPtr<ultralight::Buffer> PrefixFileSystem::OpenFile(
    const ultralight::String& path) {
#ifdef IFRIT_WEB_TRACE_FS
  std::fprintf(stderr, "[fs] OpenFile(%s)\n", toUtf8(path).c_str());
#endif
  if (std::string p = toUtf8(path); isImageSourcePath(p)) {
    std::string name = std::filesystem::path(p).stem().string();
    if (!ultralight::ImageSourceProvider::instance().GetImageSource(
            name.c_str()) &&
        m_logger)
      m_logger->log(LogLevel::Warning,
                    "page requested image slot '" + name +
                        "' but no WebImage is registered under that name "
                        "(create it before loading the page)");
    std::string content = "IMGSRC-V1\n" + name;
    return ultralight::Buffer::CreateFromCopy(content.data(), content.size());
  }

  std::ifstream file(resolve(path), std::ios::binary | std::ios::ate);
  if (!file) return nullptr;

  std::streamsize fileSize = file.tellg();
  file.seekg(0, std::ios::beg);

  // malloc keeps the 16-byte alignment the ICU data file requires.
  void* data = std::malloc(static_cast<size_t>(fileSize));
  if (!data || !file.read(static_cast<char*>(data), fileSize)) {
    std::free(data);
    return nullptr;
  }
  return ultralight::Buffer::Create(
      data, static_cast<size_t>(fileSize), nullptr,
      [](void*, void* bufferData) { std::free(bufferData); });
}

}  // namespace sigil::scry
