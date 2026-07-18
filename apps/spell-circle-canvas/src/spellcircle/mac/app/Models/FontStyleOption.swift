/// One style variant of a font family, as enumerated by NSFontManager.
struct FontStyleOption: Identifiable, Hashable {
    let styleName: String
    let postScriptName: String
    let weight: Int // CSS scale, 100–900
    let italic: Bool

    var id: String { postScriptName }

    /// AppKit's 0–15 weight scale mapped onto the CSS 100–900 scale the
    /// engine (SkFontStyle) uses. 5 is regular, 9 is bold.
    static func cssWeight(fromAppKitWeight appKitWeight: Int) -> Int {
        switch appKitWeight {
        case ..<2: return 100
        case 2: return 200
        case 3: return 300
        case 4: return 350
        case 5: return 400
        case 6: return 500
        case 7: return 500
        case 8: return 600
        case 9: return 700
        case 10, 11: return 800
        default: return 900
        }
    }
}
