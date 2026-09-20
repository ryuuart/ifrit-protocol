"""A small tree of every shape the readers have to handle.

One library, one node type with its verbs, one factory, one kit
component, two values — one of them made by converting the other — one
enumeration, a name two namespaces share, a convenience Python has and
C++ has not, and a public surface that re-exports part of it under a
renamed spelling. Small enough to read, and it exercises every branch
the real tree exercises.
"""

import contextlib
import sys
import tempfile
import textwrap
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[3]))

COMPOUND = """<?xml version='1.0' encoding='UTF-8' standalone='no'?>
<doxygen version="1.18.0">
  <compounddef id="classsigil_1_1paint_1_1_brush" kind="class" prot="public">
    <compoundname>sigil::paint::Brush</compoundname>
    <sectiondef kind="public-func">
      <memberdef kind="function" id="classsigil_1_1paint_1_1_brush_1aaa" prot="public"
                 static="no" const="no" explicit="no">
        <type>Brush &amp;</type>
        <definition>Brush &amp; sigil::paint::Brush::tint</definition>
        <argsstring>(Ink colour)</argsstring>
        <name>tint</name>
        <qualifiedname>sigil::paint::Brush::tint</qualifiedname>
        <param><type><ref refid="structsigil_1_1paint_1_1_ink" kindref="compound">Ink</ref></type>
               <declname>colour</declname></param>
        <briefdescription><para>The colour the brush lays down. </para></briefdescription>
        <detaileddescription><para>A tint is resolved where the mark lands. </para></detaileddescription>
        <location file="sigilpaint/Brush.h" line="20" declfile="sigilpaint/Brush.h" declline="20"/>
      </memberdef>
      <memberdef kind="function" id="classsigil_1_1paint_1_1_brush_1abb" prot="public"
                 static="no" const="no" explicit="no">
        <type>Brush &amp;</type>
        <definition>Brush &amp; sigil::paint::Brush::width</definition>
        <argsstring>(float value)</argsstring>
        <name>width</name>
        <qualifiedname>sigil::paint::Brush::width</qualifiedname>
        <param><type>float</type><declname>value</declname></param>
        <briefdescription><para>How wide the mark is. </para></briefdescription>
        <detaileddescription/>
        <location file="sigilpaint/Brush.h" line="24" declfile="sigilpaint/Brush.h" declline="24"/>
      </memberdef>
      <memberdef kind="function" id="classsigil_1_1paint_1_1_brush_1acd" prot="public"
                 static="no" const="yes" explicit="no">
        <type>Brush</type>
        <definition>Brush sigil::paint::Brush::dry</definition>
        <argsstring>() const</argsstring>
        <name>dry</name>
        <qualifiedname>sigil::paint::Brush::dry</qualifiedname>
        <briefdescription><para>A second brush, as this one stands with no ink on it. </para></briefdescription>
        <detaileddescription/>
        <location file="sigilpaint/Brush.h" line="28" declfile="sigilpaint/Brush.h" declline="28"/>
      </memberdef>
    </sectiondef>
    <location file="sigilpaint/Brush.h" line="12"/>
  </compounddef>
</doxygen>
"""

VALUE = """<?xml version='1.0' encoding='UTF-8' standalone='no'?>
<doxygen version="1.18.0">
  <compounddef id="structsigil_1_1paint_1_1_ink" kind="struct" prot="public">
    <compoundname>sigil::paint::Ink</compoundname>
    <briefdescription><para>What a mark is made of. </para></briefdescription>
    <sectiondef kind="public-func">
      <memberdef kind="function" id="structsigil_1_1paint_1_1_ink_1acc" prot="public"
                 static="no" const="no" explicit="no">
        <type/>
        <definition>sigil::paint::Ink::Ink</definition>
        <argsstring>(unsigned int packed)</argsstring>
        <name>Ink</name>
        <qualifiedname>sigil::paint::Ink::Ink</qualifiedname>
        <param><type>unsigned int</type><declname>packed</declname></param>
        <briefdescription/><detaileddescription/>
        <location file="sigilpaint/Ink.h" line="9"/>
      </memberdef>
    </sectiondef>
    <sectiondef kind="public-attrib">
      <memberdef kind="variable" id="structsigil_1_1paint_1_1_ink_1add" prot="public" static="no">
        <type>float</type><definition>float sigil::paint::Ink::alpha</definition>
        <argsstring/><name>alpha</name>
        <qualifiedname>sigil::paint::Ink::alpha</qualifiedname>
        <briefdescription/><detaileddescription/>
        <location file="sigilpaint/Ink.h" line="12"/>
      </memberdef>
    </sectiondef>
    <location file="sigilpaint/Ink.h" line="7"/>
  </compounddef>
</doxygen>
"""

WASH = """<?xml version='1.0' encoding='UTF-8' standalone='no'?>
<doxygen version="1.18.0">
  <compounddef id="structsigil_1_1paint_1_1_wash" kind="struct" prot="public">
    <compoundname>sigil::paint::Wash</compoundname>
    <briefdescription><para>Ink let down with water. </para></briefdescription>
    <sectiondef kind="public-func">
      <memberdef kind="function" id="structsigil_1_1paint_1_1_wash_1aee" prot="public"
                 static="no" const="no" explicit="no">
        <type/>
        <definition>sigil::paint::Wash::Wash</definition>
        <argsstring>(Ink source)</argsstring>
        <name>Wash</name>
        <qualifiedname>sigil::paint::Wash::Wash</qualifiedname>
        <param><type><ref refid="structsigil_1_1paint_1_1_ink" kindref="compound">Ink</ref></type>
               <declname>source</declname></param>
        <briefdescription/><detaileddescription/>
        <location file="sigilpaint/Wash.h" line="10"/>
      </memberdef>
    </sectiondef>
    <location file="sigilpaint/Wash.h" line="8"/>
  </compounddef>
</doxygen>
"""

STOCK = """<?xml version='1.0' encoding='UTF-8' standalone='no'?>
<doxygen version="1.18.0">
  <compounddef id="structsigil_1_1paint_1_1stock_1_1_wash" kind="struct" prot="public">
    <compoundname>sigil::paint::stock::Wash</compoundname>
    <briefdescription><para>A wash mixed to a recipe. </para></briefdescription>
    <location file="sigilpaint/stock/Wash.h" line="6"/>
  </compounddef>
</doxygen>
"""

MIXERS = """<?xml version='1.0' encoding='UTF-8' standalone='no'?>
<doxygen version="1.18.0">
  <compounddef id="namespacesigil_1_1paint_1_1mixers" kind="namespace" prot="public">
    <compoundname>sigil::paint::mixers</compoundname>
    <sectiondef kind="func">
      <memberdef kind="function" id="namespacesigil_1_1paint_1_1mixers_1a11" prot="public"
                 static="no" const="no" explicit="no">
        <type><ref refid="structsigil_1_1paint_1_1_ink" kindref="compound">Ink</ref></type>
        <definition>Ink sigil::paint::mixers::hexInk</definition>
        <argsstring>(unsigned int packed, float alpha)</argsstring>
        <name>hexInk</name>
        <qualifiedname>sigil::paint::mixers::hexInk</qualifiedname>
        <param><type>unsigned int</type><declname>packed</declname></param>
        <param><type>float</type><declname>alpha</declname></param>
        <briefdescription><para>An ink from a packed colour and an opacity. </para></briefdescription>
        <detaileddescription/>
        <location file="sigilpaint/Mixers.h" line="12"
                  declfile="sigilpaint/Mixers.h" declline="12"/>
      </memberdef>
    </sectiondef>
    <location file="sigilpaint/Mixers.h" line="4"/>
  </compounddef>
</doxygen>
"""

NAMESPACE = """<?xml version='1.0' encoding='UTF-8' standalone='no'?>
<doxygen version="1.18.0">
  <compounddef id="namespacesigil_1_1paint" kind="namespace" prot="public">
    <compoundname>sigil::paint</compoundname>
    <sectiondef kind="func">
      <memberdef kind="function" id="namespacesigil_1_1paint_1a01" prot="public"
                 static="no" const="no" explicit="no">
        <type><ref refid="classsigil_1_1paint_1_1_brush" kindref="compound">Brush</ref></type>
        <definition>Brush sigil::paint::brush</definition>
        <argsstring>()</argsstring>
        <name>brush</name>
        <qualifiedname>sigil::paint::brush</qualifiedname>
        <briefdescription><para>A new brush, with nothing set. </para></briefdescription>
        <detaileddescription/>
        <location file="sigilpaint/Brush.h" line="40" declfile="sigilpaint/Brush.h" declline="40"/>
      </memberdef>
      <memberdef kind="function" id="namespacesigil_1_1paint_1a02" prot="public"
                 static="no" const="no" explicit="no">
        <type><ref refid="structsigil_1_1paint_1_1_ink" kindref="compound">Ink</ref></type>
        <definition>Ink sigil::paint::hexInk</definition>
        <argsstring>(unsigned int packed)</argsstring>
        <name>hexInk</name>
        <qualifiedname>sigil::paint::hexInk</qualifiedname>
        <param><type>unsigned int</type><declname>packed</declname></param>
        <briefdescription><para>An ink from a packed hexadecimal colour. </para></briefdescription>
        <detaileddescription/>
        <location file="sigilpaint/Ink.h" line="20" declfile="sigilpaint/Ink.h" declline="20"/>
      </memberdef>
    </sectiondef>
    <sectiondef kind="enum">
      <memberdef kind="enum" id="namespacesigil_1_1paint_1a03" prot="public" static="no" strong="yes">
        <type/><name>Cap</name>
        <qualifiedname>sigil::paint::Cap</qualifiedname>
        <enumvalue id="e1" prot="public"><name>Butt</name>
          <briefdescription><para>Cut square at the end. </para></briefdescription>
          <detaileddescription/></enumvalue>
        <enumvalue id="e2" prot="public"><name>Round</name>
          <briefdescription/><detaileddescription/></enumvalue>
        <briefdescription><para>How a stroke ends. </para></briefdescription>
        <detaileddescription/>
        <location file="sigilpaint/Brush.h" line="6"/>
      </memberdef>
    </sectiondef>
    <location file="sigilpaint/Brush.h" line="4"/>
  </compounddef>
</doxygen>
"""

KIT = """<?xml version='1.0' encoding='UTF-8' standalone='no'?>
<doxygen version="1.18.0">
  <compounddef id="namespacesigil_1_1paint_1_1kit" kind="namespace" prot="public">
    <compoundname>sigil::paint::kit</compoundname>
    <sectiondef kind="func">
      <memberdef kind="function" id="namespacesigil_1_1paint_1_1kit_1a09" prot="public"
                 static="no" const="no" explicit="no">
        <type><ref refid="classsigil_1_1paint_1_1_brush" kindref="compound">Brush</ref></type>
        <definition>Brush sigil::paint::kit::hairline</definition>
        <argsstring>()</argsstring>
        <name>hairline</name>
        <qualifiedname>sigil::paint::kit::hairline</qualifiedname>
        <briefdescription><para>The thinnest brush the device can draw. </para></briefdescription>
        <detaileddescription/>
        <location file="sigilpaint/kit/Hairline.h" line="8"
                  declfile="sigilpaint/kit/Hairline.h" declline="8"/>
      </memberdef>
    </sectiondef>
    <location file="sigilpaint/kit/Hairline.h" line="4"/>
  </compounddef>
</doxygen>
"""

# A verb family the node inherits instead of declaring: a class template
# over the value that inherits it, whose one member hands that value
# back. `Tree.inherit_the_edges()` lays it down, with a Brush that names
# it as a base; the tree every other case reads does without it.
EDGES = """<?xml version='1.0' encoding='UTF-8' standalone='no'?>
<doxygen version="1.18.0">
  <compounddef id="classsigil_1_1paint_1_1_edge_verbs" kind="class" prot="public">
    <compoundname>sigil::paint::EdgeVerbs</compoundname>
    <templateparamlist>
      <param><type>class Derived</type></param>
    </templateparamlist>
    <sectiondef kind="public-func">
      <memberdef kind="function" id="classsigil_1_1paint_1_1_edge_verbs_1aff" prot="public"
                 static="no" const="no" explicit="no">
        <type>Derived &amp;</type>
        <definition>Derived &amp; sigil::paint::EdgeVerbs&lt; Derived &gt;::soften</definition>
        <argsstring>(float amount)</argsstring>
        <name>soften</name>
        <qualifiedname>sigil::paint::EdgeVerbs::soften</qualifiedname>
        <param><type>float</type><declname>amount</declname></param>
        <briefdescription><para>How far the edge of the mark bleeds. </para></briefdescription>
        <detaileddescription/>
        <location file="sigilpaint/verbs/Edge.h" line="14"/>
      </memberdef>
    </sectiondef>
    <sectiondef kind="private-func">
      <memberdef kind="function" id="classsigil_1_1paint_1_1_edge_verbs_1a00" prot="private"
                 static="no" const="no" explicit="no">
        <type>Derived &amp;</type>
        <definition>Derived &amp; sigil::paint::EdgeVerbs&lt; Derived &gt;::self</definition>
        <argsstring>()</argsstring>
        <name>self</name>
        <qualifiedname>sigil::paint::EdgeVerbs::self</qualifiedname>
        <briefdescription/><detaileddescription/>
        <location file="sigilpaint/verbs/Edge.h" line="18"/>
      </memberdef>
    </sectiondef>
    <location file="sigilpaint/verbs/Edge.h" line="10"/>
  </compounddef>
</doxygen>
"""

EDGES_BASE = """    <compoundname>sigil::paint::Brush</compoundname>
    <basecompoundref refid="classsigil_1_1paint_1_1_edge_verbs" prot="public"
                     virt="non-virtual">sigil::paint::EdgeVerbs&lt; Brush &gt;</basecompoundref>
"""

INDEX = """<?xml version='1.0' encoding='UTF-8' standalone='no'?>
<doxygenindex version="1.18.0"/>
"""

DECLARATIONS = """
from __future__ import annotations
import typing
import sigil._types as _t
__all__: list[str] = ['Brush', 'Cap', 'Ink', 'Wash', 'brush', 'hex_ink']

class Brush:

    def tint(self, colour: _t.InkLike) -> Brush:
        ...

    def width(self, value: typing.SupportsFloat) -> Brush:
        ...

    def thicken(self, amount: typing.SupportsFloat) -> Brush:
        ...

    def thin(self, amount: typing.SupportsFloat) -> Brush:
        ...

class Ink:
    pass

class Wash:
    pass

class Cap:
    Butt: typing.ClassVar[Cap]
    Round: typing.ClassVar[Cap]
    __members__: typing.ClassVar[dict[str, Cap]]

    def __eq__(self, other: object) -> bool:
        ...

    @property
    def name(self) -> str:
        ...

    @property
    def value(self) -> int:
        ...

def brush() -> Brush:
    ...

def hex_ink(packed: typing.SupportsInt) -> Ink:
    ...
"""

# The package's own table, as the reference layer reads it off disk.
SURFACE = """
PUBLIC_MODULES = {"_sigil.paint": "sigil.paint"}
RENAMES = {"sigil.paint": {"hexInk": "hex_ink"}}


def public_module(raw):
    return PUBLIC_MODULES[raw]


def public_name(public, name):
    return RENAMES.get(public, {}).get(name, name)
"""

ROLES = """
from typing import TypeAlias
import sigil.paint

ColorLike: TypeAlias = str | tuple[float, float, float]
InkLike: TypeAlias = ColorLike | sigil.paint.Ink
"""

BINDING = """
#include <sigilpaint/Brush.h>
#include <sigilpaint/Ink.h>

namespace sigil::python {
namespace py = pybind11;
using paint::Brush;

void bindPaint(py::module_& root) {
  auto module = root.def_submodule("paint");
  py::class_<Brush> brush(module, "Brush");
  py::class_<paint::Ink>(module, "Ink");
  py::enum_<paint::Cap>(module, "Cap")
      .value("Butt", paint::Cap::Butt)
      .value("Round", paint::Cap::Round);
  brush.def("tint", [](Brush& self, py::object value) -> Brush& {
        return self.tint(ink(value));
      })
      .def("width", &Brush::width, py::arg("value"))
      .def("thicken", [](Brush& self, float amount) -> Brush& {
        return self.width(amount * 2.0f);
      });
  module.def("brush", &paint::brush);
  module.def("hexInk", [](unsigned packed) { return paint::hexInk(packed); });
}
}  // namespace sigil::python
"""

# One hand-written page, one page naming nothing, and the example the
# first of them asks for.
PAGE = """---
kind: verb
library: SigilPaint
name: tint
group: Colour
example: tint_verb
python: sigil.paint.Brush.tint
---

# tint

The colour every mark after it is laid down in.

## Description

A tint is resolved where the mark lands, not where it is set.

## Examples

```python
paint.brush().tint(sigil.paint.nothing)
```

<!-- example: tint_verb -->
"""

ORPHAN = """---
kind: verb
library: SigilPaint
name: nothingAtAll
---

Nothing declares this.
"""

EXAMPLE = """from sigil import paint

paint.brush().tint("#1f2933")
"""


class Library:
    """One row of the manifest, as the docs verb reads it."""

    def __init__(self, name: str, brief: str, strip: list):
        self.name = name
        self.brief = brief
        self.strip = strip


class Manifest:
    """What the docs verb hands the reference layer, over the small tree."""

    def __init__(self, root: Path, source: Path):
        self.root = root / "docs"
        self.work = root / "work"
        self.templates = root / "templates"
        self.libraries = [
            Library(
                "SigilPaint",
                "Marks, and what they are made of.",
                [str(source / "include"), str(source), str(root)],
            )
        ]

    def find(self, name: str) -> Library:
        return next(one for one in self.libraries if one.name == name)


class Tree(unittest.TestCase):
    """A fixture that lays the small tree down on disk."""

    def setUp(self) -> None:
        self.held = tempfile.TemporaryDirectory()
        self.root = Path(self.held.name)
        self.addCleanup(self.held.cleanup)
        xml = self.root / "work" / "SigilPaint" / "xml"
        xml.mkdir(parents=True)
        for name, body in (
            ("index.xml", INDEX),
            ("classsigil_1_1paint_1_1_brush.xml", COMPOUND),
            ("structsigil_1_1paint_1_1_ink.xml", VALUE),
            ("structsigil_1_1paint_1_1_wash.xml", WASH),
            ("structsigil_1_1paint_1_1stock_1_1_wash.xml", STOCK),
            ("namespacesigil_1_1paint.xml", NAMESPACE),
            ("namespacesigil_1_1paint_1_1kit.xml", KIT),
            ("namespacesigil_1_1paint_1_1mixers.xml", MIXERS),
        ):
            (xml / name).write_text(body)

        self.source = self.root / "src" / "paint"
        prose = self.source / "reference" / "pages" / "verbs"
        prose.mkdir(parents=True)
        (prose / "tint.md").write_text(PAGE)
        (prose / "nothingAtAll.md").write_text(ORPHAN)
        examples = self.source / "reference" / "examples"
        examples.mkdir(parents=True)
        (examples / "tint_verb.py").write_text(EXAMPLE)
        (self.root / "templates").mkdir()

        package = self.root / "package"
        (package / "typing").mkdir(parents=True)
        (package / "typing" / "surface.py").write_text(textwrap.dedent(SURFACE))
        self.package = package
        declarations = self.root / "declarations" / "sigil"
        declarations.mkdir(parents=True)
        (declarations / "paint.pyi").write_text(textwrap.dedent(DECLARATIONS))
        (declarations / "__init__.pyi").write_text(
            "from . import paint as paint\n__all__: list[str] = ['paint']\n"
        )
        (declarations / "_types.pyi").write_text(textwrap.dedent(ROLES))
        self.declarations = declarations

        # One directory per library under the binding root, which is how
        # the package lays its sources out and what the scan has to walk.
        sources = self.root / "bindings"
        (sources / "paint").mkdir(parents=True)
        (sources / "paint" / "PaintBindings.cpp").write_text(
            textwrap.dedent(BINDING)
        )
        self.sources = [sources]

    def inherit_the_edges(self) -> None:
        """The Brush takes one of its verbs from a mixin over itself."""
        xml = self.root / "work" / "SigilPaint" / "xml"
        (xml / "classsigil_1_1paint_1_1_edge_verbs.xml").write_text(EDGES)
        (xml / "classsigil_1_1paint_1_1_brush.xml").write_text(
            COMPOUND.replace(
                "    <compoundname>sigil::paint::Brush</compoundname>\n", EDGES_BASE
            )
        )

    def catalogue(self, node: bool = False):
        from sigil.reference import bindings, catalogue, doxygen_xml, python_stubs

        with lowered_floor(node):
            inventories = doxygen_xml.read(self.root / "work", ["SigilPaint"])
        surface, roles = python_stubs.read(self.package, self.declarations)
        return catalogue.Catalogue(
            inventories, surface, roles, bindings.read(self.sources)
        )

    def built(self, **asked):
        """One whole run over the small tree, read but not yet written."""
        from sigil.reference.build import Build, Options

        manifest = Manifest(self.root, self.source)
        options = Options(
            package=self.package,
            declarations=self.declarations,
            binding_sources=self.sources,
            **asked,
        )
        run = Build(manifest, options)
        with lowered_floor(True):
            self.assertTrue(run.read())
        return run


@contextlib.contextmanager
def lowered_floor(lower: bool):
    """The fixture's Brush chains twice, and a node chains ten times.

    Two members is the shape the readers have to handle; ten of them is
    only more of the same text, so the floor comes down instead.
    """
    from sigil.reference import doxygen_xml

    if not lower:
        yield
        return
    standing = doxygen_xml.NODE_FLOOR
    doxygen_xml.NODE_FLOOR = 2
    try:
        yield
    finally:
        doxygen_xml.NODE_FLOOR = standing
