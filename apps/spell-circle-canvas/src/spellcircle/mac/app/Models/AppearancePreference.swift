import AppKit

/// Forced app appearance, persisted in UserDefaults. The canvas backdrop
/// palette follows automatically via viewDidChangeEffectiveAppearance.
enum AppearancePreference: String, CaseIterable, Identifiable {
    case system, light, dark

    var id: Self { self }

    var label: String {
        switch self {
        case .system: "System"
        case .light: "Light"
        case .dark: "Dark"
        }
    }

    var nsAppearance: NSAppearance? {
        switch self {
        case .system: nil
        case .light: NSAppearance(named: .aqua)
        case .dark: NSAppearance(named: .darkAqua)
        }
    }
}
