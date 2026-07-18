import SpellCircleMacBridge
import SwiftUI

/// The window-level Metal canvas backdrop. Lives at the ROOT of the view
/// tree — never inside the navigation chrome — so canvas position and UI
/// layout stay independent concerns: pane reveal animations and column
/// resizes can't move or resize the CAMetalLayer, only window resizes can.
/// (Hosting it inside the detail column rides the system's pane animation
/// — the drift-then-snap.)
struct CanvasBackdropView: NSViewRepresentable {
    @Environment(EngineModel.self) private var model

    func makeNSView(context: Context) -> SCKCanvasView {
        let view = SCKCanvasView(frame: .zero)
        view.engine = model.engine
        // The engine's Skia progressive blur and the SwiftUI glass strip
        // are alternatives — exactly one toolbar treatment renders.
        view.drawsTopEdgeBlur = !ContentView.usesGlassToolbarStrip
        model.canvasView = view
        return view
    }

    func updateNSView(_ nsView: SCKCanvasView, context: Context) {}
}

/// The canvas's transparent stand-in inside the detail column: forwards
/// pointer gestures to the backdrop canvas and reports the region the
/// chrome leaves uncovered (fit-centering insets).
struct CanvasDetailProxy: NSViewRepresentable {
    @Environment(EngineModel.self) private var model

    func makeNSView(context: Context) -> SCKCanvasDetailProxyView {
        let view = SCKCanvasDetailProxyView(frame: .zero)
        // Resolved lazily: the backdrop may not have been created yet when
        // the chrome builds this proxy.
        let model = self.model
        view.canvasProvider = { model.canvasView }
        return view
    }

    func updateNSView(_ nsView: SCKCanvasDetailProxyView, context: Context) {}
}
