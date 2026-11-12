pragma ComponentBehavior: Bound

// ChatFolderTabs.qml — folder tab strip for the chat list.
//
// Folder filtering is wired by the parent page: ChatsPage.qml owns
// `currentFolderId`, passes it in here, and reacts to folderSelected(folderId)
// by switching the chat list it shows. The folders come from
// ChatFolderListModel (roles: folderId, title, icon, unreadCount, isMain);
// this component only maps that model onto DsTabs, which takes a plain list of
// labels, and translates the selected tab index back into a folder id. It
// never filters, fetches, or calls into C++ itself.
//
// The main list is row 0 of the model (id 0, "All chats"), so the "All" entry
// is simply the first tab; no extra entry is synthesised here. Unread counts
// are not rendered because DsTabs labels are plain text.

import QtQuick

Item {
    id: root

    property var model: null
    property var currentFolderId: 0

    signal folderSelected(var folderId)

    // Number of rows in `model` (QAbstractListModel exposes `count`; a plain
    // JS array exposes `length`).
    readonly property int _count: {
        if (!root.model)
            return 0
        if (root.model.count !== undefined)
            return Number(root.model.count)
        if (root.model.length !== undefined)
            return Number(root.model.length)
        return 0
    }

    // Row index -> { id, title }, filled by the probe delegates below. A
    // QAbstractListModel cannot be handed to DsTabs directly, so the probe
    // materialises the titles DsTabs needs while the id array preserves the
    // folder mapping.
    property var _rows: ({})
    property var _labels: []
    property var _ids: []

    readonly property int _currentIndex: {
        const wanted = Number(root.currentFolderId)
        for (let i = 0; i < root._ids.length; ++i) {
            if (root._ids[i] === wanted)
                return i
        }
        return -1
    }

    function _collect(index, folderId, title) {
        if (index < 0)
            return
        root._rows[index] = {
            id: Number(folderId),
            title: title === undefined || title === null ? "" : String(title)
        }
        root._publish()
    }

    function _publish() {
        const count = root._count
        const labels = []
        const ids = []

        for (let i = 0; i < count; ++i) {
            const row = root._rows[i]
            if (row === undefined)
                return
            labels.push(row.title)
            ids.push(row.id)
        }

        let unchanged = labels.length === root._labels.length
        for (let i = 0; unchanged && i < labels.length; ++i) {
            unchanged = labels[i] === root._labels[i] && ids[i] === root._ids[i]
        }
        if (unchanged)
            return

        root._labels = labels
        root._ids = ids
    }

    implicitWidth: tabs.implicitWidth
    implicitHeight: tabs.implicitHeight

    DsTabs {
        id: tabs

        anchors.fill: parent
        model: root._labels
        currentIndex: root._currentIndex

        onActivated: function(index) {
            if (index >= 0 && index < root._ids.length)
                root.folderSelected(root._ids[index])

            // DsTabs assigns currentIndex itself on click, which would break
            // the binding to currentFolderId. Restore it so the strip always
            // reflects the folder the page actually selected.
            tabs.currentIndex = Qt.binding(function() {
                return root._currentIndex
            })
        }
    }

    // Invisible probe: one delegate per folder row, used only to read the
    // model roles that DsTabs cannot consume.
    Repeater {
        model: root.model

        // The model can shrink to zero without any delegate completing, so
        // clear the materialised labels when the probe's row count drops.
        onCountChanged: root._publish()

        delegate: Item {
            required property int index
            required property var folderId
            required property var title

            width: 0
            height: 0
            visible: false

            Component.onCompleted: root._collect(index, folderId, title)
        }
    }
}
