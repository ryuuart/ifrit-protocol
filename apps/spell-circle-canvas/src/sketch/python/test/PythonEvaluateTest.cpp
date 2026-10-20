/** @file
 * Text a client evaluates in a running Python sketch, and the pointer and
 * keys a session feeds its input hooks, through a host in this process.
 */

#include <gtest/gtest.h>
#include <sigilsketch/testing/InProcessHost.h>

#include <memory>
#include <string_view>
#include <utility>

#include "PythonTestSupport.h"

namespace {

namespace sketch = sigil::sketch;
namespace fs = std::filesystem;
using sketch::test::fonts;
using sketch::test::PythonSource;

/** A HOST IN THIS PROCESS ON @p entry, under the clock a client steps,
 *  with the evaluate door as @p evaluates says. */
std::unique_ptr<sketch::testing::InProcessHost> evaluatingHost(
    const PythonSource& source, bool evaluates) {
  sketch::testing::InProcessHostOptions options;
  options.stateDirectory = source.scratch.path / "state";
  options.session.fonts = &fonts();
  options.session.pythonLoader = &sketch::python::load;
  options.session.evaluates = evaluates;
  auto host =
      std::make_unique<sketch::testing::InProcessHost>(std::move(options));
  EXPECT_TRUE(host->clock(sigil::protocol::clock::Policy_Advance));
  const auto opened = host->open(source.entry().string());
  EXPECT_TRUE(opened) << (opened ? "" : opened.error().message);
  return host;
}

constexpr std::string_view kEvaluated =
    "from sigil.compose import box\n"
    "from sigil.sketch import sketch\n"
    "COLOUR = '#ff0000'\n"
    "@sketch(size=(64, 48), background='#000000', captureAt=0)\n"
    "class Study:\n"
    "    described = 0\n"
    "    def setup(self):\n"
    "        self.described += 1\n"
    "        return box().width(64).height(48).fill(COLOUR)\n";

TEST(SketchPython, EvaluatedTextRunsInTheSketchsModuleAndDescribesItAgain) {
  PythonSource source("sigil_python_evaluate");
  source.write("entry.py", kEvaluated);
  const auto host = evaluatingHost(source, true);
  ASSERT_TRUE(host->step(1.0 / 60.0));
  using sigil::protocol::session::EvaluationMode_Statements;

  // An expression answers its value, read from the running instance as
  // `self`; every evaluation that runs to its end has a sketch that declares
  // its tree in setup asked for it again, after the text ran.
  const auto described = host->evaluate("self.described");
  ASSERT_TRUE(described) << described.error().message;
  EXPECT_EQ(described.result().value, "1");
  EXPECT_FALSE(described.result().error);
  EXPECT_EQ(host->evaluate("self.described").result().value, "2");
  const auto colour = host->evaluate("COLOUR");
  ASSERT_TRUE(colour);
  EXPECT_EQ(colour.result().value, "'#ff0000'");
  const auto context = host->evaluate("ctx is not None and sigil is not None");
  ASSERT_TRUE(context);
  EXPECT_EQ(context.result().value, "True");

  // Statements change the module's own names and answer what they printed.
  const auto changed =
      host->evaluate("COLOUR = '#00ff00'\nprint('changed', COLOUR)",
                     EvaluationMode_Statements);
  ASSERT_TRUE(changed) << changed.error().message;
  EXPECT_EQ(changed.result().value, "");
  EXPECT_EQ(changed.result().output, "changed #00ff00\n");
  EXPECT_FALSE(changed.result().error);
  EXPECT_EQ(host->evaluate("COLOUR").result().value, "'#00ff00'");
  EXPECT_TRUE(host->step(1.0 / 60.0));
}

TEST(SketchPython, EvaluatedTextThatRaisesIsAnsweredAndTheSketchRunsOn) {
  PythonSource source("sigil_python_evaluate_raises");
  source.write("entry.py", kEvaluated);
  const auto host = evaluatingHost(source, true);
  ASSERT_TRUE(host->step(1.0 / 60.0));
  using sigil::protocol::session::EvaluationMode_Statements;
  EXPECT_EQ(host->evaluate("self.described").result().value, "1");

  const auto raised = host->evaluate(
      "print('before')\nCOLOUR = '#0000ff'\n1 / 0", EvaluationMode_Statements);
  ASSERT_TRUE(raised) << raised.error().message;
  ASSERT_TRUE(raised.result().error);
  EXPECT_EQ(raised.result().error->type, "ZeroDivisionError");
  EXPECT_EQ(raised.result().error->message, "division by zero");
  EXPECT_NE(raised.result().error->traceback.find("<prompt"), std::string::npos)
      << raised.result().error->traceback;
  EXPECT_EQ(raised.result().output, "before\n");
  // What ran before the raise stands, nothing is described again, and the
  // session draws on.
  EXPECT_EQ(host->evaluate("self.described").result().value, "2");
  EXPECT_EQ(host->evaluate("COLOUR").result().value, "'#0000ff'");
  const auto syntax = host->evaluate("def", EvaluationMode_Statements);
  ASSERT_TRUE(syntax);
  ASSERT_TRUE(syntax.result().error);
  EXPECT_EQ(syntax.result().error->type, "SyntaxError");
  const auto leaving =
      host->evaluate("raise SystemExit(3)", EvaluationMode_Statements);
  ASSERT_TRUE(leaving);
  ASSERT_TRUE(leaving.result().error);
  EXPECT_EQ(leaving.result().error->type, "SystemExit");
  EXPECT_TRUE(host->step(1.0 / 60.0)) << host->readout();
  EXPECT_TRUE(host->still(1.0)) << host->readout();
}

constexpr std::string_view kInputHooks =
    "from sigil.sketch.globals import *\n"
    "size(64, 48)\n"
    "pressed = []\n"
    "released = []\n"
    "keys = []\n"
    "seen = []\n"
    "def setup():\n"
    "    pass\n"
    "def update(elapsed, ctx):\n"
    "    at = ctx.pointer.at\n"
    "    seen.append(((at.x, at.y), ctx.pointer.pressed))\n"
    "def mousePressed():\n"
    "    pressed.append((mouseX, mouseY))\n"
    "def mouseReleased():\n"
    "    released.append((mouseX, mouseY))\n"
    "def keyPressed():\n"
    "    keys.append((key, keyCode))\n";

TEST(SketchPython,
     ASessionsPointerAndKeysReachTheInputHooksOfAFileWithoutDraw) {
  PythonSource source("sigil_python_input_hooks");
  source.write("entry.py", kInputHooks);
  const auto host = evaluatingHost(source, true);
  ASSERT_TRUE(host->step(1.0 / 60.0)) << host->readout();
  EXPECT_EQ(host->evaluate("pressed").result().value, "[]");

  // The session's pointer, put before a step, is what update reads off the
  // context and what the press the step finds calls mousePressed with.
  const auto pointed = host->pointer(12, 30, true);
  ASSERT_TRUE(pointed) << pointed.error().message;
  ASSERT_TRUE(host->step(1.0 / 60.0)) << host->readout();
  EXPECT_EQ(host->evaluate("pressed").result().value, "[(12.0, 30.0)]");
  EXPECT_EQ(host->evaluate("seen[-1]").result().value, "((12.0, 30.0), True)");
  EXPECT_EQ(host->evaluate("mouseIsPressed").result().value, "True");

  ASSERT_TRUE(host->pointer(20, 10, false));
  const auto keyed = host->key("a", 65, true);
  ASSERT_TRUE(keyed) << keyed.error().message;
  ASSERT_TRUE(host->step(1.0 / 60.0)) << host->readout();
  EXPECT_EQ(host->evaluate("released").result().value, "[(20.0, 10.0)]");
  EXPECT_EQ(host->evaluate("keys").result().value, "[('a', 65)]");
  EXPECT_EQ(host->evaluate("pressed").result().value, "[(12.0, 30.0)]");
}

TEST(SketchPython, AHostThatWasNotToldToEvaluatesNothing) {
  PythonSource source("sigil_python_evaluate_shut");
  source.write("entry.py", kEvaluated);
  const auto host = evaluatingHost(source, false);
  const auto refused = host->evaluate("COLOUR");
  ASSERT_FALSE(refused);
  EXPECT_EQ(refused.error().code, sigil::protocol::ErrorCode_failed);
  EXPECT_EQ(refused.error().message.rfind("session.evaluate:", 0), 0u)
      << refused.error().message;
  EXPECT_TRUE(host->step(1.0 / 60.0));
}

}  // namespace
