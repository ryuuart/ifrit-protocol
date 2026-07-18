import SwiftUI

/// Canvas inspector (trailing split-view column): attributes of the scene
/// content itself — colors, typography, geometry, canvas size — per the
/// HIG's inspector/settings split. App-level behavior (render rate) lives
/// in the standard Settings window instead (AppSettingsView, ⌘,).
/// Sections mirror the Qt app's GraphicsSettingsForm.
struct SettingsPane: View {
    @Environment(EngineModel.self) private var model

    var body: some View {
        @Bindable var model = model
        VStack(alignment: .leading, spacing: 0) {
            HStack {
                Text("Canvas")
                    .font(.subheadline.weight(.semibold))
                Spacer()
                Button {
                    model.showSettings = false
                } label: {
                    Image(systemName: "xmark.circle.fill")
                        .foregroundStyle(.secondary)
                }
                .buttonStyle(.plain)
                .help("Close inspector")
            }
            .padding(.horizontal, 16)
            .padding(.vertical, 12)

            Divider()

            ScrollView {
                VStack(alignment: .leading, spacing: 18) {
                    section("Appearance") {
                        ColorPicker("Accent color", selection: $model.accentColor)
                        NumericSettingRow(title: "Stroke width",
                                          value: $model.strokeWidth,
                                          range: 0.5...100, step: 0.5)
                        NumericSettingRow(title: "Scale", value: $model.scale,
                                          range: 0.05...16, step: 0.25)
                    }
                    section("Labels") {
                        FontFamilyComboBox()
                        FontStyleComboBox()
                        NumericSettingRow(title: "Font size",
                                          value: $model.fontSize,
                                          range: 1...512, step: 1)
                        NumericSettingRow(title: "Label offset",
                                          value: $model.labelOffset,
                                          range: -1000...1000, step: 5)
                        NumericSettingRow(title: "Point distance",
                                          value: $model.pointDistance,
                                          range: 0...1000, step: 5)
                    }
                    section("Boxes") {
                        NumericSettingRow(title: "Width", value: $model.boxWidth,
                                          range: 1...4000, step: 10)
                        NumericSettingRow(title: "Height", value: $model.boxHeight,
                                          range: 1...4000, step: 10)
                        NumericSettingRow(title: "Padding", value: $model.boxPadding,
                                          range: 0...500, step: 2)
                        NumericSettingRow(title: "Distance", value: $model.boxDistance,
                                          range: 0...1000, step: 5)
                    }
                    section("Canvas") {
                        CanvasSizeRow(title: "Width", value: $model.canvasWidth)
                        CanvasSizeRow(title: "Height", value: $model.canvasHeight)
                        Text("Native pixels, \(EngineModel.canvasSizeRange.lowerBound)–\(EngineModel.canvasSizeRange.upperBound). Applied after you pause typing.")
                            .font(.caption)
                            .foregroundStyle(.tertiary)
                    }
                }
                .padding(16)
            }
        }
    }

    @ViewBuilder
    private func section(_ title: String,
                         @ViewBuilder content: () -> some View) -> some View {
        VStack(alignment: .leading, spacing: 9) {
            Text(title.uppercased())
                .font(.caption2.weight(.semibold))
                .foregroundStyle(.secondary)
                .kerning(0.8)
            content()
        }
    }
}
