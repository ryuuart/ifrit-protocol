#pragma once

/** @file
 * THE RECORDING FORMAT: a feed's arrivals as a file, written as they
 * come and read back as a list.
 *
 * A recording opens with the line `sigil-feed-recording 1`. Every frame
 * after it is the arrival's time as a double, then the message's length
 * as a 32-bit unsigned, then that many bytes. Both numbers are in the
 * byte order of the machine that wrote them: a recording is read by
 * that machine and by its peers rather than carried between
 * architectures, and every target this tree builds for is
 * little-endian.
 *
 * A frame reaches the disk as it arrives, so a run that is killed
 * leaves a file whose last frame may be cut short. Reading drops that
 * frame and keeps every whole one before it.
 *
 * An arrival's generation is not written down. It counts the messages
 * one feed has taken, which is a property of the feed rather than of
 * the recording, so a reader numbers the frames 1, 2, 3 as it reads
 * them.
 *
 * Neither is the address a message came from. A recording is the
 * messages and not who sent them, so a replayed arrival names no
 * sender.
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
