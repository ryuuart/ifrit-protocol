#include "QuicLibrary.h"

#include <msquic.h>

#include <string>

namespace sigil::io::quic {

thread_local bool insideCallback = false;

const Library& library() {
  static const Library one = [] {
    Library made;
    if (QUIC_FAILED(MsQuicOpen2(&made.api))) return Library{};
    const QUIC_REGISTRATION_CONFIG config{"sigil-feed",
                                          QUIC_EXECUTION_PROFILE_LOW_LATENCY};
    if (QUIC_FAILED(made.api->RegistrationOpen(&config, &made.registration))) {
      MsQuicClose(made.api);
      return Library{};
    }
    return made;
  }();
  return one;
}

QUIC_BUFFER protocol() {
  static std::string held{kProtocol};
  QUIC_BUFFER named;
  named.Length = static_cast<uint32_t>(held.size());
  named.Buffer = reinterpret_cast<uint8_t*>(held.data());
  return named;
}

std::string reasonOf(QUIC_STATUS status) {
  if (status == QUIC_STATUS_CONNECTION_REFUSED) return "the port refused it";
  if (status == QUIC_STATUS_CONNECTION_TIMEOUT) return "nothing answered";
  if (status == QUIC_STATUS_CONNECTION_IDLE)
    return "nothing crossed it for long enough that it was let go";
  if (status == QUIC_STATUS_UNREACHABLE)
    return "nothing is listening there, or the host cannot be reached";
  if (status == QUIC_STATUS_ALPN_NEG_FAILURE)
    return "the other end does not speak " + std::string(kProtocol);
  if (status == QUIC_STATUS_ADDRESS_IN_USE) return "the port is already held";
  if (status == QUIC_STATUS_ADDRESS_NOT_AVAILABLE)
    return "no interface of this machine has that address";
  if (status == QUIC_STATUS_INVALID_ADDRESS)
    return "that is not an address this machine can bind";
  if (status == QUIC_STATUS_ABORTED) return "it was cut short";
  if (status == QUIC_STATUS_USER_CANCELED) return "the other end gave it up";
  if (status == QUIC_STATUS_HANDSHAKE_FAILURE)
    return "the two ends could not finish a handshake";
  if (status == QUIC_STATUS_TLS_ERROR)
    return "the certificate or the key was refused";
  if (status == QUIC_STATUS_BAD_CERTIFICATE)
    return "the certificate was not accepted";
  if (status == QUIC_STATUS_EXPIRED_CERTIFICATE)
    return "the certificate has expired";
  if (status == QUIC_STATUS_UNKNOWN_CERTIFICATE ||
      status == QUIC_STATUS_CERT_UNTRUSTED_ROOT)
    return "nobody this machine trusts signed that certificate, which a "
           "self-signed one is reached past with insecure=1";
  if (status == QUIC_STATUS_INVALID_PARAMETER)
    return "the library refused what it was given";
  if (status == QUIC_STATUS_NOT_SUPPORTED)
    return "the quic library on this machine does not carry that";
  return "quic status " + std::to_string(static_cast<unsigned>(status));
}

QUIC_SETTINGS feedSettings() {
  QUIC_SETTINGS settings{};
  settings.IdleTimeoutMs = kIdleMs;
  settings.IsSet.IdleTimeoutMs = 1;
  settings.HandshakeIdleTimeoutMs = kReachWithinMs;
  settings.IsSet.HandshakeIdleTimeoutMs = 1;
  settings.KeepAliveIntervalMs = kKeepAliveMs;
  settings.IsSet.KeepAliveIntervalMs = 1;
  settings.PeerUnidiStreamCount = kMessagesInFlight;
  settings.IsSet.PeerUnidiStreamCount = 1;
  settings.DatagramReceiveEnabled = 1;
  settings.IsSet.DatagramReceiveEnabled = 1;
  return settings;
}

HQUIC configurationFor(const QUIC_CREDENTIAL_CONFIG& credential,
                       QUIC_STATUS& why) {
  const Library& lib = library();
  QUIC_SETTINGS settings = feedSettings();
  const QUIC_BUFFER named = protocol();
  HQUIC configuration = nullptr;
  why = lib.api->ConfigurationOpen(lib.registration, &named, 1, &settings,
                                   sizeof(settings), nullptr, &configuration);
  if (QUIC_FAILED(why)) return nullptr;
  why = lib.api->ConfigurationLoadCredential(configuration, &credential);
  if (QUIC_FAILED(why)) {
    lib.api->ConfigurationClose(configuration);
    return nullptr;
  }
  return configuration;
}

}  // namespace sigil::io::quic
