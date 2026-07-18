import SwiftUI

/// Combo-box font family picker: typing filters the family list, which
/// drops down automatically with every candidate previewed in its own
/// typeface; the chevron browses the full list.
struct FontFamilyComboBox: View {
    @Environment(EngineModel.self) private var model
    @State private var text = ""
    @State private var listVisible = false
    @FocusState private var fieldFocused: Bool

    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            HStack {
                Text("Font")
                Spacer()
                TextField("Font", text: $text)
                    .textFieldStyle(.roundedBorder)
                    .frame(width: 170)
                    .labelsHidden()
                    .focused($fieldFocused)
                    .onSubmit(commitTyped)
                Button {
                    if listVisible {
                        listVisible = false
                    } else {
                        text = ""
                        listVisible = true
                    }
                } label: {
                    Image(systemName: "chevron.up.chevron.down")
                        .font(.caption)
                }
                .help("Browse fonts")
            }
            if listVisible {
                familyList
                    .transition(.opacity)
            }
        }
        .onAppear { text = displayName(model.fontFamily) }
        .onChange(of: model.fontFamily) { _, family in
            text = displayName(family)
        }
        .onChange(of: text) { _, newText in
            // Typing pulls the filtered list up automatically; programmatic
            // resets (selection mirrors) close it again below.
            if fieldFocused && newText != displayName(model.fontFamily) {
                listVisible = true
            }
        }
        .onChange(of: fieldFocused) { _, focused in
            if focused {
                listVisible = true
            }
        }
        .onExitCommand {
            listVisible = false
            text = displayName(model.fontFamily)
        }
        .animation(.easeOut(duration: 0.15), value: listVisible)
    }

    private func displayName(_ family: String) -> String {
        family.isEmpty ? "System Default" : family
    }

    /// Families matching the typed text; everything while browsing.
    private var filteredFamilies: [String] {
        let typed = text.trimmingCharacters(in: .whitespaces)
        if typed.isEmpty || typed == displayName(model.fontFamily) {
            return model.fontFamilies
        }
        return model.fontFamilies.filter {
            $0.localizedCaseInsensitiveContains(typed)
        }
    }

    /// Accepts typed input: exact family match first, else the top
    /// filtered candidate, else revert.
    private func commitTyped() {
        let typed = text.trimmingCharacters(in: .whitespaces)
        defer { listVisible = false; fieldFocused = false }
        if typed.isEmpty || typed.caseInsensitiveCompare("System Default") == .orderedSame {
            model.fontFamily = ""
            text = displayName("")
            return
        }
        if let exact = model.fontFamilies.first(where: {
            $0.caseInsensitiveCompare(typed) == .orderedSame
        }) {
            model.fontFamily = exact
        } else if let first = filteredFamilies.first {
            model.fontFamily = first
            text = first
        } else {
            text = displayName(model.fontFamily)
        }
    }

    private var familyList: some View {
        ScrollViewReader { proxy in
            ScrollView {
                LazyVStack(alignment: .leading, spacing: 0) {
                    if filteredFamilies.count == model.fontFamilies.count {
                        familyRow(label: "System Default", family: "")
                    }
                    ForEach(filteredFamilies, id: \.self) { family in
                        familyRow(label: family, family: family)
                            .id(family)
                    }
                    if filteredFamilies.isEmpty {
                        Text("No matching fonts")
                            .font(.caption)
                            .foregroundStyle(.secondary)
                            .padding(8)
                    }
                }
                .padding(4)
            }
            .frame(height: 210)
            .background(.quaternary.opacity(0.5),
                        in: RoundedRectangle(cornerRadius: 8))
            .onAppear {
                if !model.fontFamily.isEmpty {
                    proxy.scrollTo(model.fontFamily, anchor: .center)
                }
            }
        }
    }

    private func familyRow(label: String, family: String) -> some View {
        let selected = model.fontFamily == family
        return Button {
            model.fontFamily = family
            text = displayName(family)
            listVisible = false
            fieldFocused = false
        } label: {
            HStack {
                // Each family previews itself, like the Qt font dialog.
                Text(label)
                    .font(family.isEmpty ? .body : .custom(family, size: 14))
                    .lineLimit(1)
                Spacer()
                if selected {
                    Image(systemName: "checkmark")
                        .font(.caption)
                }
            }
            .contentShape(Rectangle())
            .padding(.horizontal, 8)
            .padding(.vertical, 5)
            .background(selected ? Color.accentColor.opacity(0.2) : .clear,
                        in: RoundedRectangle(cornerRadius: 6))
        }
        .buttonStyle(.plain)
    }
}

/// Style variant picker for the selected family, each option previewed in
/// its actual face (Regular, Semibold Italic, …).
struct FontStyleComboBox: View {
    @Environment(EngineModel.self) private var model
    @State private var listVisible = false

    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            HStack {
                Text("Style")
                Spacer()
                Button {
                    listVisible.toggle()
                } label: {
                    HStack {
                        Text(model.currentFontStyleName())
                        Image(systemName: "chevron.up.chevron.down")
                            .font(.caption)
                    }
                }
                .disabled(styleOptions.isEmpty)
            }
            if listVisible {
                styleList
                    .transition(.opacity)
            }
        }
        .animation(.easeOut(duration: 0.15), value: listVisible)
    }

    private var styleOptions: [FontStyleOption] {
        model.fontStyleOptions(for: model.fontFamily)
    }

    private var styleList: some View {
        ScrollView {
            LazyVStack(alignment: .leading, spacing: 0) {
                ForEach(styleOptions) { option in
                    let selected = option.weight == model.fontWeight
                        && option.italic == model.fontItalic
                    Button {
                        model.fontWeight = option.weight
                        model.fontItalic = option.italic
                        listVisible = false
                    } label: {
                        HStack {
                            Text(option.styleName)
                                .font(.custom(option.postScriptName, size: 14))
                                .lineLimit(1)
                            Spacer()
                            if selected {
                                Image(systemName: "checkmark")
                                    .font(.caption)
                            }
                        }
                        .contentShape(Rectangle())
                        .padding(.horizontal, 8)
                        .padding(.vertical, 5)
                        .background(selected ? Color.accentColor.opacity(0.2) : .clear,
                                    in: RoundedRectangle(cornerRadius: 6))
                    }
                    .buttonStyle(.plain)
                }
            }
            .padding(4)
        }
        .frame(height: min(210, CGFloat(styleOptions.count) * 30 + 12))
        .background(.quaternary.opacity(0.5),
                    in: RoundedRectangle(cornerRadius: 8))
    }
}
