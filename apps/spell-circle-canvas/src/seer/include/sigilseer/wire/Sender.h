#pragma once

/** @file
 * SENDING DOWN A WIRE: the peer a message goes to, one message or the
 * same message again and again, driven by the caller's own clock, and
 * the messages a reader spells rather than types out byte by byte.
 *
 * A wire that speaks a format is answered in what it speaks. The
 * hexadecimal a reader would otherwise type is a header, a set of type
 * tags, a status byte or a run of padding all at once — which is a
 * message nobody spells twice without a mistake in it — so the three
 * wires that name their format carry the message a reader means instead:
 * an address with its arguments, a note played on a channel, a universe
 * of dimmers.
 *
 * The peer is a wire like any other — it is opened through Wires, it
 * stands in the same list, and what the peer sends back arrives on it —
 * so a reader watches the answer to what was just sent without opening
 * anything else. The repeat has no thread: `tick()` is what sends, so a
 * host that stops calling it stops sending, and nothing is in flight
 * once it returns.
 */

#include <sigilio/hub/Feed.h>
#include <sigilio/source/Source.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace sigil::seer {

class Wires;

/** THE BYTES OF ONE OSC MESSAGE: @p address, and the arguments
 *  @p arguments spells as a JSON document — a list being the arguments
 *  in order, any other value the one argument it is, and nothing at all
 *  a message carrying none. Empty when the address is empty, when the
 *  arguments are not a document, and when the message will not fit one
 *  packet, so half a message never goes out.
 *
 *  An address is where on the instrument at the other end the message
 *  lands, and it is the one part of a packet that is not an argument. */
io::Bytes oscMessage(std::string_view address, std::string_view arguments);

/** THE BYTES OF ONE MIDI MESSAGE: @p kind — "NoteOn", "NoteOff",
 *  "PolyAftertouch", "ControlChange", "ProgramChange", "Aftertouch" or
 *  "PitchBend" — played on @p channel, 1 to 16, carrying @p first and
 *  @p second, which are the numbers that kind takes in the order the
 *  wire carries them: the note and how hard it was struck, the key and
 *  the weight leaned on it, the controller and where it now stands. A
 *  kind that takes one number — a program, a whole keyboard's weight, a
 *  wheel's distance from centre — takes @p first and leaves @p second
 *  off the wire.
 *
 *  Empty for a kind the wire has no status byte for, so half a message
 *  never goes out. A number past what its place on the wire holds is
 *  written at the nearer end of that place rather than wrapped, since a
 *  wrapped note is a note nobody played. */
io::Bytes midiMessage(std::string_view kind, int channel, int first,
                      int second);

/** ONE MIDI MESSAGE WRITTEN IN WORDS: the kind, the channel it is played
 *  on, and the numbers that kind carries, which is the same message
 *  `midiMessage()` spells and is how it is said where there is no form
 *  to fill in. The second number stands at nothing for a kind that
 *  carries one, since the wire carries none for it. */
struct MidiWords {
  std::string kind;
  int channel = 1;
  int first = 0;
  int second = 0;
};

/** WHAT @p words SAY: the kind, the channel and the numbers that kind
 *  carries, blanks between them and nothing else — "NoteOn 1 60 100" for
 *  a kind that carries two numbers and "ProgramChange 2 7" for a kind
 *  that carries one. Nothing when the first word is no kind a channel
 *  carries, when a number is not written out whole, and when there are
 *  more or fewer numbers than that kind takes, so a number nobody said
 *  is never played as one they did. */
std::optional<MidiWords> midiWords(std::string_view words);

/** THE BYTES OF ONE ART-NET PACKET: a universe of dimmers for
 *  @p universe, whose levels @p channels spells as a JSON list of
 *  numbers — each 0 to 255, in the order the desk numbers them, so the
 *  channel a desk calls 1 is the first of the list. Empty when the
 *  levels are not a list, so a desk is never sent half a universe; a
 *  list with nothing in it is every fixture dark, which is a thing a
 *  desk says. */
io::Bytes dmxMessage(int universe, std::string_view channels);

/** THE WAY OUT: one peer, and the message that goes to it. */
class Sender {
 public:
  explicit Sender(Wires& wires);

  /** Opens @p uri as the peer every send reaches from now on, through
   *  the wires this was made over. A peer that cannot be opened is
   *  answered all the same, carrying the sentence that says why. */
  std::shared_ptr<io::Feed> openPeer(std::string_view uri);

  /** The peer, or null before one is opened. */
  std::shared_ptr<io::Feed> peer() const;

  /** The URI the peer was opened on; empty before one is opened. */
  const std::string& peerUri() const { return m_peerUri; }

  /** Sends @p bytes to the peer once. False when there is no peer, when
   *  the wire is one-way, and when it is closed. */
  bool send(const io::Bytes& bytes);

  /** Sends @p bytes every @p period seconds for as long as tick() runs,
   *  beginning with the next tick. A period that is not positive stops
   *  the repeat instead. */
  void repeat(io::Bytes bytes, double period);

  /** Takes the repeat off; the peer and the message it was sending
   *  stay. */
  void stopRepeating();

  bool repeating() const { return m_repeating; }
  double period() const { return m_period; }

  /** Sends the repeated message when one is due at @p seconds on the
   *  caller's clock. A host that stalled sends one message and waits a
   *  whole period again: what was missed while nothing was running is
   *  not made up for in a burst. */
  void tick(double seconds);

  /** How many messages this sender has put on a wire. */
  uint64_t sent() const { return m_sent; }

 private:
  Wires& m_wires;
  std::string m_peerUri;
  io::Bytes m_message;
  bool m_repeating = false;
  double m_period = 0;
  /** When the next repeated message is due; nothing until the first
   *  tick after the repeat began, which is what makes that tick send. */
  std::optional<double> m_due;
  uint64_t m_sent = 0;
};

}  // namespace sigil::seer
