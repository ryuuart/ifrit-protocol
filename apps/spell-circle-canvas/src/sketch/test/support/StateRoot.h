#pragma once

/** @file
 * A state root a test owns: the process keeps what it writes between
 * runs in a scratch directory for as long as the test stands.
 */

#include <sigilsketch/core/State.h>

#include <filesystem>
#include <string_view>

#include "ScratchDir.h"

namespace sigil::sketch::test {

/** A SCRATCH DIRECTORY THE PROCESS KEEPS ITS STATE UNDER while this
 *  stands: builds, and whatever a host keeps beside them, land here and
 *  nowhere a later run would read. The root that stood before is put back
 *  on the way out, and the directory is removed. */
class StateRoot {
 public:
  explicit StateRoot(std::string_view label)
      : m_scratch(label), m_previous(stateDirectory()) {
    setStateDirectory(m_scratch.path);
  }
  StateRoot(const StateRoot&) = delete;
  StateRoot& operator=(const StateRoot&) = delete;
  ~StateRoot() { setStateDirectory(m_previous); }

  [[nodiscard]] const std::filesystem::path& path() const {
    return m_scratch.path;
  }

 private:
  sigil::test::ScratchDir m_scratch;
  std::filesystem::path m_previous;
};

}  // namespace sigil::sketch::test
