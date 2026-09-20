#include <pybind11/stl.h>
#include <sigilcompose/kit/Document.h>
#include <sigilpython/compose/Convert.h>
#include <sigilpython/compose/Kit.h>

#include <sigilpython/compose/Registration.h>
#include <string>
#include <utility>

namespace sigil::python {
namespace py = pybind11;
namespace {
namespace document = compose::document;

template <auto Factory>
void textComponent(py::module_& module, const char* name) {
  module.def(
      name,
      [](const std::string& words) { return Factory(compose::Utf8(words)); },
      py::arg("words"));
}
}  // namespace

void bindDocument(py::module_& compose) {
  auto module = compose.def_submodule("document");
  module.attr("measure") = py::str(document::measure);
  module.attr("gap") = py::str(document::gap);
  module.attr("listGap") = py::str(document::listGap);
  module.attr("quoteInset") = py::str(document::quoteInset);

  module.def("article", [](py::args children) {
    return document::article().children(elements(children));
  });
  module.def("section", [](py::args children) {
    return document::section().children(elements(children));
  });
  module.def("list", [](py::args children) {
    return document::list().children(elements(children));
  });
  module.def(
      "quote", [](const std::string& words) { return document::quote(words); },
      py::arg("words"));
  module.def("quote", [](py::args children) {
    return document::quote().children(elements(children));
  });
  module.def(
      "heading",
      [](int level, const std::string& words) {
        return document::heading(level, words);
      },
      py::arg("level"), py::arg("words"));
  textComponent<document::h1>(module, "h1");
  textComponent<document::h2>(module, "h2");
  textComponent<document::h3>(module, "h3");
  textComponent<document::h4>(module, "h4");
  textComponent<document::h5>(module, "h5");
  textComponent<document::h6>(module, "h6");
  module.def(
      "paragraph",
      [](const std::string& words) { return document::paragraph(words); },
      py::arg("words"));
  module.def("paragraph",
             py::overload_cast<const weave::RichText&>(&document::paragraph),
             py::arg("words"));
  textComponent<document::lead>(module, "lead");
  textComponent<document::caption>(module, "caption");
  textComponent<document::label>(module, "label");
  textComponent<document::eyebrow>(module, "eyebrow");
  textComponent<document::footer>(module, "footer");
  textComponent<document::code>(module, "code");
  module.def(
      "item",
      [](const std::string& words, const std::string& marker) {
        return document::item(words, marker);
      },
      py::arg("words"), py::arg("marker") = "\xe2\x80\xa2");
  module.def(
      "item",
      [](compose::Element body, const std::string& marker) {
        return document::item(std::move(body), marker);
      },
      py::arg("body"), py::arg("marker") = "\xe2\x80\xa2");
  module.def(
      "figure",
      [](compose::Element body, const std::string& note) {
        return document::figure(std::move(body), note);
      },
      py::arg("body"), py::arg("note") = "");
  module.def("rule", &document::rule);
}
}  // namespace sigil::python
