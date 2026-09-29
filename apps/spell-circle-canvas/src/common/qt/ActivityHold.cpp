#include "ActivityHold.h"

namespace ifrit::qt {

void ActivityHold::set(const void* holder, ActivityReason reason,
                       bool stands) {
  const bool wasHeld = held();
  if (stands)
    m_standing.insert({holder, reason});
  else
    m_standing.erase({holder, reason});
  announce(wasHeld);
}

void ActivityHold::withdraw(const void* holder) {
  const bool wasHeld = held();
  std::erase_if(m_standing,
                [holder](const auto& entry) { return entry.first == holder; });
  announce(wasHeld);
}

void ActivityHold::announce(bool wasHeld) {
  if (held() != wasHeld && m_onChange) m_onChange(held());
}

}  // namespace ifrit::qt
