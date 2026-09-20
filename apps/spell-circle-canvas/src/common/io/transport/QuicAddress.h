/** @file
 * WHAT A quic:// URI NAMES, and how an end of one is spelled back: the
 * host and port the authority carries, the settings its query carries,
 * the file a named certificate or key stands at, and the address a
 * connection's other end arrives under.
 */

#pragma once

#include <msquic.h>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace sigil::io {

class Hub;

namespace quic {

/** WHAT A quic:// URI NAMES: the host that makes it an end to call
 *  rather than a port to hold, the port, the certificate and key a port
 *  answers with, whether a call checks what it is shown, and whether a
 *  send goes as a datagram rather than a stream. */
struct Address {
  std::string host;
  std::string port;
  std::string certificate;
  std::string key;
  bool insecure = false;
  bool datagrams = false;
};

/** The value @p name carries in @p query, or nothing where the query
 *  carries none. A query is ampersand-separated pairs, so a door that
 *  grows a second setting spells it beside these and neither has to know
 *  about the other; a value may hold anything but an ampersand, which is
 *  what lets one of them be a URI. */
std::string_view valueNamed(std::string_view query, std::string_view name);

/** The address @p uri names, or nothing when it names none: a host and a
 *  port, with the query behind them. The host is bracketed when it is an
 *  IPv6 literal and left out altogether to hold a port on every
 *  interface; the port is decimal digits and nothing else. */
std::optional<Address> parseAddress(std::string_view uri);

/** The port @p place named, as a number. It parsed as one already, which
 *  is what made the address an address. */
uint16_t portOf(const Address& place);

/** The authority as a caller writes it: the host, bracketed where it is
 *  an IPv6 literal, and the port behind it. */
std::string authorityOf(const Address& place);

/** The file @p named stands at: the hub's mount table first, and the
 *  name as a plain path where no mount matches it. Empty where nothing
 *  of that name is a file, which is what a door that cannot answer for
 *  itself says. */
std::filesystem::path fileAt(const Hub& hub, const std::string& named);

/** One end's address, spelled the way a URI of this scheme is,
 *  "quic://127.0.0.1:52341". An IPv6 address is bracketed, so what
 *  follows the last colon is always the port; a peer that reached a
 *  dual-stack listener over IPv4 arrives as its address mapped into
 *  IPv6, and is named by the IPv4 address it can be written back to
 *  rather than by the mapping. It is the same spelling the datagram
 *  transport beside this one gives an endpoint, so one reader reads the
 *  sender of an arrival off either. */
std::string peerAddress(const QUIC_ADDR& address);

}  // namespace quic

}  // namespace sigil::io
