#include "QuicAddress.h"

#include <arpa/inet.h>
#include <msquic.h>
#include <netinet/in.h>

#include <charconv>
#include <cstring>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

#include "sigilio/hub/Hub.h"

namespace sigil::io::quic {

std::string_view valueNamed(std::string_view query, std::string_view name) {
  while (!query.empty()) {
    const size_t next = query.find('&');
    const std::string_view pair = query.substr(0, next);
    if (pair.size() > name.size() && pair.starts_with(name) &&
        pair[name.size()] == '=')
      return pair.substr(name.size() + 1);
    if (next == std::string_view::npos) break;
    query = query.substr(next + 1);
  }
  return {};
}

std::optional<Address> parseAddress(std::string_view uri) {
  constexpr std::string_view kScheme = "quic://";
  if (!uri.starts_with(kScheme)) return std::nullopt;
  std::string_view rest = uri.substr(kScheme.size());
  Address address;
  // THE QUERY IS THE DOOR'S OWN ARRANGEMENT and no part of the address
  // anybody reaches, so it comes off first: what the socket stands on,
  // what the feed reports and what an arrival names are the authority
  // alone.
  if (const size_t question = rest.find('?');
      question != std::string_view::npos) {
    const std::string_view query = rest.substr(question + 1);
    address.certificate = std::string(valueNamed(query, "cert"));
    address.key = std::string(valueNamed(query, "key"));
    address.insecure = valueNamed(query, "insecure") == "1";
    address.datagrams = valueNamed(query, "datagrams") == "1";
    rest = rest.substr(0, question);
  }

  if (rest.starts_with('[')) {
    const size_t bracket = rest.find(']');
    if (bracket == std::string_view::npos || bracket == 1) return std::nullopt;
    address.host = std::string(rest.substr(1, bracket - 1));
    rest = rest.substr(bracket + 1);
  } else {
    // An unbracketed host ends at the first colon, which leaves an IPv6
    // literal written without its brackets to fail on its own digits.
    const size_t colon = rest.find(':');
    if (colon == std::string_view::npos) return std::nullopt;
    address.host = std::string(rest.substr(0, colon));
    rest = rest.substr(colon);
  }
  if (!rest.starts_with(':')) return std::nullopt;
  const std::string_view digits = rest.substr(1);
  unsigned int port = 0;
  const char* const end = digits.data() + digits.size();
  const std::from_chars_result read = std::from_chars(digits.data(), end, port);
  if (read.ec != std::errc() || read.ptr != end || port > 65535)
    return std::nullopt;
  address.port = std::string(digits);
  return address;
}

uint16_t portOf(const Address& place) {
  unsigned int port = 0;
  std::from_chars(place.port.data(), place.port.data() + place.port.size(),
                  port);
  return static_cast<uint16_t>(port);
}

std::string authorityOf(const Address& place) {
  return place.host.find(':') == std::string::npos
             ? place.host + ":" + place.port
             : "[" + place.host + "]:" + place.port;
}

std::filesystem::path fileAt(const Hub& hub, const std::string& named) {
  if (named.empty()) return {};
  std::filesystem::path path = hub.resolve(named);
  if (path.empty()) path = named;
  std::error_code ec;
  if (!std::filesystem::is_regular_file(path, ec) || ec) return {};
  return path;
}

std::string peerAddress(const QUIC_ADDR& address) {
  char printed[INET6_ADDRSTRLEN] = {};
  const std::string port = std::to_string(QuicAddrGetPort(&address));
  if (QuicAddrGetFamily(&address) == QUIC_ADDRESS_FAMILY_INET) {
    if (!inet_ntop(AF_INET, &address.Ipv4.sin_addr, printed, sizeof(printed)))
      return {};
    return "quic://" + std::string(printed) + ":" + port;
  }
  const in6_addr& six = address.Ipv6.sin6_addr;
  if (IN6_IS_ADDR_V4MAPPED(&six)) {
    in_addr four{};
    std::memcpy(&four, six.s6_addr + 12, sizeof(four));
    if (!inet_ntop(AF_INET, &four, printed, sizeof(printed))) return {};
    return "quic://" + std::string(printed) + ":" + port;
  }
  if (!inet_ntop(AF_INET6, &six, printed, sizeof(printed))) return {};
  return "quic://[" + std::string(printed) + "]:" + port;
}

}  // namespace sigil::io::quic
