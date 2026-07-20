// Platform handlers handed to ultralight::Platform: the SkBitmap-backed
// surface, the two-root file system, and the logger bridge.

#include "WebInternal.h"

#include <include/core/SkColorSpace.h>
#include <include/core/SkImageInfo.h>

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

namespace sigil::web {

std::string executableAdjacentResourceDir() {
  std::filesystem::path executable;
#ifdef __APPLE__
  uint32_t size = 0;
  _NSGetExecutablePath(nullptr, &size);
  std::string buffer(size, '\0');
  if (_NSGetExecutablePath(buffer.data(), &size) != 0)
    return {};
  buffer.resize(std::strlen(buffer.c_str()));
  executable = buffer;
#else
  std::error_code ec;
  executable = std::filesystem::read_symlink("/proc/self/exe", ec);
  if (ec)
    return {};
#endif
  std::error_code ec;
  executable = std::filesystem::weakly_canonical(executable, ec);
  if (ec)
    return {};
  std::filesystem::path dir = executable.parent_path() / "resources";
  if (!std::filesystem::is_regular_file(dir / "icudt67l.dat", ec))
    return {};
  return dir.string();
}

void SkiaSurface::Resize(uint32_t width, uint32_t height) {
  if (m_bitmap.width() == static_cast<int>(width) &&
      m_bitmap.height() == static_cast<int>(height))
    return;

  SkImageInfo info =
      SkImageInfo::Make(static_cast<int>(width), static_cast<int>(height),
                        kBGRA_8888_SkColorType, kPremul_SkAlphaType,
                        SkColorSpace::MakeSRGB());
  // 16-byte row alignment keeps Ultralight's SIMD paint paths on their
  // fast lane (same intent as Config::bitmap_alignment for the default
  // BitmapSurface).
  size_t rowBytes = (info.minRowBytes() + 15) & ~static_cast<size_t>(15);
  m_bitmap.allocPixels(info, rowBytes);
  m_bitmap.eraseColor(SK_ColorTRANSPARENT);
}

std::string PrefixFileSystem::resolve(const ultralight::String &path) const {
  std::string p = toUtf8(path);
  constexpr std::string_view kResourcePrefix = "resources/";
  if (p.rfind(kResourcePrefix, 0) == 0)
    return m_resourceDir + "/" + p.substr(kResourcePrefix.size());
  if (!p.empty() && p.front() == '/')
    return p;
  return m_baseDir + "/" + p;
}

namespace {

// "<name>.imgsrc" paths are virtual: they resolve to the WebImage
// registered under <name> via Ultralight's ImageSourceProvider, so the
// file system synthesizes the IMGSRC indirection file Ultralight expects
// instead of touching disk.
bool isImageSourcePath(const std::string &path) {
  return std::filesystem::path(path).extension() == ".imgsrc";
}

} // namespace

bool PrefixFileSystem::FileExists(const ultralight::String &path) {
#ifdef IFRIT_WEB_TRACE_FS
  std::fprintf(stderr, "[fs] FileExists(%s)\n", toUtf8(path).c_str());
#endif
  if (isImageSourcePath(toUtf8(path)))
    return true;
  std::error_code ec;
  return std::filesystem::is_regular_file(resolve(path), ec);
}

ultralight::String
PrefixFileSystem::GetFileMimeType(const ultralight::String &path) {
  std::string ext = std::filesystem::path(toUtf8(path)).extension().string();
  for (char &c : ext)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

  static const std::pair<std::string_view, std::string_view> kMimeTypes[] = {
      {".html", "text/html"},        {".htm", "text/html"},
      {".css", "text/css"},          {".js", "application/javascript"},
      {".mjs", "application/javascript"},
      {".json", "application/json"}, {".txt", "text/plain"},
      {".svg", "image/svg+xml"},     {".png", "image/png"},
      {".jpg", "image/jpeg"},        {".jpeg", "image/jpeg"},
      {".gif", "image/gif"},         {".webp", "image/webp"},
      {".avif", "image/avif"},       {".ico", "image/x-icon"},
      {".woff", "font/woff"},        {".woff2", "font/woff2"},
      {".ttf", "font/ttf"},          {".otf", "font/otf"},
      {".wasm", "application/wasm"}, {".pem", "application/x-pem-file"},
      {".dat", "application/octet-stream"},
      {".imgsrc", "image/imgsrc"},
  };
  for (const auto &[extension, mime] : kMimeTypes)
    if (ext == extension)
      return ultralight::String(std::string(mime).c_str());
  return "application/unknown";
}

ultralight::String
PrefixFileSystem::GetFileCharset(const ultralight::String &) {
  return "utf-8";
}

ultralight::RefPtr<ultralight::Buffer>
PrefixFileSystem::OpenFile(const ultralight::String &path) {
#ifdef IFRIT_WEB_TRACE_FS
  std::fprintf(stderr, "[fs] OpenFile(%s)\n", toUtf8(path).c_str());
#endif
  if (std::string p = toUtf8(path); isImageSourcePath(p)) {
    std::string name = std::filesystem::path(p).stem().string();
    // The page still gets the indirection file (the slot may be
    // registered later), but a missing WebImage at load time is almost
    // always a typo or ordering bug — say so instead of failing silently.
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
  if (!file)
    return nullptr;

  std::streamsize fileSize = file.tellg();
  file.seekg(0, std::ios::beg);

  // malloc keeps the 16-byte alignment the ICU data file requires.
  void *data = std::malloc(static_cast<size_t>(fileSize));
  if (!data || !file.read(static_cast<char *>(data), fileSize)) {
    std::free(data);
    return nullptr;
  }
  return ultralight::Buffer::Create(
      data, static_cast<size_t>(fileSize), nullptr,
      [](void *, void *bufferData) { std::free(bufferData); });
}

void CallbackLogger::LogMessage(ultralight::LogLevel level,
                                const ultralight::String &message) {
  LogLevel mapped = LogLevel::Info;
  if (level == ultralight::LogLevel::Error)
    mapped = LogLevel::Error;
  else if (level == ultralight::LogLevel::Warning)
    mapped = LogLevel::Warning;
  log(mapped, toUtf8(message));
}

void CallbackLogger::log(LogLevel level, const std::string &message) {
  if (m_callback) {
    m_callback(level, message);
    return;
  }
  if (level == LogLevel::Info)
    return;
  std::fprintf(stderr, "[SigilUltralight:%s] %s\n",
               level == LogLevel::Error ? "error" : "warning",
               message.c_str());
}

} // namespace sigil::web
