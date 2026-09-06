#pragma once

/** @file
 * The header/host skew guard: what a reload compares a framework header
 * against, and which header is too new for the image that is running.
 */

#include <filesystem>
#include <string>

namespace sigil::sketch {

/** THE FIRST ABI-BOUNDARY REPOSITORY HEADER on @p flagsFile's -I paths
 *  that is newer than @p hostTime; empty when none is.
 *
 *  A sketch dylib compiled against framework headers NEWER than the host
 *  binary loads into a host whose structs have the OLD layout: dispatch
 *  corrupts and the crash points nowhere near the cause. The ABI version
 *  guards deliberate changes to the sketch surface; this guards every
 *  header edit.
 *
 *  EVERY PUBLIC HEADER OF A FRAMEWORK LIBRARY IS AN ABI BOUNDARY, and the
 *  line is drawn there rather than at a list of file names. A sketch
 *  constructs the libraries' objects and the host mutates them — a pool
 *  filled in the dylib and resized by the host, an element built in the
 *  dylib and reconciled by the host — so any header that changes a layout
 *  either side reads changes it on ONE side only, and the corruption
 *  surfaces wherever the object is next touched rather than where it was
 *  caused. A header believed harmless is exactly the header that gets one
 *  wrong. */
[[nodiscard]] std::string newerHeaderThanHost(
    const std::filesystem::path& flagsFile,
    std::filesystem::file_time_type hostTime);

}  // namespace sigil::sketch
