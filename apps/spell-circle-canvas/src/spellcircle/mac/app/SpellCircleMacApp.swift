import AppKit
import SwiftUI

/// Quits when the last (only) window closes: a viewer with no window is
/// just a headless process squatting on the UDP port and Syphon name.
final class AppDelegate: NSObject, NSApplicationDelegate {
    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool {
        true
    }
}

/// Native macOS SpellCircle receiver: the same UDP wire protocol, shared
/// C++ scene core, Skia Graphite rendering, and Syphon publishing as the
/// Qt app — presented with SwiftUI Liquid Glass chrome. The Qt app remains
/// the cross-platform build; this one exists to feel at home on macOS.
@main
struct SpellCircleMacApp: App {
    @NSApplicationDelegateAdaptor(AppDelegate.self) private var appDelegate
    @State private var model = EngineModel()

    var body: some Scene {
        // A single split-view window: sidebar, canvas detail, and a
        // trailing canvas inspector for scene attributes. WindowGroup, not
        // the single-instance `Window` scene — that one never presents its
        // window at launch here (macOS 26, verified windowless even with
        // .defaultLaunchBehavior(.presented) and fresh saved state).
        // Single-window semantics are enforced instead: ⌘N is removed
        // below (the engine, canvas view, and Syphon server are process
        // singletons a second window would fight over), and the delegate
        // quits the app when the window closes.
        WindowGroup("SpellCircle") {
            ContentView()
                .environment(model)
                .frame(minWidth: 860, minHeight: 560)
        }
        .commands {
            // No File ▸ New Window: this is a single-window app.
            CommandGroup(replacing: .newItem) {}
            // The system's inspector plumbing: View ▸ Hide/Show Inspector
            // with the standard ⌃⌘I, wired to the presented inspector.
            InspectorCommands()
        }

        // App-level behavior lives in the standard Settings window (⌘,),
        // per the HIG's inspector/settings split.
        Settings {
            AppSettingsView()
                .environment(model)
        }
    }
}
