#pragma once

/** @file
 * WRITING A WIRE DOWN, AND PLAYING IT BACK.
 *
 * Recording appends every arrival from the moment it starts to a file;
 * what came before is not in the file, because the feed handed those
 * messages out already. Replaying is the same URI opened onto that file
 * instead of onto a socket, so everything downstream — the list, the
 * log, the readings — is looking at the same kind of wire it was looking
 * at live, and the messages arrive as time is moved forward rather than
 * all at once.
 */

#include <sigilio/hub/Feed.h>

#include <filesystem>
#include <memory>
#include <string_view>

namespace sigil::seer {

class Wires;

/** THE ONE RECORDING A SESSION IS MAKING, and the door back onto a file
 *  that was made earlier. */
class Recorder {
 public:
  /** A recorder over @p wires, which must outlive it. Nothing is being
   *  recorded until `record()` says so. */
  explicit Recorder(Wires& wires);
  ~Recorder();

  Recorder(const Recorder&) = delete;
  Recorder& operator=(const Recorder&) = delete;

  /** Appends every arrival on @p feed from now on to @p path. Recording
   *  a second wire stops the first: one session writes one file, so the
   *  file a reader is told about is the file being written. False when
   *  there is no feed to record. */
  bool record(const std::shared_ptr<io::Feed>& feed,
              std::filesystem::path path);

  /** Takes no more. What reached the file stays whole. */
  void stop();

  /** Whether a wire is being written to a file right now. */
  bool recording() const;

  /** The file being written; empty when nothing is being recorded. */
  const std::filesystem::path& path() const { return m_path; }

  /** Opens @p uri onto the recording at @p path: the wire that was there
   *  is closed, the URI is resolved onto the file, and the feed that
   *  comes back delivers what the file holds as the wires are
   *  dispatched. The answer carries a sentence in its `error()` when the
   *  file is not a recording. */
  std::shared_ptr<io::Feed> replay(std::string_view uri,
                                   const std::filesystem::path& path);

 private:
  Wires& m_wires;
  /** The feed being recorded, held weakly: a wire closed while it is
   *  being recorded takes its writer with it, and this is left with
   *  nothing to stop rather than with a feed nobody else has. */
  std::weak_ptr<io::Feed> m_feed;
  std::filesystem::path m_path;
};

}  // namespace sigil::seer
