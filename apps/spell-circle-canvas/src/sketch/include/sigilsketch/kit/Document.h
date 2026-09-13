#pragma once

/** @file
 * THE WORDS A SKETCH SETS, read from beside it rather than typed into it:
 * the document as a value, the line it answers, and the figures a
 * sentence names.
 *
 * A page of prose is `passage()`; this is the other shape a sketch's words
 * come in — a record of named runs, each line a sentence and the class it
 * is set in, with the measurements the sketch took written into the
 * sentences that quote them.
 */

#include <sigilcompose/core/Element.h>
#include <sigilcompose/core/Utf8.h>
#include <sigildata/decode/Json.h>
#include <sigilweave/paragraph/RichText.h>

#include <concepts>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sigil::sketch {
/** The per-frame value a sketch body is handed; its own words are
 *  `<sigilsketch/canvas/Sketch.h>`, which this names and does not
 *  include. */
struct SketchContext;
}  // namespace sigil::sketch

namespace sigil::sketch::kit {

/** THE SKETCH'S OWN DOCUMENT: the JSON file beside it, read as the words
 *  the plate sets.
 *
 *      Document doc{ctx, "data/content.json"};
 *      doc.figure("rest", kit::formatted("%.2f", measured));
 *      …
 *      column().children({text(doc["masthead"]["title"]),
 *                         each(doc.run("notes"), lineOf)})
 *
 *  A DOCUMENT IS READ, NEVER DEMANDED. A missing file, a missing key and a
 *  key holding the wrong kind all answer nothing — no record, no words, no
 *  lines — so every reader states what it falls back to and a plate whose
 *  file did not arrive draws its furniture and none of its lettering. That
 *  is the one reading a live-edited file can have: a sentence deleted from
 *  the file deletes its line.
 *
 *  THE FIGURES ARE THE DOCUMENT'S, NOT THE LINE'S. `{name}` anywhere in a
 *  sentence is replaced by the figure of that name and left exactly as
 *  written where no figure carries it, so a sentence that quotes a
 *  measurement quotes the measurement the run took rather than a number
 *  typed beside it. They are already formatted when they arrive: how a
 *  number reads is the measurement's business.
 *
 *  IT DECIDES NO LOOK. A class is a name the document wrote and the sheet
 *  in force resolves; nothing here reads a theme.
 *
 *  Copyable and cheap: the document itself is shared, and the figures are
 *  the copy's own. */
class Document {
 public:
  /** ONE LINE OF THE DOCUMENT: the sentence, its figures already written
   *  in, and the class the document named for it.
   *
   *  AN EMPTY CLASS IS THE RUNNING VOICE the tree already carries, which
   *  is what a line whose look is the page's says. A line whose CONTENT
   *  decides its colour — a verdict, a warning, a school of thought —
   *  names its own, and the sheet in force says what that is. A line is
   *  the document's rather than the card's because what it carries is a
   *  name to be resolved and not a look: `kit::Line` beside it carries a
   *  card's own ink and beat, which a sentence read out of a file has
   *  nothing to say about. */
  struct Line {
    compose::Utf8 words;
    std::string styleClass;
    bool operator==(const Line&) const = default;
  };

  Document() = default;
  /** The document among the SKETCH'S OWN FILES, `ctx.local(name)` —
   *  `"data/content.json"` beside the sketch — through the resource hub,
   *  which caches it and notices an edit to it. */
  Document(SketchContext& ctx, std::string_view name);
  /** The document already in hand, for a sketch that loaded it itself. */
  explicit Document(std::shared_ptr<const data::Json> held)
      : m_held(std::move(held)) {}

  /** Whether a document arrived at all. */
  [[nodiscard]] bool loaded() const { return m_held != nullptr; }

  /** THE FIGURES THE SENTENCES NAME, under the names they call them by
   *  and already formatted: `{name}` anywhere in a sentence is replaced by
   *  the reading of that name.
   *
   *  It takes the whole table because a study's measurements arrive
   *  together — one statement after the run that took them — and a name
   *  stated again keeps the later reading, so a second call adds to what
   *  the first left. Chainable. */
  using Figures = std::vector<std::pair<std::string, compose::Utf8>>;
  Document& figures(Figures named);

  /** THE RECORD AT @p key, or a null value where there is none — so a
   *  chain of lookups through members that are not there answers null
   *  rather than crashing, and a node is text wherever text is taken. */
  [[nodiscard]] const data::Json& operator[](std::string_view key) const;

  /** THE ONE SENTENCE of @p node, with its figures written in. */
  [[nodiscard]] compose::Utf8 phrase(const data::Json& node) const;

  /** THE RUN OF LINES of @p node — the list `each()` walks. A bare string
   *  is that sentence in no class of its own; a record is
   *  `{"words": …, "class": …}`. */
  [[nodiscard]] std::vector<Line> run(const data::Json& node) const;

  /** THE PASSAGE at @p node as ONE mixed-text value: the same run of lines
   *  woven into one paragraph, each named line changing only what its
   *  class says and an unnamed one set in the voice the paragraph carries.
   *  It is how a bulletin's own emphasis survives being content rather
   *  than code. */
  [[nodiscard]] weave::RichText passage(const data::Json& node) const;

  /** THE SAME THREE AT A KEY of this document, which is how a sketch
   *  usually reads one; a NODE is what a run nested inside a record is
   *  read from.
   *
   *  The key is taken as a template rather than as a `std::string_view`
   *  because `data::Json` is constructible from a string: a plain overload
   *  pair would leave `doc.run("notes")` ambiguous between the key and a
   *  document node built out of it, where a deduced parameter matches the
   *  literal exactly and the node overload drops out of the set. */
  template <class Key>
    requires std::convertible_to<Key, std::string_view>
  [[nodiscard]] compose::Utf8 phrase(const Key& key) const {
    return phrase((*this)[std::string_view(key)]);
  }
  template <class Key>
    requires std::convertible_to<Key, std::string_view>
  [[nodiscard]] std::vector<Line> run(const Key& key) const {
    return run((*this)[std::string_view(key)]);
  }
  template <class Key>
    requires std::convertible_to<Key, std::string_view>
  [[nodiscard]] weave::RichText passage(const Key& key) const {
    return passage((*this)[std::string_view(key)]);
  }

 private:
  std::shared_ptr<const data::Json> m_held;
  std::vector<std::pair<std::string, std::string>> m_figures;
};

/** ONE LINE AS A LEAF, set in the class the document named for it — an
 *  unnamed line in the voice the tree already carries.
 *
 *      column().children({each(doc.run("notes"), lineOf)})
 */
[[nodiscard]] compose::Element lineOf(const Document::Line& one);

}  // namespace sigil::sketch::kit
