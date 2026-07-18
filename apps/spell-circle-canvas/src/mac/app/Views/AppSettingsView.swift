import SwiftUI

/// App-level settings, in the standard Settings window (⌘,) per the HIG:
/// engine behavior that isn't an attribute of the scene content.
struct AppSettingsView: View {
    @Environment(EngineModel.self) private var model

    var body: some View {
        @Bindable var model = model
        Form {
            Section {
                Picker("Appearance", selection: $model.appearance) {
                    ForEach(AppearancePreference.allCases) { preference in
                        Text(preference.label).tag(preference)
                    }
                }
                .pickerStyle(.segmented)
            } header: {
                Text("Appearance")
            }

            Section {
                NumericSettingRow(title: "Target FPS",
                                  value: $model.targetFramesPerSecond,
                                  range: 1...240, step: 10)
            } header: {
                Text("Rendering")
            } footer: {
                Text("Render rate cap for incoming scenes (60, 120, 144, …). Frames keep rendering — and publishing to Syphon — while the display sleeps.")
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }
        }
        .formStyle(.grouped)
        .frame(width: 380, height: 250)
    }
}
