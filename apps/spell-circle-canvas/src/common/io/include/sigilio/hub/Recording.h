#pragma once

/** @file
 * @ingroup io-hub
 * THE RECORDING FORMAT: a feed's arrivals as a file, written as they
 * come and read back as a list. A recording opens with the line
 * `sigil-feed-recording 1`; every frame after it is the arrival's time
 * as a double, the message's length as a 32-bit unsigned, and that many
 * bytes, the two numbers in the byte order of the machine that wrote
 * them. A frame reaches the disk as it arrives, so a run that is killed
 * leaves a last frame that may be cut short, which reading drops.
 * Neither an arrival's generation nor the address it came from is
 * written down.
 */

#include <filesystem>
#include <fstream>
#include <optional>
#include <vector>

#include "sigilio/hub/Feed.h"

namespace sigil::io {

/** A RECORDING BEING WRITTEN: the file is emptied on the way in, and
 *  every arrival is appended to it as it comes. */
class RecordingWriter {
 public:
  /** Opens @p path for writing, emptying whatever stood there. */
  explicit RecordingWriter(const std::filesystem::path& path);

  /** Appends one frame and puts it on the disk. False when the file
   *  cannot take it, which leaves every frame already written whole. */
  bool append(const Arrival& arrival);

  /** Whether the file is open and everything written so far reached
   *  it. */
  bool good() const;

 private:
  std::ofstream m_stream;
};

/** Every arrival @p path holds, in the order it lists them and numbered
 *  from 1. Nothing when the file cannot be read or does not open with
 *  the format's header. A last frame cut short is dropped and the whole
 *  frames before it are kept. */
std::optional<std::vector<Arrival>> readRecording(
    const std::filesystem::path& path);

}  // namespace sigil::io
