import SwiftUI

/// A labeled numeric input: free-form text entry plus a stepper for
/// fine-grained adjustment (HIG: text fields for arbitrary values, steppers
/// for small increments — never sliders for precise numbers).
struct NumericSettingRow: View {
    let title: String
    @Binding var value: Double
    let range: ClosedRange<Double>
    var step: Double = 1

    var body: some View {
        HStack {
            Text(title)
            Spacer()
            TextField(title, value: $value,
                      format: .number.precision(.fractionLength(0...2)))
                .textFieldStyle(.roundedBorder)
                .multilineTextAlignment(.trailing)
                .monospacedDigit()
                .frame(width: 68)
                .labelsHidden()
                .onChange(of: value) { _, newValue in
                    let bounded = newValue.clamped(to: range)
                    if bounded != newValue { value = bounded }
                }
            Stepper(title, value: $value, in: range, step: step)
                .labelsHidden()
        }
    }
}

/// A canvas-dimension input: debounced so half-typed values (or a stray 0)
/// never reach texture allocation, then clamped to the valid range.
struct CanvasSizeRow: View {
    let title: String
    @Binding var value: Int

    @State private var text = ""

    var body: some View {
        HStack {
            Text(title)
            Spacer()
            TextField(title, text: $text)
                .textFieldStyle(.roundedBorder)
                .multilineTextAlignment(.trailing)
                .monospacedDigit()
                .frame(width: 68)
                .labelsHidden()
                .onSubmit(applyNow)
            Stepper(title, value: $value,
                    in: EngineModel.canvasSizeRange, step: 100)
                .labelsHidden()
        }
        .onAppear { text = String(value) }
        .onChange(of: value) { _, newValue in
            // External change (stepper, clamping): mirror into the field.
            if Int(text) != newValue { text = String(newValue) }
        }
        // Debounce via task identity: every edit cancels the previous task
        // and starts a new one, so the value applies only after a typing
        // pause (or immediately on submit above).
        .task(id: text) {
            guard let typed = Int(text), typed != value else { return }
            try? await Task.sleep(for: .seconds(0.6))
            guard !Task.isCancelled else { return }
            applyNow()
        }
    }

    private func applyNow() {
        guard let typed = Int(text) else {
            text = String(value)
            return
        }
        let bounded = typed.clamped(to: EngineModel.canvasSizeRange)
        value = bounded
        text = String(bounded)
    }
}
