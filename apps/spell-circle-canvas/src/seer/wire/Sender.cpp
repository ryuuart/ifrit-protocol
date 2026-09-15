/** @file
 * The peer a message goes to, the one send, the repeat the caller's
 * clock drives, and the messages a reader spells: an OSC packet, a MIDI
 * message written in fields or in words, and a universe of dimmers.
 */

#include "sigilseer/wire/Sender.h"

#include <sigildata/decode/ArtNet.h>
#include <sigildata/decode/Json.h>
#include <sigildata/decode/Midi.h>
#include <sigildata/decode/Osc.h>

#include <charconv>
#include <cstddef>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "sigilseer/wire/Wires.h"

namespace sigil::seer {
namespace {

/** Whether @p letter is one of the characters that stand between words
 *  rather than one a word is made of. */
bool isBlank(char letter) {
  return letter == ' ' || letter == '\t' || letter == '\n' || letter == '\r';
}

/** Whether @p text holds anything but blanks. An editor a reader has
 *  typed nothing into is a message with no arguments, not a document
 *  that would not parse. */
bool anythingIn(std::string_view text) {
  for (const char letter : text)
    if (!isBlank(letter)) return true;
  return false;
}

/** THE NUMBERS A KIND CARRIES, under the names the codec writes them by:
 *  one name and no second for a kind that carries one number, and no
 *  name at all for a kind the wire has no status byte for. It is the one
 *  place that says how many numbers a kind takes, so a message spelled
 *  from fields and a message read out of words carry the same ones. */
struct Numbers {
  const char* first = nullptr;
  const char* second = nullptr;
};

Numbers numbersOf(std::string_view kind) {
  if (kind == "NoteOn" || kind == "NoteOff") return {"note", "velocity"};
  if (kind == "PolyAftertouch") return {"note", "pressure"};
  if (kind == "ControlChange") return {"controller", "value"};
  if (kind == "ProgramChange") return {"program", nullptr};
  if (kind == "Aftertouch") return {"pressure", nullptr};
  if (kind == "PitchBend") return {"bend", nullptr};
  return {};
}

/** @p word read as a whole number into @p number. False when it holds
 *  anything besides one, a wheel's minus sign included, since a number
 *  read up to the character that stopped it is a number nobody wrote. */
bool numberOf(std::string_view word, int& number) {
  const char* const past = word.data() + word.size();
  const std::from_chars_result read =
      std::from_chars(word.data(), past, number);
  return read.ec == std::errc() && read.ptr == past;
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
  const Numbers numbers = numbersOf(kind);
  // A kind with no status byte behind it is no message: half a message
  // spelled is not a shorter one.
  if (!numbers.first) return {};
  // The numbers under the names that kind calls them, which is what the
  // codec writes its data bytes from: a message spelled by its fields
  // and not by its bytes is one a reader can read back.
  data::Json::Object played{{"kind", data::Json(std::string(kind))},
                            {"channel", data::Json(channel)}};
  played.push_back({numbers.first, data::Json(first)});
  if (numbers.second) played.push_back({numbers.second, data::Json(second)});
  io::Bytes message;
  message.bytes = data::encodeMidi(data::Json(std::move(played)));
  return message;
}

std::optional<MidiWords> midiWords(std::string_view words) {
  std::vector<std::string_view> said;
  for (size_t at = 0; at != words.size();) {
    while (at != words.size() && isBlank(words[at])) ++at;
    const size_t from = at;
    while (at != words.size() && !isBlank(words[at])) ++at;
    if (at != from) said.push_back(words.substr(from, at - from));
  }
  if (said.empty()) return std::nullopt;

  MidiWords spelled;
  spelled.kind = std::string(said.front());
  const Numbers numbers = numbersOf(spelled.kind);
  if (!numbers.first) return std::nullopt;
  // The channel and the numbers that kind carries, and not one word
  // more: a kind given a number it does not take was meant as another
  // kind, and one short of what it takes would be played with a number
  // nobody said.
  if (said.size() != (numbers.second ? 4u : 3u)) return std::nullopt;
  if (!numberOf(said[1], spelled.channel)) return std::nullopt;
  if (!numberOf(said[2], spelled.first)) return std::nullopt;
  if (numbers.second && !numberOf(said[3], spelled.second)) return std::nullopt;
  return spelled;
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
