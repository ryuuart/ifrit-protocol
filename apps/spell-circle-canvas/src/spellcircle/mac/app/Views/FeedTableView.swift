import AppKit
import SwiftUI

/// The activity feed as a console-style AppKit NSTableView: newest rows
/// append at the bottom; the view follows the tail only while the user is
/// already there, and otherwise holds the exact reading position — even as
/// the row cap trims old rows off the top. SwiftUI's List exposes no scroll
/// control for those semantics, and its per-row diffing costs main-thread
/// time during pane animations; NSTableView materializes only visible rows
/// and reloads without diffing, which makes high-rate log updates free.
struct FeedTableView: NSViewRepresentable {
    var entries: [FeedEntry] // oldest first; newest at the bottom

    func makeCoordinator() -> Coordinator { Coordinator() }

    func makeNSView(context: Context) -> NSScrollView {
        let table = NSTableView()
        table.headerView = nil
        table.backgroundColor = .clear
        table.selectionHighlightStyle = .none
        table.intercellSpacing = NSSize(width: 0, height: 0)
        table.rowHeight = 38
        table.usesAutomaticRowHeights = false
        table.dataSource = context.coordinator
        table.delegate = context.coordinator

        let column = NSTableColumn(identifier: .init("entry"))
        column.resizingMask = .autoresizingMask
        table.addTableColumn(column)
        table.sizeLastColumnToFit()

        let scroll = NSScrollView()
        scroll.documentView = table
        scroll.hasVerticalScroller = true
        scroll.drawsBackground = false
        scroll.autohidesScrollers = true
        return scroll
    }

    func updateNSView(_ scrollView: NSScrollView, context: Context) {
        let coordinator = context.coordinator
        guard let table = scrollView.documentView as? NSTableView else { return }
        let old = coordinator.entries
        // The model only ever appends at the tail and trims the head, so
        // newest-id + count identifies the array.
        guard old.count != entries.count || old.last?.id != entries.last?.id
        else { return }

        // Console semantics, decided against pre-reload geometry: follow
        // the tail only when the user is already at it (or the feed was
        // empty); otherwise hold the reading position exactly.
        let clip = scrollView.contentView
        let wasPinnedToTail = old.isEmpty ||
            clip.bounds.maxY >= table.frame.maxY - table.rowHeight / 2

        // Rows the cap trimmed off the top = where the new head sat in the
        // old array (arrays are ≤ feedCap, the scan is trivial).
        var trimmed = 0
        if let newHead = entries.first, !old.isEmpty {
            trimmed = old.firstIndex { $0.id == newHead.id } ?? old.count
        }

        coordinator.entries = entries
        table.reloadData()

        if wasPinnedToTail {
            if !entries.isEmpty {
                table.scrollRowToVisible(entries.count - 1)
            }
        } else if trimmed > 0 {
            // Trimming removed content above the viewport; shift the scroll
            // origin up by the same height so visible rows stay put.
            let origin = NSPoint(
                x: clip.bounds.origin.x,
                y: max(0, clip.bounds.origin.y - CGFloat(trimmed) * table.rowHeight))
            clip.scroll(to: origin)
            scrollView.reflectScrolledClipView(clip)
        }
    }

    final class Coordinator: NSObject, NSTableViewDataSource, NSTableViewDelegate {
        var entries: [FeedEntry] = []

        func numberOfRows(in tableView: NSTableView) -> Int {
            entries.count
        }

        func tableView(_ tableView: NSTableView,
                       viewFor tableColumn: NSTableColumn?,
                       row: Int) -> NSView? {
            let identifier = NSUserInterfaceItemIdentifier("feedCell")
            let cell = tableView.makeView(withIdentifier: identifier,
                                          owner: nil) as? FeedCellView
                ?? FeedCellView(identifier: identifier)
            let entry = entries[row]
            cell.messageField.stringValue = entry.message
            cell.detailField.stringValue = "\(entry.clockTime) · \(entry.source)"
            return cell
        }
    }
}

/// Two-line log cell: message over a dimmed timestamp/source detail line.
final class FeedCellView: NSTableCellView {
    let messageField = NSTextField(labelWithString: "")
    let detailField = NSTextField(labelWithString: "")

    init(identifier: NSUserInterfaceItemIdentifier) {
        super.init(frame: .zero)
        self.identifier = identifier

        messageField.font = .systemFont(ofSize: 12)
        messageField.textColor = .labelColor
        messageField.lineBreakMode = .byTruncatingTail
        detailField.font = .monospacedDigitSystemFont(ofSize: 10,
                                                      weight: .regular)
        detailField.textColor = .tertiaryLabelColor
        detailField.lineBreakMode = .byTruncatingTail

        for field in [messageField, detailField] {
            field.translatesAutoresizingMaskIntoConstraints = false
            addSubview(field)
        }
        NSLayoutConstraint.activate([
            messageField.topAnchor.constraint(equalTo: topAnchor, constant: 4),
            messageField.leadingAnchor.constraint(equalTo: leadingAnchor, constant: 10),
            messageField.trailingAnchor.constraint(lessThanOrEqualTo: trailingAnchor, constant: -10),
            detailField.topAnchor.constraint(equalTo: messageField.bottomAnchor, constant: 1),
            detailField.leadingAnchor.constraint(equalTo: messageField.leadingAnchor),
            detailField.trailingAnchor.constraint(lessThanOrEqualTo: trailingAnchor, constant: -10),
        ])
    }

    @available(*, unavailable)
    required init?(coder: NSCoder) { fatalError() }
}
