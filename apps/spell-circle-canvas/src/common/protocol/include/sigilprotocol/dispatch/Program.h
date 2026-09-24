#pragma once

/** @file
 * @ingroup protocol-runtime
 * WHAT ONLY THE PROGRAM KNOWS ABOUT ITSELF: its name and version, where
 * it keeps its state, how its clock moves and which sessions it holds —
 * what `host.describe` answers beside what the dispatcher knows.
 */

#include <sigilprotocol/protocol_values.h>

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace sigil::protocol {

/** THE PROGRAM A DISPATCHER ANSWERS FOR. The dispatcher knows which
 *  domains are mounted and which clients are attached; everything else
 *  `host.describe` answers comes from here, read at the moment it is
 *  asked. */
struct Program {
  /** The program: Sketchbook, Seer or the receiver. */
  std::string name;
  /** The program's own version. */
  std::string version;
  /** The directory the program keeps its state under: the protocol's
   *  address file, the stills a client asks for and every cache. An
   *  endpoint refuses to listen without one. */
  std::filesystem::path stateRoot;
  /** How the program's clock moves now; empty for the wall clock, which
   *  is what a program with no clock agent runs by. */
  std::function<clock::Policy()> clockPolicy;
  /** The sketch sessions the program holds open now; empty for none. */
  std::function<std::vector<session::values::Summary>()> sessions;
};

}  // namespace sigil::protocol
