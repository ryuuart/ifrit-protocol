/** @file
 * Reading a host and port out of an authority, and printing a peer back
 * the way a URI spells it.
 */

#include "Address.h"

#include <boost/asio/ip/address_v4.hpp>
#include <boost/asio/ip/address_v6.hpp>
#include <charconv>
#include <cstring>
#include <locale>
#include <sstream>
#include <system_error>

namespace sigil::io::network {

std::optional<std::uint16_t> readPort(std::string_view digits) {
  unsigned int port = 0;
  const char* const end = digits.data() + digits.size();
  const std::from_chars_result read = std::from_chars(digits.data(), end, port);
  if (read.ec != std::errc() || read.ptr != end || port > 65535)
    return std::nullopt;
  return static_cast<std::uint16_t>(port);
}

std::optional<HostAndPort> readAuthority(std::string_view authority) {
  HostAndPort address;
  std::string_view rest = authority;
  if (rest.starts_with('[')) {
    const size_t bracket = rest.find(']');
    if (bracket == std::string_view::npos || bracket == 1) return std::nullopt;
    address.host = std::string(rest.substr(1, bracket - 1));
    rest = rest.substr(bracket + 1);
  } else {
    const size_t colon = rest.find(':');
    if (colon == std::string_view::npos) return std::nullopt;
    address.host = std::string(rest.substr(0, colon));
    rest = rest.substr(colon);
  }
  if (!rest.starts_with(':')) return std::nullopt;
  const std::optional<std::uint16_t> port = readPort(rest.substr(1));
  if (!port) return std::nullopt;
  address.port = *port;
  return address;
}

std::optional<boost::asio::ip::address> addressOf(std::string_view binary) {
  if (binary.size() == 4) {
    boost::asio::ip::address_v4::bytes_type four{};
    std::memcpy(four.data(), binary.data(), four.size());
    return boost::asio::ip::address(boost::asio::ip::address_v4(four));
  }
  if (binary.size() == 16) {
    boost::asio::ip::address_v6::bytes_type sixteen{};
    std::memcpy(sixteen.data(), binary.data(), sixteen.size());
    const boost::asio::ip::address_v6 six(sixteen);
    if (six.is_v4_mapped())
      return boost::asio::ip::address(
          boost::asio::ip::make_address_v4(boost::asio::ip::v4_mapped, six));
    return boost::asio::ip::address(six);
  }
  return std::nullopt;
}

std::string printedAddress(std::string_view scheme,
                           const boost::asio::ip::address& host,
                           unsigned int port) {
  std::ostringstream printed;
  printed.imbue(std::locale::classic());
  printed << scheme << "://";
  if (host.is_v6()) {
    const boost::asio::ip::address_v6 six = host.to_v6();
    if (six.is_v4_mapped())
      printed << boost::asio::ip::make_address_v4(boost::asio::ip::v4_mapped,
                                                  six);
    else
      printed << "[" << six << "]";
  } else {
    printed << host;
  }
  printed << ":" << port;
  return printed.str();
}

}  // namespace sigil::io::network
