#pragma once

/** @file
 * HOW A NETWORK TRANSPORT READS AND WRITES AN ADDRESS: the host and port
 * an authority names, the port alone, a peer's raw bytes as an address,
 * and an address spelled back the way a URI is. The datagram transport,
 * the socket listener and the socket client all read an authority the
 * same way and print a peer the same way, so one reader reads the sender
 * of an arrival off any of them.
 */

#include <boost/asio/ip/address.hpp>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace sigil::io::network {

/** A host and the port on it. The host is empty where an authority left
 *  it out, which is a port to listen on every interface. */
struct HostAndPort {
  std::string host;
  std::uint16_t port = 0;
};

/** The port @p digits spells: decimal digits and nothing else, no larger
 *  than a port can be. Nothing for anything else. */
std::optional<std::uint16_t> readPort(std::string_view digits);

/** The host and port @p authority names, or nothing where it names none.
 *  The host is bracketed when it is an IPv6 literal; an unbracketed host
 *  ends at the first colon, which leaves an IPv6 literal written without
 *  its brackets to fail on its own digits. */
std::optional<HostAndPort> readAuthority(std::string_view authority);

/** The address in @p binary, four bytes or sixteen; nothing for any other
 *  length. An IPv4 peer that reached a dual-stack socket arrives mapped
 *  into IPv6 and is answered as the IPv4 address it can be written back
 *  to. */
std::optional<boost::asio::ip::address> addressOf(std::string_view binary);

/** @p host and @p port spelled the way a URI of @p scheme is,
 *  "udp://127.0.0.1:52341". An IPv6 address is bracketed, so what follows
 *  the last colon is always the port, and an IPv4 address mapped into
 *  IPv6 is named by the IPv4 address it can be written back to. */
std::string printedAddress(std::string_view scheme,
                           const boost::asio::ip::address& host,
                           unsigned int port);

}  // namespace sigil::io::network
