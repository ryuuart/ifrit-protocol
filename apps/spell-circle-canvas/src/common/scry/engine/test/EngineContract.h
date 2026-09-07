#pragma once

/** @file
 * What both engines promise whichever way they publish. Ultralight
 * allows one Renderer per process, so the CPU-mode and the GPU-mode
 * engines are two binaries; a claim that is about the engine and not
 * about how a frame is carried is written once here and asked of each.
 */

#include <gtest/gtest.h>
#include <sigilscry/engine/WebEngine.h>
#include <sigilscry/engine/WebView.h>

#include "Wait.h"

namespace sigil::scry::test {

/** Pages made and dropped while the render loop runs. Every round
 *  publishes, so a view torn down under the loop leaves the engine able
 *  to stand up the next one. */
inline void expectPagesComeAndGo(WebEngine& engine) {
  for (int round = 0; round < 12; ++round) {
    std::shared_ptr<WebView> view =
        engine.createView(64, 64, {.transparent = false});
    ASSERT_NE(view, nullptr) << "round " << round;
    view->loadHTML(
        "<html><body style='background:#0000ff;margin:0'></body></html>");
    EXPECT_TRUE(waitForFrame(*view, 0)) << "round " << round;
  }
}

}  // namespace sigil::scry::test
