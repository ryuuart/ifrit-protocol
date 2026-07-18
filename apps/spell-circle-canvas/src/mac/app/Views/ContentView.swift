import SwiftUI

/// Standard macOS split view (HIG: Split views + Toolbars) layered OVER a
/// window-level canvas backdrop: the Metal canvas renders at the root of
/// the tree, the navigation chrome (sidebar glass pane, toolbar, trailing
/// inspector) floats above it, and a transparent proxy in the detail
/// column forwards gestures down to the canvas. Canvas position and UI
/// layout are independent concerns — nothing the chrome animates can move
/// or resize the canvas; only the window can.
struct ContentView: View {
    /// Toolbar chrome style. The engine's Skia pass is a TRUE progressive
    /// blur of the canvas (sigma ramp, no mask); the Liquid Glass strip is
    /// the system material, which shows a sheen but cannot blur the Metal
    /// layer beneath and only fades via a gradient mask. Exactly one of
    /// the two renders (see CanvasBackdropView, which flips the engine
    /// flag off this constant).
    static let usesGlassToolbarStrip = true

    @Environment(EngineModel.self) private var model

    var body: some View {
        @Bindable var model = model
        ZStack {
            CanvasBackdropView()
                .ignoresSafeArea()

            // Layered ABOVE the canvas backdrop but BENEATH the navigation
            // chrome, so the sidebar's glass pane draws over the strip
            // instead of the strip washing over the Activity Panel.
            if Self.usesGlassToolbarStrip {
                GlassToolbarStrip()
            }

            NavigationSplitView {
                ActivityPanel()
                    .navigationSplitViewColumnWidth(min: 250, ideal: 280, max: 380)
                    .navigationTitle("Status")
            } detail: {
                // Landmarks ordering: the toolbar belongs to the detail
                // content, with the inspector applied outside it. The
                // inspector toggles through the system mechanism
                // (InspectorCommands, ⌃⌘I) plus this convenience button.
                // The glass toolbar strip is a ZStack layer beneath this
                // chrome; the proxy just extends under the bar.
                CanvasDetailProxy()
                    .ignoresSafeArea(.container, edges: .top)
                    .navigationTitle("SpellCircle")
                    .toolbar {
                        ToolbarItemGroup {
                            Button("Fit", systemImage: "rectangle.arrowtriangle.2.inward") {
                                model.canvasView?.resetView()
                            }
                            .help("Fit canvas to window")

                            Button("Actual Size", systemImage: "1.magnifyingglass") {
                                model.canvasView?.zoomToActualSize()
                            }
                            .help("Zoom to 100%")
                        }

                        ToolbarSpacer(.fixed)

                        ToolbarItem {
                            Button("Canvas Inspector", systemImage: "sidebar.trailing") {
                                model.showSettings.toggle()
                            }
                            .help("Canvas inspector (⌃⌘I)")
                        }
                    }
            }
            .inspector(isPresented: $model.showSettings) {
                SettingsPane()
                    .inspectorColumnWidth(min: 300, ideal: 330, max: 440)
            }
            // No system bar (background hidden): the toolbar chrome is the
            // glass strip below. The engine's Skia progressive blur stays
            // available as the self-rendered alternative
            // (SCKCanvasView.drawsTopEdgeBlur) — never enable both.
            .toolbarBackgroundVisibility(.hidden, for: .windowToolbar)
        }
    }
}
