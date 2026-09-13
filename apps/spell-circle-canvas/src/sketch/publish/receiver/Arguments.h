#pragma once

/** @file
 * The command line as one value: every verb Receiver takes, read once
 * into what the lane behind it is asked through.
 */

#include <optional>
#include <string>

namespace receiver {

/** WHAT THIS RUN WAS ASKED FOR. The shape of the line names the lane —
 *  the listing, the window, the grab — and the rest narrows it: which
 *  publication, whose application when two announce one name, and how
 *  long a grab waits for how many frames.
 *
 *  The two numbers are OPTIONAL rather than defaulted, so a lane that has
 *  no use for one can refuse it instead of reading past it: each lane
 *  names its own default for what it does read. */
struct Arguments {
  /** The name the publication announced itself under. Empty only for the
   *  listing, which is about every name at once. */
  std::string server;
  /** Which application's publication, when two announce the same name.
   *  Empty takes whichever answers first. */
  std::string app;
  /** Where a grab writes its PNG. Empty opens the window instead. */
  std::string grabPath;
  /** How many frames must arrive before a grab reads one back. */
  std::optional<int> frames;
  /** How long a grab waits for its publication and its frames. */
  std::optional<double> timeoutSeconds;
  /** Every publication on this machine, and nothing else. */
  bool list = false;
};

/** Reads the whole command line. Nothing when a flag was not understood,
 *  when its value could not be read as what it has to be, or when the
 *  line asks for two lanes at once, with what was wrong on stderr; a run
 *  that gets nothing back exits without subscribing to anything. */
std::optional<Arguments> parseArguments(int argc, char* argv[]);

}  // namespace receiver
