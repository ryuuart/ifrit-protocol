"""A small tree of every shape the readers have to handle.

One library, one node type with one verb, one factory, one value with
an implicit conversion, one enumeration, and a Python surface that
re-exports part of it under a renamed spelling. Small enough to read,
and it exercises every branch the real tree exercises.
"""

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

INDEX = """<?xml version='1.0' encoding='UTF-8' standalone='no'?>
<doxygenindex version="1.18.0"/>
"""

STUB = """
from __future__ import annotations
import typing
__all__: list[str] = ['Brush', 'Ink', 'Cap', 'brush', 'hexInk']

class Brush:

    def tint(self, colour: _t.InkLike) -> Brush:
        ...

    def width(self, value: typing.SupportsFloat) -> Brush:
        ...

class Ink:
    pass

class Cap:
    pass

def brush() -> Brush:
    ...

def hexInk(packed: typing.SupportsInt) -> Ink:
    ...
"""

PUBLIC = """
from _sigil.paint import (
    Brush,
    Ink,
    brush,
)
from _sigil.paint import (
    hexInk as hex_ink,
)
"""

ROLES = """
from typing import TypeAlias
from _sigil import paint

ColorLike: TypeAlias = str | tuple[float, float, float]
InkLike: TypeAlias = ColorLike | paint.Ink
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
      .def("width", &Brush::width, py::arg("value"));
  module.def("brush", &paint::brush);
  module.def("hexInk", [](unsigned packed) { return paint::hexInk(packed); });
}
}  // namespace sigil::python
"""


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
            ("namespacesigil_1_1paint.xml", NAMESPACE),
            ("namespacesigil_1_1paint_1_1kit.xml", KIT),
        ):
            (xml / name).write_text(body)

        package = self.root / "package"
        (package / "stubs" / "_sigil" / "paint").mkdir(parents=True)
        (package / "stubs" / "_sigil" / "paint" / "__init__.pyi").write_text(
            textwrap.dedent(STUB)
        )
        (package / "stubs" / "_sigil" / "__init__.pyi").write_text(
            "from . import paint\n__all__: list[str] = ['paint']\n"
        )
        (package / "sigil").mkdir(parents=True)
        (package / "sigil" / "paint.pyi").write_text(textwrap.dedent(PUBLIC))
        (package / "typing").mkdir(parents=True)
        (package / "typing" / "_types.pyi").write_text(textwrap.dedent(ROLES))
        self.package = package

        sources = self.root / "bindings"
        sources.mkdir()
        (sources / "PaintBindings.cpp").write_text(textwrap.dedent(BINDING))
        self.sources = [sources]

    def catalogue(self):
        from sigil.reference import bindings, catalogue, doxygen_xml, python_stubs

        inventories = doxygen_xml.read(self.root / "work", ["SigilPaint"])
        surface, roles = python_stubs.read(self.package)
        return catalogue.Catalogue(
            inventories, surface, roles, bindings.read(self.sources)
        )
