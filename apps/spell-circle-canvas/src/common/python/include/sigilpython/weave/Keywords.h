#pragma once

/** @file
 * The three wide keywords on a PARTIAL, as Python reaches them: the
 * surface a bound `Type` or `ParagraphBlock` takes so a field can be written as
 * `inherit`, `initial` or `unset` rather than as a value.
 *
 * A field's Python attribute takes the field's own type, and none of
 * those types can hold a keyword — so the keyword table is its own
 * three verbs, named for what they do to one field.
 */

#include <pybind11/pybind11.h>
#include <sigilweave/style/Keyword.h>

namespace sigil::python {

/** Gives @p partial the keyword surface: `keyword(field, keyword)` writes
 *  one, `keywordOf(field)` reads it back or answers `None`, and
 *  `clearKeyword(field)` takes it off again. @p Field is whatever
 *  enumerates the partial's fields — `weave.TypeField`,
 *  `weave.ParagraphField` — and the partial keeps the value the field held,
 *  because the keyword and the value are one layer whose later statement
 *  the writer decides. */
template <class Partial, class Field>
pybind11::class_<Partial>& bindKeywords(pybind11::class_<Partial>& partial) {
  namespace py = pybind11;
  partial
      .def(
          "keyword",
          [](Partial& self, Field field, sigil::weave::Keyword keyword) {
            self.keywords.set(field, keyword);
          },
          py::arg("field"), py::arg("keyword"))
      .def(
          "keywordOf",
          [](const Partial& self, Field field) {
            return self.keywords.find(field);
          },
          py::arg("field"))
      .def(
          "clearKeyword",
          [](Partial& self, Field field) { self.keywords.clear(field); },
          py::arg("field"));
  return partial;
}

}  // namespace sigil::python
