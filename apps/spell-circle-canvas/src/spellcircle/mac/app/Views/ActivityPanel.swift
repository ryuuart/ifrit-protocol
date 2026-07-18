import SwiftUI

/// Sidebar content: connection status and readouts as a (static) sidebar
/// List, with the activity feed below it as a console-style AppKit table
/// (see FeedTableView for why a SwiftUI List doesn't fit that job).
struct ActivityPanel: View {
    @Environment(EngineModel.self) private var model

    var body: some View {
        @Bindable var model = model
        VStack(spacing: 0) {
            List {
                Section {
                    HStack(spacing: 8) {
                        Circle()
                            .fill(model.listening ? Color.green : Color.secondary.opacity(0.5))
                            .frame(width: 7, height: 7)
                        Text(model.statusText)
                            .font(.callout)
                            .lineLimit(1)
                    }

                    HStack(spacing: 8) {
                        Text("Port")
                            .foregroundStyle(.secondary)
                        TextField("Port", value: $model.port,
                                  format: .number.grouping(.never))
                            .textFieldStyle(.roundedBorder)
                            .multilineTextAlignment(.center)
                            .monospacedDigit()
                            .frame(width: 60)
                            .labelsHidden()
                        Spacer()
                        Button(action: model.toggleListening) {
                            Text(model.listening ? "Stop" : "Listen")
                                .frame(width: 52)
                        }
                        .controlSize(.small)
                    }
                }

                Section("Status") {
                    infoRow("Zoom", model.zoomPercent > 0
                        ? "\(Int(model.zoomPercent.rounded()))%" : "—")
                    infoRow("Canvas", "\(model.canvasWidth) × \(model.canvasHeight) px")
                    infoRow("Render", model.renderMillis > 0
                        ? String(format: "%.1f ms", model.renderMillis) : "—")
                    infoRow("Rate", model.scenesPerSecond > 0.5
                        ? String(format: "%.0f scenes/s", model.scenesPerSecond) : "—")
                    // View-zoom actions live in the window toolbar.
                }
            }
            .listStyle(.sidebar)
            .scrollDisabled(true)
            .frame(height: 218)

            activityHeader
                .padding(.horizontal, 16)
                .padding(.top, 2)
                .padding(.bottom, 6)

            if model.feed.isEmpty {
                VStack(spacing: 8) {
                    Image(systemName: "dot.radiowaves.up.forward")
                        .font(.title3)
                        .foregroundStyle(.tertiary)
                    Text("Waiting for scenes on UDP :\(String(model.port))")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                        .multilineTextAlignment(.center)
                }
                .frame(maxWidth: .infinity, maxHeight: .infinity)
            } else {
                // Console feed (AppKit table, see FeedTableView): newest at
                // the bottom, follows the tail only when pinned there, and
                // holds the reading position through cap trims — semantics
                // a SwiftUI List can't express, and its per-packet diffing
                // costs main-thread time during pane animations.
                FeedTableView(entries: model.feed)
            }
        }
    }

    private var activityHeader: some View {
        HStack(spacing: 8) {
            Text("Activity")
                .font(.subheadline.weight(.semibold))
                .foregroundStyle(.secondary)
            // Clear sits right beside the list's name (HIG proximity),
            // labeled explicitly (HIG buttons: text when an icon alone is
            // ambiguous): wipes the scene and this feed together.
            Button("Clear", action: model.clearScene)
                .buttonStyle(.plain)
                .font(.caption.weight(.medium))
                .foregroundStyle(model.feed.isEmpty ? .tertiary : .secondary)
                .disabled(model.feed.isEmpty)
                .help("Clear scene and activity feed")
            Spacer()
            Text(model.feed.isEmpty ? "" : "\(model.feed.count)")
                .font(.caption.monospacedDigit())
                .foregroundStyle(.secondary)
        }
    }

    /// A readout row: LabeledContent pairs label and value semantically
    /// (accessibility reads them as one element), styled to keep the value
    /// primary and the label dimmed.
    private func infoRow(_ label: String, _ value: String) -> some View {
        LabeledContent {
            Text(value)
                .monospacedDigit()
                .foregroundStyle(.primary)
        } label: {
            Text(label)
                .foregroundStyle(.secondary)
        }
        .font(.callout)
    }
}
