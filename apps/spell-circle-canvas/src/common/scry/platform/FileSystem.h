#pragma once

/** @file
 * The file system Ultralight's Platform is handed: two roots on disk
 * and one virtual extension. Internal: it names Ultralight types, and
 * Ultralight is private to the library.
 */

#include <Ultralight/platform/FileSystem.h>

#include <string>

namespace sigil::scry {

class CallbackLogger;

/**
 * Disk file system with two roots: paths under Ultralight's
 * resource_path_prefix ("resources/") map into the SDK resource dir
 * (ICU data, CA certs), absolute paths are taken as they are, and
 * everything else resolves against the user-facing base dir for
 * file:/// content.
 *
 * "<name>.imgsrc" paths are virtual: they resolve to the WebImage
 * registered under <name> through Ultralight's ImageSourceProvider, so
 * the file system synthesizes the IMGSRC indirection file Ultralight
 * expects instead of touching disk. A slot no WebImage is registered
 * under still gets its file — the slot may be registered later — plus a
 * warning naming it, since at load time that is almost always a typo or
 * an ordering bug.
 */
class PrefixFileSystem final : public ultralight::FileSystem {
 public:
  PrefixFileSystem(std::string resourceDir, std::string baseDir,
                   CallbackLogger* logger)
      : m_resourceDir(std::move(resourceDir)),
        m_baseDir(std::move(baseDir)),
        m_logger(logger) {}

  // ultralight::FileSystem
  bool FileExists(const ultralight::String& path) override;
  ultralight::String GetFileMimeType(const ultralight::String& path) override;
  ultralight::String GetFileCharset(const ultralight::String& path) override;
  ultralight::RefPtr<ultralight::Buffer> OpenFile(
      const ultralight::String& path) override;

  /** The disk path an Ultralight path lands on, by the two-root rule. */
  std::string resolve(const ultralight::String& path) const;

 private:
  std::string m_resourceDir;
  std::string m_baseDir;
  CallbackLogger* m_logger;
};

}  // namespace sigil::scry
