/** @file
 * What Python keeps past a session's teardown: a context kept outside it
 * fails safely, and neither a stored element nor a global graphics
 * callback keeps the sketch or its generation alive.
 */

#include <gtest/gtest.h>

#include "PythonTestSupport.h"

namespace {

namespace sketch = sigil::sketch;
namespace fs = std::filesystem;
using sketch::test::assets;
using sketch::test::fonts;
using sketch::test::PythonSource;

TEST(SketchPython, AContextKeptOutsideItsSessionFailsSafelyAfterTeardown) {
  PythonSource source("sigil_python_context_lifetime");
  source.write("entry.py",
               "import builtins\n"
               "class Study:\n"
               "    def setup(self, ctx):\n"
               "        builtins._sigil_expired_context = ctx\n");
  {
    const sketch::Kind kind = sketch::python::load(source.entry());
    auto session = kind->open(fonts(), assets(), true, "saved");
  }
  source.write("entry.py",
               "import builtins\n"
               "class Study:\n"
               "    def setup(self, ctx):\n"
               "        ctx.canvas(1, 1)\n"
               "        try:\n"
               "            builtins._sigil_expired_context.canvas(300, 200)\n"
               "        except RuntimeError:\n"
               "            ctx.canvas(80, 60)\n"
               "        finally:\n"
               "            del builtins._sigil_expired_context\n");
  const sketch::Kind kind = sketch::python::load(source.entry());
  auto session = kind->open(fonts(), assets(), true, "checked");
  EXPECT_EQ(session->canvas().size, SkSize::Make(80, 60));
}

TEST(SketchPython, AStoredElementDoesNotKeepItsSketchAliveAfterTeardown) {
  PythonSource source("sigil_python_callback_cycle");
  source.write("entry.py",
               "import builtins\n"
               "import weakref\n"
               "from sigil.compose import graphics\n"
               "class Study:\n"
               "    def setup(self, ctx):\n"
               "        ctx.canvas(80, 60)\n"
               "        self.node = graphics('loop', self.draw).size(80, 60)\n"
               "        ctx.render(self.node)\n"
               "        builtins._sigil_callback_owner = weakref.ref(self)\n"
               "    def draw(self, pen):\n"
               "        pen.circle(40, 30, 20)\n");
  {
    const sketch::Kind kind = sketch::python::load(source.entry());
    auto session = kind->open(fonts(), assets(), true, "saved");
  }
  source.write("entry.py",
               "import builtins\n"
               "class Study:\n"
               "    def setup(self, ctx):\n"
               "        previous = builtins._sigil_callback_owner\n"
               "        del builtins._sigil_callback_owner\n"
               "        assert previous() is None, 'A stored graphics callback "
               "retained its sketch'\n"
               "        ctx.canvas(80, 60)\n");
  const sketch::Kind kind = sketch::python::load(source.entry());
  auto session = kind->open(fonts(), assets(), true, "checked");
  EXPECT_EQ(session->canvas().size, SkSize::Make(80, 60));
}

TEST(SketchPython, AGlobalGraphicsCallbackDoesNotKeepItsGenerationAlive) {
  PythonSource source("sigil_python_global_callback_lifetime");
  source.write("entry.py",
               "import builtins\n"
               "import weakref\n"
               "from sigil.compose import graphics\n"
               "class Token:\n"
               "    pass\n"
               "owner = Token()\n"
               "builtins._sigil_generation_owner = weakref.ref(owner)\n"
               "def draw(pen):\n"
               "    pass\n"
               "GLOBAL = graphics('global', draw)\n"
               "class Study:\n"
               "    def setup(self, ctx):\n"
               "        ctx.render(GLOBAL)\n");
  {
    const sketch::Kind kind = sketch::python::load(source.entry());
    auto session = kind->open(fonts(), assets(), true, "global");
  }
  source.write("entry.py",
               "import builtins\n"
               "import gc\n"
               "gc.collect()\n"
               "try:\n"
               "    assert builtins._sigil_generation_owner() is None\n"
               "finally:\n"
               "    del builtins._sigil_generation_owner\n"
               "class Study:\n"
               "    def setup(self, ctx):\n"
               "        ctx.canvas(80, 60)\n");
  const sketch::Kind kind = sketch::python::load(source.entry());
  auto session = kind->open(fonts(), assets(), true, "checked");
  EXPECT_EQ(session->canvas().size, SkSize::Make(80, 60));
}

}  // namespace
