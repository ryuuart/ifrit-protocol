#include <pybind11/operators.h>
#include <pybind11/stl.h>
#include <sigilcompose/core/Cascade.h>
#include <sigilcompose/core/Element.h>
#include <sigilcompose/core/Selector.h>
#include <sigilcompose/core/StyleSheet.h>
#include <sigilcompose/core/Var.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmotion/values/Transition.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/Extend.h>
#include <sigilpython/compose/Convert.h>
#include <sigilpython/compose/Nodes.h>
#include <sigilpython/compose/Registration.h>

#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace sigil::python {
namespace py = pybind11;

namespace {

constexpr auto fluent = py::return_value_policy::reference_internal;
using compose::Element;
using compose::ElementSelector;
using compose::RelativeSelector;
using compose::Rule;
using compose::Specificity;
using compose::StyleSheet;

/** The sheet the statements in @p statements state, in order: a rule
 *  stands for itself and a sheet for the rules it holds. */
StyleSheet sheetOf(const py::iterable& statements) {
  StyleSheet whole;
  for (py::handle statement : statements) {
    if (py::isinstance<StyleSheet>(statement)) {
      whole = whole + statement.cast<StyleSheet>();
    } else if (py::isinstance<Rule>(statement)) {
      whole = whole + StyleSheet{statement.cast<Rule>()};
    } else {
      throw py::type_error(
          "A sheet statement is a compose.Rule or another "
          "compose.StyleSheet.");
    }
  }
  return whole;
}

/** A weight written out, which is both its own repr and the tail of the
 *  repr of everything that carries one. */
std::string weightText(const Specificity& weight) {
  return "Specificity(classes=" + std::to_string(weight.classes) +
         ", roles=" + std::to_string(weight.roles) + ")";
}

/** What CSS weighs a selector at, as the pair it counts where nothing
 *  carries an id. */
void bindSpecificity(py::module_& composition) {
  py::class_<Specificity> specificity(
      composition, "Specificity",
      "How heavily a selector weighs: the classes and pseudo-classes it "
      "names, then the roles. The pair compares left to right, so one "
      "class outweighs any number of roles, and combinators and `*` weigh "
      "nothing at all.");

  specificity
      .def(py::init([](int classes, int roles) {
             return Specificity{classes, roles};
           }),
           py::arg("classes") = 0, py::arg("roles") = 0,
           "A weight of `classes` classes and `roles` roles.")
      .def_readwrite("classes", &Specificity::classes,
                     "The style classes and pseudo-classes the selector "
                     "names.")
      .def_readwrite("roles", &Specificity::roles,
                     "The roles it names, which is what a CSS type "
                     "selector is here.")
      .def("copy", [](const Specificity& self) { return self; })
      .def(py::self == py::self)
      .def(py::self != py::self)
      // The ordering is spelled out rather than taken from the operator
      // helpers, because those leave their one argument unnamed and
      // every declaration this package writes names what it takes.
      .def(
          "__lt__",
          [](const Specificity& self, const Specificity& other) {
            return self < other;
          },
          py::arg("other"), py::is_operator())
      .def(
          "__le__",
          [](const Specificity& self, const Specificity& other) {
            return self <= other;
          },
          py::arg("other"), py::is_operator())
      .def(
          "__gt__",
          [](const Specificity& self, const Specificity& other) {
            return self > other;
          },
          py::arg("other"), py::is_operator())
      .def(
          "__ge__",
          [](const Specificity& self, const Specificity& other) {
            return self >= other;
          },
          py::arg("other"), py::is_operator())
      .def("__hash__",
           [](const Specificity& self) {
             return py::hash(py::make_tuple(self.classes, self.roles));
           })
      .def("__repr__",
           [](const Specificity& self) { return weightText(self); });
  copyProtocol(specificity);
}

/** The selector itself: the combinators, the structural pseudo-classes
 *  and the three set operators. */
void bindElementSelector(py::module_& composition) {
  py::class_<ElementSelector> selector(
      composition, "ElementSelector",
      "Which elements a rule speaks about, as one immutable value: either "
      "a list of alternatives, which `|` builds, or one chain of compounds "
      "joined by combinators whose last compound is the SUBJECT, the "
      "element the rule actually styles. Every method answers a NEW "
      "selector. A default-built one, and one a bad CSS text produced, "
      "match nothing, and nothing absorbs: compounding or qualifying it "
      "matches nothing too, so a misprint can only narrow a rule away. "
      "`a.child(b)` reads \"a then b\", so the subject is `b`. Equality is "
      "structural; there is no reading that answers the text back, so a "
      "selector carries no hash and cannot key a dictionary or join a "
      "set.");

  selector
      .def(py::init<>(),
           "The selector that matches nothing, which is the zero of this "
           "algebra.")
      .def("child", &ElementSelector::child, py::arg("subject"),
           "This selector, then `subject` as its direct child — CSS "
           "`a > b`.")
      .def("descendant", &ElementSelector::descendant, py::arg("subject"),
           "This selector, then `subject` anywhere under it — CSS `a b`.")
      .def("next", &ElementSelector::next, py::arg("subject"),
           "This selector, then `subject` as the sibling immediately after "
           "it — CSS `a + b`.")
      .def("sibling", &ElementSelector::sibling, py::arg("subject"),
           "This selector, then `subject` as any later sibling of it — CSS "
           "`a ~ b`.")
      .def("firstChild", &ElementSelector::firstChild,
           "The subject only where it is its parent's first child.")
      .def("lastChild", &ElementSelector::lastChild,
           "The subject only where it is its parent's last child.")
      .def("onlyChild", &ElementSelector::onlyChild,
           "The subject only where it is its parent's one and only child.")
      .def(
          "nthChild",
          [](const ElementSelector& self, int step, int offset,
             std::optional<ElementSelector> of) {
            if (of) return self.nthChild(step, offset, std::move(*of));
            return self.nthChild(step, offset);
          },
          py::arg("step"), py::arg("offset"), py::arg("of") = py::none(),
          "The subject only at the positions `step * n + offset` counted "
          "from the first child, n running 0, 1, 2… — CSS "
          "`:nth-child(an+b)`, whose `odd` is `(2, 1)` and `even` is "
          "`(2, 0)`. Given `of`, the count is taken over only the siblings "
          "matching it.")
      .def(
          "nthLastChild",
          [](const ElementSelector& self, int step, int offset,
             std::optional<ElementSelector> of) {
            if (of) return self.nthLastChild(step, offset, std::move(*of));
            return self.nthLastChild(step, offset);
          },
          py::arg("step"), py::arg("offset"), py::arg("of") = py::none(),
          "`nthChild`, counted from the last child instead.")
      .def("firstOfType", &ElementSelector::firstOfType,
           "The subject only where it is the first sibling of its ROLE, "
           "which is what a type is here. An element with no role has no "
           "type, so no of-type pseudo-class matches it.")
      .def("lastOfType", &ElementSelector::lastOfType,
           "The last sibling of the subject's role.")
      .def("onlyOfType", &ElementSelector::onlyOfType,
           "The only sibling of the subject's role.")
      .def("nthOfType", &ElementSelector::nthOfType, py::arg("step"),
           py::arg("offset"),
           "`nthChild`'s count taken over the siblings of the subject's "
           "role alone — CSS `:nth-of-type(an+b)`, which takes no `of` "
           "filter.")
      .def("nthLastOfType", &ElementSelector::nthLastOfType, py::arg("step"),
           py::arg("offset"),
           "`nthOfType`, counted from the last sibling instead.")
      .def("empty", &ElementSelector::empty,
           "CSS :empty: the subject only where it has no children and no "
           "words — a text leaf holding any text is not empty.")
      .def("root", &ElementSelector::root,
           "The subject only where it is the root of the tree its sheet "
           "sees: the node that applied the sheet, or the tree's own root "
           "where the sheet stands on it.")
      .def("specificity", &ElementSelector::specificity,
           "What CSS weighs this selector at: the heaviest alternative "
           "where it is a list, and the sum over every compound where it "
           "is a chain.")
      .def("matchesNothing", &ElementSelector::matchesNothing,
           "Whether this selector can never match: default-built, or built "
           "from a CSS text this library does not read.")
      .def(
          "__or__",
          [](const ElementSelector& left, const ElementSelector& right) {
            return left | right;
          },
          py::arg("other"), py::is_operator(),
          "A selector list, CSS's comma: either side matching is a match. "
          "A side that matches nothing leaves the other standing, which is "
          "the one place nothing is the identity rather than the zero.")
      .def(
          "__and__",
          [](const ElementSelector& left, const ElementSelector& right) {
            return left & right;
          },
          py::arg("other"), py::is_operator(),
          "A compound, CSS `.card.wide`: both sides matching the SAME "
          "element. The right side folds into the left's subject compound, "
          "so a right side that is itself a chain matches nothing.")
      .def(
          "__invert__", [](const ElementSelector& self) { return !self; },
          py::is_operator(),
          "A negation, CSS `:not(...)`, which `select.notAnyOf` also "
          "spells. Python has no `!`, so the house's negation is written "
          "`~`.")
      .def("copy", [](const ElementSelector& self) { return self; })
      .def(py::self == py::self)
      .def(py::self != py::self)
      .def("__repr__", [](const ElementSelector& self) {
        if (self.matchesNothing())
          return std::string("<compose.ElementSelector matching nothing>");
        return "<compose.ElementSelector weighing " +
               weightText(self.specificity()) + ">";
      });
  copyProtocol(selector);
  // Equality is structural over a shared body the bindings cannot read
  // back, so no hash can agree with it.
  selector.attr("__hash__") = py::none();
}

/** What a `:has()` looks for: a chain opening with its relation to the
 *  element the `:has()` stands on. */
void bindRelativeSelector(py::module_& composition) {
  py::class_<RelativeSelector> relative(
      composition, "RelativeSelector",
      "What a `:has()` looks for, read from the element it stands on — "
      "CSS's relative selector: a chain whose first compound is reached "
      "from that element by a relation, anywhere under it (a plain "
      "`ElementSelector` stands for that), as a direct child, as the next "
      "sibling or as any later sibling. Only `select.has` takes one, so a "
      "relation cannot open a rule, where it would speak about nothing. "
      "`select.child(a).descendant(b)` is `> a b`: the relation opens the "
      "chain and the methods extend it.");
  relative
      .def(py::init<ElementSelector>(), py::arg("descendant"),
           "`descendant` reached anywhere under the element — CSS "
           "`:has(b)`.")
      .def("child", &RelativeSelector::child, py::arg("subject"),
           "This chain, then `subject` as its direct child.")
      .def("descendant", &RelativeSelector::descendant, py::arg("subject"),
           "This chain, then `subject` anywhere under it.")
      .def("next", &RelativeSelector::next, py::arg("subject"),
           "This chain, then `subject` as the sibling immediately after "
           "it.")
      .def("sibling", &RelativeSelector::sibling, py::arg("subject"),
           "This chain, then `subject` as any later sibling of it.")
      .def("matchesNothing", &RelativeSelector::matchesNothing,
           "Whether this can never be reached: default-built, or built from "
           "a selector that matches nothing.")
      .def(
          "__or__",
          [](const RelativeSelector& left, const RelativeSelector& right) {
            return left | right;
          },
          py::arg("other"), py::is_operator(),
          "A list of relative selectors, either one reaching being a match "
          "— the comma inside CSS's `:has(+ a, ~ b)`. A plain selector "
          "reaches anywhere under the element.")
      .def(
          "__ror__",
          [](const RelativeSelector& right, const ElementSelector& left) {
            return RelativeSelector(left) | right;
          },
          py::arg("other"), py::is_operator())
      .def("copy", [](const RelativeSelector& self) { return self; })
      .def(py::self == py::self)
      .def(py::self != py::self)
      .def("__repr__", [](const RelativeSelector& self) {
        return std::string(self.matchesNothing()
                               ? "<compose.RelativeSelector reaching nothing>"
                               : "<compose.RelativeSelector>");
      });
  copyProtocol(relative);
  relative.attr("__hash__") = py::none();
  py::implicitly_convertible<ElementSelector, RelativeSelector>();
}

/** The typed front door: the same selectors built name by name. */
void bindSelectFactories(py::module_& factories) {
  factories.doc() =
      "The typed front door onto the selector grammar, for the author who "
      "would rather not write a CSS string. `|` is a selector list, `&` a "
      "compound on one element, `~` a negation.";

  factories.def("styleClass", &compose::select::styleClass, py::arg("name"),
                "Elements carrying `name` among their style classes — CSS "
                "`.name`.");
  factories.def("role", &compose::select::role, py::arg("name"),
                "Elements whose role is `name` — CSS's type selector, a "
                "bare word.");
  factories.def("any", &compose::select::any,
                "Any element at all — CSS `*`, which weighs nothing.");
  factories.def("is_", &compose::select::is, py::arg("alternatives"),
                "Elements matching any of `alternatives`, weighing as the "
                "heaviest of them — CSS `:is(...)`. The native name is "
                "`is`, which Python reserves, so it is spelled with a "
                "trailing underscore as every reserved name is here.");
  factories.def("notAnyOf", &compose::select::notAnyOf, py::arg("alternatives"),
                "Elements matching none of `alternatives`, weighing as the "
                "heaviest of them — CSS `:not(...)`, which `~` also "
                "spells.");
  factories.def("where", &compose::select::where, py::arg("alternatives"),
                "Elements matching any of `alternatives`, weighing NOTHING "
                "at all — CSS `:where(...)`, which is how a default "
                "anything can override is stated.");
  factories.def("has", &compose::select::has, py::arg("relatives"),
                "Elements from which some of `relatives` can be reached — "
                "CSS `:has(...)`, weighing as the heaviest of them. A plain "
                "selector is reached anywhere under the element; `child`, "
                "`next` and `sibling` name the other three relations. "
                "`has(a | b)` asks for either, `has(a) & has(b)` for both. "
                "A `:has()` inside another matches nothing, as in CSS.");
  factories.def("child", &compose::select::child, py::arg("subject"),
                "A relative selector for `has`: `subject` as a direct child "
                "of the element the `:has()` stands on — CSS `:has(> b)`.");
  factories.def("next", &compose::select::next, py::arg("subject"),
                "A relative selector for `has`: `subject` as the sibling "
                "immediately after that element — CSS `:has(+ b)`.");
  factories.def("sibling", &compose::select::sibling, py::arg("subject"),
                "A relative selector for `has`: `subject` as any later "
                "sibling of that element — CSS `:has(~ b)`.");
}

/** One rule: a selector and the partials it lays on every element that
 *  selector speaks about. */
void bindRule(py::module_& composition) {
  py::class_<Rule> rule(
      composition, "Rule",
      "One rule of a selector sheet: which elements it speaks about, and "
      "what it states about them. Its verbs ARE the element's — the box, "
      "the flex line, the placement, the corners and overflow, the paint, "
      "the compositing lanes, the 2D transform and the cascade — bound "
      "once for both, and the element's own verb stands over the rule's. "
      "What a rule leaves unsaid the element inherits or keeps at its "
      "initial value. A rule holds STATIC values: a live binding, an "
      "entrance and an animation stay verbs on the element. It reaches an "
      "element through `Element.applyStyleSheet`. Equality is structural, "
      "and a rule carries no hash, because its selector carries none.");

  rule.def(py::init<ElementSelector>(), py::arg("subject"),
           "A rule speaking about the elements `subject` names, stating "
           "nothing yet.");
  // The verbs a rule shares with every node, and the text properties it
  // shares with a text leaf, each bound once for both.
  bindDeclarationVerbs(rule);
  bindTextPropertyVerbs(rule);
  rule.def(
          "transition",
          [](Rule& self, motion::Transition how) -> Rule& {
            return self.transition(std::move(how));
          },
          py::arg("how"), fluent,
          "How a matched element's values change when a later describe "
          "moves them, so a class toggle that recolours an element eases "
          "rather than snapping. The element's own `transition` stands "
          "over it, and among matched rules the strongest that states one "
          "wins.")
      .def("selector", &Rule::selector, py::return_value_policy::copy,
           "Which elements this rule speaks about.")
      .def("type", &Rule::type, py::return_value_policy::copy,
           "The font half of what it states.")
      .def("block", py::overload_cast<>(&Rule::block, py::const_),
           py::return_value_policy::copy, "The block half of what it states.")
      .def("copy", [](const Rule& self) { return self; })
      .def(py::self == py::self)
      .def(py::self != py::self)
      .def("__repr__", [](const Rule& self) {
        if (self.selector().matchesNothing())
          return std::string("<compose.Rule speaking about nothing>");
        return "<compose.Rule weighing " +
               weightText(self.selector().specificity()) + ">";
      });
  copyProtocol(rule);
  rule.attr("__hash__") = py::none();
}

/** The ordered sheet of rules, and the verb that puts one in force. */
void bindStyleSheet(py::module_& composition) {
  py::class_<StyleSheet> sheet(
      composition, "StyleSheet",
      "The rules a sheet states, in the order they were written, as one "
      "immutable value: declared once and applied at as many subtrees as "
      "the author likes. A statement is a `compose.Rule` or another "
      "`compose.StyleSheet`, whose rules then stand in its place. Order is "
      "only the LAST tiebreak: which rule wins at an element is CSS's, "
      "specificity first and order after it. It is the one sheet an author "
      "writes, put in force with `Element.applyStyleSheet`. Two sheets "
      "compare by value, and a sheet carries no hash, because its rules "
      "carry none.");

  sheet
      .def(py::init([](const py::iterable& statements) {
             return sheetOf(statements);
           }),
           py::arg("statements") = py::tuple(),
           "The sheet these statements state, in order.")
      .def(
          "rules", [](const StyleSheet& self) { return self.rules(); },
          "Every rule the sheet states, an included sheet's rules standing "
          "where it stood. Read as an owned list.")
      .def("size", &StyleSheet::size, "How many rules the sheet states.")
      .def("empty", &StyleSheet::empty, "Whether it states no rule at all.")
      .def("__len__", &StyleSheet::size)
      .def("__iter__",
           [](const StyleSheet& self) {
             return py::iter(py::cast(self.rules()));
           })
      .def(
          "__add__",
          [](const StyleSheet& earlier, const StyleSheet& later) {
            return earlier + later;
          },
          py::arg("later"), py::is_operator(),
          "The two sheets as one, `later`'s rules after this one's. Later "
          "is only a tiebreak, so a later rule wins where the two weigh the "
          "same and loses where it weighs less.")
      .def("copy", [](const StyleSheet& self) { return self; })
      .def(py::self == py::self)
      .def(py::self != py::self)
      .def("__repr__", [](const StyleSheet& self) {
        return "<compose.StyleSheet of " + std::to_string(self.size()) +
               " rules>";
      });
  copyProtocol(sheet);
  sheet.attr("__hash__") = py::none();
}

}  // namespace

void bindComposeSelectors(pybind11::module_& module) {
  auto composition = submodule(module, "compose");
  auto factories = submodule(module, "compose.select");

  bindSpecificity(composition);
  bindElementSelector(composition);
  bindRelativeSelector(composition);
  bindSelectFactories(factories);
  bindRule(composition);
  bindStyleSheet(composition);

  composition.def(
      "selector", &compose::selector, py::arg("cssText"),
      "The CSS front door, parsed once into a selector value: "
      "`\".card > .title:first-child\"`, `\".row:nth-child(odd)\"`, "
      "`\":is(.a, .b) .c\"`, `\":not(.x)\"`, `\"*\"`. A text this library "
      "does not read warns once and matches nothing — including an `an+b` "
      "number too large to hold and a nesting too deep to read, which are "
      "REFUSED rather than clamped.");
  composition.def("rule", py::overload_cast<std::string_view>(&compose::rule),
                  py::arg("cssText"),
                  "A rule speaking about the elements `cssText` names. A "
                  "text this library does not read matches nothing, so the "
                  "rule states what it states about no element at all.");
  composition.def("rule", py::overload_cast<ElementSelector>(&compose::rule),
                  py::arg("subject"),
                  "A rule speaking about the elements `subject` names.");

  // The element class belongs to the file that registered it, and this
  // verb takes the sheet registered just above.
  extend<Element>(module, "compose.Element")
      .def("applyStyleSheet", &Element::applyStyleSheet, py::arg("sheet"),
           fluent,
           "Applies `sheet` to this node and everything under it. Calling "
           "this again applies another sheet, later in order; nothing is "
           "removed, because a tree that should stop applying one is "
           "described without it. THE SHEET SEES ONLY THIS SUBTREE: every "
           "compound of a selector — the subject, and every ancestor or "
           "sibling it names — must match this node or one below it, so a "
           "sheet that must name an outer element is applied at or above "
           "that element. This node is the root of what the sheet sees, so "
           "it matches `:root` and stands as the only child of nothing for "
           "these rules.");
}

}  // namespace sigil::python
