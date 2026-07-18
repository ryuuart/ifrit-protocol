import AppKit
import SpellCircleMacBridge
import SwiftUI

/// Observable facade over SCKEngine: publishes feed/status state to SwiftUI,
/// pushes settings into the engine, and persists them in UserDefaults.
/// The engine delivers every callback on the main queue, and the whole
/// model is @MainActor-isolated to match (the bridge API is annotated
/// NS_SWIFT_UI_ACTOR, so the conformance checks out under strict
/// concurrency).
///
/// @Observable (not ObservableObject) deliberately: invalidation is per
/// property *read*, so a feed insert re-evaluates only the feed table and a
/// zoom change only the zoom readout — never the whole window tree (split
/// view, toolbar, inspector) on the thread the canvas renders on.
@MainActor
@Observable
final class EngineModel: NSObject, SCKEngineDelegate {
    @ObservationIgnored let engine = SCKEngine()
    @ObservationIgnored weak var canvasView: SCKCanvasView? {
        didSet {
            canvasView?.onViewChanged = { [weak self] percent in
                if self?.zoomPercent != percent {
                    self?.zoomPercent = percent
                }
            }
        }
    }

    private(set) var feed: [FeedEntry] = []
    private(set) var listening = false
    private(set) var statusText = ""
    private(set) var zoomPercent: Double = 0
    private(set) var renderMillis: Double = 0
    private(set) var scenesPerSecond: Double = 0
    /// Whether the trailing settings inspector is open. The sidebar and
    /// inspector are real split-view columns (not overlays), so the canvas
    /// needs no content insets — the detail area is all its own.
    var showSettings = false

    // MARK: Settings (persisted; didSet pushes into the engine)

    /// Valid range for the offscreen canvas texture — the engine clamps too,
    /// but clamping here keeps the UI honest about what was applied.
    static let canvasSizeRange = 16...8192

    var port: Int { didSet { engine.port = Int32(port); save("port", port) } }
    var canvasWidth: Int {
        didSet {
            let bounded = canvasWidth.clamped(to: Self.canvasSizeRange)
            if bounded != canvasWidth { canvasWidth = bounded; return }
            engine.canvasWidth = Int32(bounded)
            save("canvasWidth", bounded)
        }
    }
    var canvasHeight: Int {
        didSet {
            let bounded = canvasHeight.clamped(to: Self.canvasSizeRange)
            if bounded != canvasHeight { canvasHeight = bounded; return }
            engine.canvasHeight = Int32(bounded)
            save("canvasHeight", bounded)
        }
    }
    /// Offscreen render rate cap (60, 120, 144, …); the engine keeps
    /// producing frames at this rate even with the display asleep.
    var targetFramesPerSecond: Double { didSet { engine.targetFramesPerSecond = targetFramesPerSecond; save("targetFramesPerSecond", targetFramesPerSecond) } }
    var scale: Double { didSet { engine.scale = scale; save("scale", scale) } }
    var strokeWidth: Double { didSet { engine.strokeWidth = strokeWidth; save("strokeWidth", strokeWidth) } }
    var labelOffset: Double { didSet { engine.labelOffset = labelOffset; save("labelOffset", labelOffset) } }
    var pointDistance: Double { didSet { engine.pointDistance = pointDistance; save("pointDistance", pointDistance) } }
    var boxWidth: Double { didSet { engine.boxWidth = boxWidth; save("boxWidth", boxWidth) } }
    var boxHeight: Double { didSet { engine.boxHeight = boxHeight; save("boxHeight", boxHeight) } }
    var boxPadding: Double { didSet { engine.boxPadding = boxPadding; save("boxPadding", boxPadding) } }
    var boxDistance: Double { didSet { engine.boxDistance = boxDistance; save("boxDistance", boxDistance) } }
    var fontSize: Double { didSet { engine.fontSize = fontSize; save("fontSize", fontSize) } }
    var fontFamily: String { didSet { engine.fontFamily = fontFamily; save("fontFamily", fontFamily) } }
    var fontWeight: Int { didSet { engine.fontWeight = Int32(fontWeight); save("fontWeight", fontWeight) } }
    var fontItalic: Bool { didSet { engine.fontItalic = fontItalic; save("fontItalic", fontItalic) } }
    var appearance: AppearancePreference {
        didSet {
            applyAppearance()
            save("appearance", appearance.rawValue)
        }
    }
    var accentColor: Color {
        didSet {
            let nsColor = NSColor(accentColor)
            engine.accentColor = nsColor
            if let srgb = nsColor.usingColorSpace(.sRGB) {
                UserDefaults.standard.set(
                    [srgb.redComponent, srgb.greenComponent, srgb.blueComponent, srgb.alphaComponent],
                    forKey: "accentColor")
            }
        }
    }

    override init() {
        let defaults = UserDefaults.standard
        port = defaults.object(forKey: "port") as? Int ?? 27015
        canvasWidth = (defaults.object(forKey: "canvasWidth") as? Int ?? 4000)
            .clamped(to: Self.canvasSizeRange)
        canvasHeight = (defaults.object(forKey: "canvasHeight") as? Int ?? 4000)
            .clamped(to: Self.canvasSizeRange)
        targetFramesPerSecond = defaults.object(forKey: "targetFramesPerSecond") as? Double ?? 60
        scale = defaults.object(forKey: "scale") as? Double ?? 1.0
        strokeWidth = defaults.object(forKey: "strokeWidth") as? Double ?? 4.0
        labelOffset = defaults.object(forKey: "labelOffset") as? Double ?? 0.0
        pointDistance = defaults.object(forKey: "pointDistance") as? Double ?? 40.0
        boxWidth = defaults.object(forKey: "boxWidth") as? Double ?? 360.0
        boxHeight = defaults.object(forKey: "boxHeight") as? Double ?? 140.0
        boxPadding = defaults.object(forKey: "boxPadding") as? Double ?? 16.0
        boxDistance = defaults.object(forKey: "boxDistance") as? Double ?? 40.0
        fontSize = defaults.object(forKey: "fontSize") as? Double ?? 36.0
        fontFamily = defaults.string(forKey: "fontFamily") ?? ""
        // Migrates the earlier boolean "fontBold" setting to a weight.
        fontWeight = defaults.object(forKey: "fontWeight") as? Int
            ?? ((defaults.object(forKey: "fontBold") as? Bool ?? true) ? 700 : 400)
        fontItalic = defaults.object(forKey: "fontItalic") as? Bool ?? false
        appearance = AppearancePreference(
            rawValue: defaults.string(forKey: "appearance") ?? "") ?? .system
        if let components = defaults.array(forKey: "accentColor") as? [Double],
           components.count == 4 {
            accentColor = Color(.sRGB, red: components[0], green: components[1],
                                blue: components[2], opacity: components[3])
        } else {
            accentColor = Color(.sRGB, red: 1, green: 0, blue: 0, opacity: 1)
        }
        super.init()

        engine.delegate = self
        pushAllSettings()
        engine.start()
        applyAppearance()
        // Pre-warm the font family enumeration at idle so the inspector's
        // first reveal doesn't pay for it mid-animation.
        Task { [weak self] in _ = self?.fontFamilies }
    }

    /// Applies the appearance preference app-wide (menus, windows, canvas
    /// backdrop palette — the canvas view observes effectiveAppearance).
    /// NSApplication.shared, not NSApp: this first runs during App.init,
    /// before NSApp is populated (NSApp is nil there and would trap).
    func applyAppearance() {
        NSApplication.shared.appearance = appearance.nsAppearance
    }

    /// didSet observers don't run during init, so mirror everything once.
    private func pushAllSettings() {
        engine.port = Int32(port)
        engine.canvasWidth = Int32(canvasWidth)
        engine.canvasHeight = Int32(canvasHeight)
        engine.targetFramesPerSecond = targetFramesPerSecond
        engine.scale = scale
        engine.strokeWidth = strokeWidth
        engine.labelOffset = labelOffset
        engine.pointDistance = pointDistance
        engine.boxWidth = boxWidth
        engine.boxHeight = boxHeight
        engine.boxPadding = boxPadding
        engine.boxDistance = boxDistance
        engine.fontSize = fontSize
        engine.fontFamily = fontFamily
        engine.fontWeight = Int32(fontWeight)
        engine.fontItalic = fontItalic
        engine.accentColor = NSColor(accentColor)
        listening = engine.listening
        statusText = engine.statusText
    }

    private func save(_ key: String, _ value: Any) {
        UserDefaults.standard.set(value, forKey: key)
    }

    func toggleListening() {
        if engine.listening { engine.stop() } else { engine.start() }
    }

    func clearScene() {
        engine.clearScene()
        feed.removeAll()
    }

    // MARK: Fonts

    /// Cached: enumerating the font manager on every SwiftUI body
    /// evaluation (the inspector re-evaluates on each metrics flush) is a
    /// measurable stutter.
    @ObservationIgnored lazy var fontFamilies: [String] =
        NSFontManager.shared.availableFontFamilies.sorted()

    /// The style variants a family offers (Regular, Semibold Italic, …),
    /// each mapped to the weight/slant the engine's typeface matching uses.
    func fontStyleOptions(for family: String) -> [FontStyleOption] {
        guard !family.isEmpty,
              let members = NSFontManager.shared.availableMembers(ofFontFamily: family)
        else { return [] }
        return members.compactMap { member in
            guard member.count >= 4,
                  let postScriptName = member[0] as? String,
                  let styleName = member[1] as? String,
                  let appKitWeight = member[2] as? Int,
                  let traits = member[3] as? UInt
            else { return nil }
            return FontStyleOption(
                styleName: styleName,
                postScriptName: postScriptName,
                weight: FontStyleOption.cssWeight(fromAppKitWeight: appKitWeight),
                italic: traits & NSFontTraitMask.italicFontMask.rawValue != 0)
        }
    }

    /// The current selection's display name within its family, for the
    /// style combo box's collapsed state.
    func currentFontStyleName() -> String {
        let options = fontStyleOptions(for: fontFamily)
        let match = options.min { lhs, rhs in
            score(lhs) < score(rhs)
        }
        return match?.styleName ?? (fontWeight >= 600 ? "Bold" : "Regular")
    }

    private func score(_ option: FontStyleOption) -> Int {
        abs(option.weight - fontWeight) + (option.italic == fontItalic ? 0 : 1000)
    }

    // MARK: SCKEngineDelegate (main queue)
    //
    // Published directly, per event: @Observable invalidation is scoped to
    // the views that read each property, so a feed insert re-evaluates only
    // the feed list and a metrics change only the status rows — no batching
    // needed. (The old 0.5 s flush existed because ObservableObject
    // republished the whole window tree per change.)

    /// A 200-row feed diffs measurably cheaper than the Qt app's 500
    /// while still far exceeding what anyone reads back.
    static let feedCap = 200

    func engineDidRenderScene(_ engine: SCKEngine) {
        canvasView?.redraw()
        refreshMetrics()
    }

    func engine(_ engine: SCKEngine, didAppend entry: SCKFeedEntry) {
        // Console order: oldest first, newest appended at the bottom; the
        // cap trims from the top (FeedTableView compensates the scroll
        // position so trims never move what the user is reading).
        feed.append(
            FeedEntry(timestamp: entry.timestamp, source: entry.source,
                      message: entry.message))
        if feed.count > Self.feedCap {
            feed.removeFirst(feed.count - Self.feedCap)
        }
    }

    func engineStatusDidChange(_ engine: SCKEngine) {
        listening = engine.listening
        statusText = engine.statusText
    }

    private func refreshMetrics() {
        // Deadbands so frame-to-frame jitter (3.7 ↔ 3.9 ms, 60 ↔ 61
        // scenes/s) doesn't flicker the readouts — display smoothing, not
        // batching.
        let millis = engine.renderMillis
        if abs(millis - renderMillis) > 0.3 || (millis == 0) != (renderMillis == 0) {
            renderMillis = (millis * 10).rounded() / 10
        }
        let rate = engine.scenesPerSecond
        if abs(rate - scenesPerSecond) > 1.5 || (rate == 0) != (scenesPerSecond == 0) {
            scenesPerSecond = rate.rounded()
        }
        // The engine stops calling back when the stream stops; check once
        // more after its 2 s staleness window so the rate settles to zero
        // instead of freezing on the last value.
        if rate > 0 { scheduleRateSettleCheck() }
    }

    @ObservationIgnored private var rateSettleTask: Task<Void, Never>?

    private func scheduleRateSettleCheck() {
        guard rateSettleTask == nil else { return }
        rateSettleTask = Task { [weak self] in
            try? await Task.sleep(for: .seconds(2.5))
            guard let self, !Task.isCancelled else { return }
            self.rateSettleTask = nil
            self.refreshMetrics()
        }
    }
}
