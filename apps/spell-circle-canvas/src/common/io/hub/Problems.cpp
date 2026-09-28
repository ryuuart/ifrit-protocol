/** @file
 * The hub's problems: at most one per URI, in the order they were first
 * said, replaced by a later report and taken back by a clear or by a
 * load of the URI that succeeds.
 */

#include <algorithm>
#include <utility>

#include "sigilio/hub/Hub.h"

namespace sigil::io {

std::vector<Problem> Hub::problems() const {
  const std::lock_guard lock(m_mutex);
  return m_problems;
}

void Hub::reportProblem(Problem problem) {
  const std::lock_guard lock(m_mutex);
  const auto said =
      std::find_if(m_problems.begin(), m_problems.end(),
                   [&](const Problem& held) { return held.uri == problem.uri; });
  if (said != m_problems.end())
    *said = std::move(problem);
  else
    m_problems.push_back(std::move(problem));
}

void Hub::clearProblem(std::string_view uri) {
  const std::lock_guard lock(m_mutex);
  std::erase_if(m_problems,
                [&](const Problem& held) { return held.uri == uri; });
}

void Hub::clearProblems() {
  const std::lock_guard lock(m_mutex);
  m_problems.clear();
}

}  // namespace sigil::io
