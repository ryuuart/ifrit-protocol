/** @file
 * The peer a message goes to, the one send, the repeat the caller's
 * clock drives, and the messages a reader spells: an OSC packet, a MIDI
 * message and a universe of dimmers.
 */

#include "sigilseer/wire/Sender.h"

#include <sigildata/decode/ArtNet.h>
#include <sigildata/decode/Json.h>
#include <sigildata/decode/Midi.h>
#include <sigildata/decode/Osc.h>

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "sigilseer/wire/Wires.h"

namespace sigil::seer {
namespace {

/** Whether @p text holds anything but blanks. An editor a reader has
 *  typed nothing into is a message with no arguments, not a document
 *  that would not parse. */
bool anythingIn(std::string_view text) {
  for (const char letter : text)
    if (letter != ' ' && letter != '\t' && letter != '\n' && letter != '\r')
      return true;
  return false;
}

}  // namespace

io::Bytes oscMessage(std::string_view address, std::string_view arguments) {
  data::Json carried{data::Json::Array{}};
  if (anythingIn(arguments)) {
    std::optional<data::Json> read = data::decodeJson(arguments);
    if (!read) return {};
    carried = std::move(*read);
  }
  io::Bytes message;
  message.bytes = data::encodeOsc(address, carried);
  return message;
}

io::Bytes midiMessage(std::string_view kind, int channel, int first,
                      int second) {
  data::Json::Object played{{"kind", data::Json(std::string(kind))},
                            {"channel", data::Json(channel)}};
  // The numbers under the names that kind calls them, which is what the
  // codec writes its data bytes from: a message spelled by its fields
  // and not by its bytes is one a reader can read back.
  if (kind == "NoteOn" || kind == "NoteOff") {
    played.push_back({"note", data::Json(first)});
    played.push_back({"velocity", data::Json(second)});
  } else if (kind == "PolyAftertouch") {
    played.push_back({"note", data::Json(first)});
    played.push_back({"pressure", data::Json(second)});
  } else if (kind == "ControlChange") {
    played.push_back({"controller", data::Json(first)});
    played.push_back({"value", data::Json(second)});
  } else if (kind == "ProgramChange") {
    played.push_back({"program", data::Json(first)});
  } else if (kind == "Aftertouch") {
    played.push_back({"pressure", data::Json(first)});
  } else if (kind == "PitchBend") {
    played.push_back({"bend", data::Json(first)});
  } else {
    // A kind with no status byte behind it is no message: half a
    // message spelled is not a shorter one.
    return {};
  }
  io::Bytes message;
  message.bytes = data::encodeMidi(data::Json(std::move(played)));
  return message;
}

io::Bytes dmxMessage(int universe, std::string_view channels) {
  const std::optional<data::Json> levels = data::decodeJson(channels);
  // A universe is a list of levels and nothing else. One number where a
  // list was meant would go out as one fixture lit and the rest of the
  // rig dark, which is a thing a desk says and never a thing a reader
  // meant to say by typing it.
  if (!levels || levels->kind() != data::Json::Kind::List) return {};
  io::Bytes packet;
  packet.bytes = data::encodeArtNet(
      data::Json(data::Json::Object{{"kind", data::Json("Dmx")},
                                    {"universe", data::Json(universe)},
                                    {"channels", *levels}}));
  return packet;
}

Sender::Sender(Wires& wires) : m_wires(wires) {}

std::shared_ptr<io::Feed> Sender::openPeer(std::string_view uri) {
  m_peer = m_wires.open(uri);
  m_peerUri = std::string(uri);
  return m_peer;
}

bool Sender::send(const io::Bytes& bytes) {
  if (!m_peer || !m_peer->send(bytes)) return false;
  ++m_sent;
  return true;
}

void Sender::repeat(io::Bytes bytes, double period) {
  if (period <= 0) {
    stopRepeating();
    return;
  }
  m_message = std::move(bytes);
  m_period = period;
  m_repeating = true;
  m_due.reset();
}

void Sender::stopRepeating() {
  m_repeating = false;
  m_due.reset();
}

void Sender::tick(double seconds) {
  if (!m_repeating) return;
  if (m_due && seconds < *m_due) return;
  send(m_message);
  m_due = seconds + m_period;
}

}  // namespace sigil::seer
