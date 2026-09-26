#include <sigilcompose/kit/Rows.h>
#include <sigilsketch/kit/Connection.h>

#include <string>
#include <vector>

namespace sigil::sketch::kit {

namespace {

/** The readiness as the word a reader expects of a door. */
const char8_t* readinessOf(io::ReadyState readiness) {
  switch (readiness) {
    case io::ReadyState::Connecting:
      return u8"connecting";
    case io::ReadyState::Open:
      return u8"open";
    case io::ReadyState::Closed:
      return u8"closed";
  }
  return u8"closed";
}

}  // namespace

compose::Element connectionReadout(const data::Connection& connection,
                                   const ConnectionReadout& how) {
  const data::ConnectionState state = connection.state();
  const std::string& sender = connection.latest().sender();
  const compose::Utf8 door =
      !state.localAddress.empty()
          ? compose::Utf8(state.localAddress)
          : (!how.door.empty() ? how.door : compose::Utf8(connection.uri()));
  const std::vector<compose::kit::Reading> rows{
      {.name = u8"door", .value = door},
      {.name = u8"state", .value = compose::Utf8(readinessOf(state.readiness))},
      {.name = u8"revision", .value = std::to_string(state.revision)},
      {.name = u8"dropped", .value = std::to_string(state.dropped)},
      {.name = u8"undecodable", .value = std::to_string(state.undecodable)},
      state.error.empty()
          ? compose::kit::Reading{.name = u8"sender",
                                  .value = sender.empty()
                                               ? compose::Utf8("-")
                                               : compose::Utf8(sender)}
          : compose::kit::Reading{.name = u8"error", .value = state.error}};
  return compose::kit::readout(rows, how.rows);
}

}  // namespace sigil::sketch::kit
