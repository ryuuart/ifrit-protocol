#include <sigilcompose/kit/Rows.h>
#include <sigilsketch/kit/Connection.h>

#include <string>
#include <vector>

namespace sigil::sketch::kit {

compose::Element connectionReadout(const data::Connection& connection,
                                   const ConnectionReadout& how) {
  const data::Connection::Vitals vitals = connection.vitals();
  const compose::Utf8 door =
      !vitals.address.empty()
          ? compose::Utf8(vitals.address)
          : (!how.door.empty() ? how.door : compose::Utf8(connection.uri()));
  const std::vector<compose::kit::Reading> rows{
      {.name = u8"door", .value = door},
      {.name = u8"generation", .value = std::to_string(vitals.generation)},
      {.name = u8"dropped", .value = std::to_string(vitals.dropped)},
      {.name = u8"undecodable", .value = std::to_string(vitals.undecodable)},
      vitals.error.empty()
          ? compose::kit::Reading{.name = u8"sender",
                                  .value = vitals.sender.empty()
                                               ? compose::Utf8("-")
                                               : compose::Utf8(vitals.sender)}
          : compose::kit::Reading{.name = u8"error", .value = vitals.error}};
  return compose::kit::readout(rows, how.rows);
}

}  // namespace sigil::sketch::kit
