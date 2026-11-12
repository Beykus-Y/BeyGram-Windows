// ArchivePage — archived chats browser.
//
// The parent owns the data layer. It assigns `model` from ChatListModel (roles:
// chatId, title, preview, unreadCount, pinned, muted, lastDate, initials,
// accentIndex, isChannel, chatType, photoFileId, photoReady, photoRevision),
// keeps `loading` in sync, and reacts to chatSelected() by opening the chat and
// to backRequested() by leaving the archive. `lastDate` is expected in Unix
// seconds, matching the chat list and message models.
//
// Filtering happens inside a DelegateModel group so the source model keeps its
// order and the view still gets a real count for the header and empty state.
// The unarchive action is the same one the archive entry in ChatsPage exposes,
// so the menu calls BeyGramApp.setChatArchived(chatId, false) directly.

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQml.Models
import BeyGram.Theme 1.0
import BeyGram.App 1.0
import "../ds/design.js" as Design

Item {
    id: page

    property var model: null
    property bool loading: false

    signal chatSelected(var chatId)
    signal backRequested()

    readonly property string query: searchField.text.trim()

    function timeLabel(value) {
        if (!value || value <= 0)
            return ""
        const date = new Date(value * 1000)
        if (isNaN(date.getTime()))
            return ""
        const now = new Date()
        if (date.toDateString() === now.toDateString())
            return Qt.formatTime(date, "hh:mm")
        return Qt.formatDate(date, "dd.MM")
    }

    function applyFilter() {
        const needle = searchField.text.trim().toLowerCase()
        const items = filteredModel.items
        if (!items || items.count === 0)
            return

        // Reset the visible group, then add back every match in source order.
        items.setGroups(0, items.count, ["items"])
        for (let i = 0; i < items.count; ++i) {
            const entry = items.get(i)
            if (!entry || !entry.model)
                continue
            const data = entry.model
            if (needle.length === 0
                || (data.title || "").toLowerCase().indexOf(needle) >= 0
                || (data.preview || "").toLowerCase().indexOf(needle) >= 0) {
                entry.inVisible = true
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        DsHeader {
            Layout.fillWidth: true
            title: "Archived chats"
            subtitle: page.loading && archiveList.count === 0
                      ? "Loading archived chats"
                      : archiveList.count + (archiveList.count === 1 ? " chat" : " chats")

            leading: DsIconButton {
                icon: "\uE72B"
                tooltip: "Back to chats"
                onClicked: page.backRequested()
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: Design.space4
            spacing: Design.space3

            DsSearchField {
                id: searchField
                Layout.fillWidth: true
                placeholderText: "Search archived chats"
                Accessible.name: "Search archived chats"
                onTextChanged: page.applyFilter()
            }

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                ListView {
                    id: archiveList
                    anchors.fill: parent
                    clip: true
                    spacing: 2
                    model: filteredModel
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                    footer: Item {
                        width: archiveList.width
                        height: (page.loading && archiveList.count > 0) ? 44 : 0

                        BusyIndicator {
                            anchors.centerIn: parent
                            running: page.loading && archiveList.count > 0
                            visible: running
                            implicitWidth: 22
                            implicitHeight: 22
                        }
                    }
                }

                BusyIndicator {
                    anchors.centerIn: parent
                    running: page.loading && archiveList.count === 0
                    visible: running
                    implicitWidth: 32
                    implicitHeight: 32
                }

                DsEmptyState {
                    anchors.centerIn: parent
                    visible: !page.loading && archiveList.count === 0
                    glyph: "🗄"
                    title: page.query.length > 0 ? "No matches" : "No archived chats"
                    description: page.query.length > 0
                                 ? "No archived chat matches your search."
                                 : "Chats you archive will appear here."
                    actionText: page.query.length > 0 ? "Clear search" : ""
                    onActionRequested: searchField.clearText()
                }
            }
        }
    }

    // --------------------------------------------------------------
    // Filtered view over the parent-supplied model
    // --------------------------------------------------------------
    DelegateModel {
        id: filteredModel
        model: page.model
        filterOnGroup: "visible"

        groups: DelegateModelGroup {
            name: "visible"
            includeByDefault: false
        }

        // New or removed source rows must be filtered again. applyFilter()
        // itself only changes the "visible" group, never the "items" group.
        items.onChanged: Qt.callLater(page.applyFilter)

        delegate: Item {
            id: delegateItem

            // Roles are qint64 in ChatListModel; keep ids and dates as var so
            // large values do not fail the delegate binding.
            required property var chatId
            required property string title
            required property string preview
            required property int unreadCount
            required property bool pinned
            required property bool muted
            required property var lastDate
            required property string initials
            required property int accentIndex
            required property int photoFileId
            required property bool photoReady
            required property int photoRevision

            width: archiveList.width
            height: Design.rowHeight

            DsChatRow {
                anchors.fill: parent
                title: delegateItem.title
                preview: delegateItem.preview
                timeText: page.timeLabel(delegateItem.lastDate)
                unread: delegateItem.unreadCount
                pinned: delegateItem.pinned
                muted: delegateItem.muted
                initials: delegateItem.initials
                accentIndex: delegateItem.accentIndex
                imageSource: delegateItem.photoReady && delegateItem.photoFileId > 0
                             ? "image://beygrammedia/" + delegateItem.photoFileId
                               + "?v=" + delegateItem.photoRevision + "&round=1"
                             : ""
                onClicked: page.chatSelected(delegateItem.chatId)
            }

            // Right click opens the row menu; left clicks fall through to the
            // DsChatRow mouse area above.
            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.RightButton
                onClicked: {
                    rowMenu.chatId = delegateItem.chatId
                    rowMenu.openAtCursor()
                }
            }
        }
    }

    DsMenu {
        id: rowMenu
        property var chatId: 0

        DsMenuItem {
            text: "Unarchive"
            glyph: "🗄"
            enabled: rowMenu.chatId !== 0
            onTriggered: BeyGramApp.setChatArchived(rowMenu.chatId, false)
        }
    }

    Connections {
        target: page.model
        ignoreUnknownSignals: true

        function onModelReset() { Qt.callLater(page.applyFilter) }
        function onRowsInserted() { Qt.callLater(page.applyFilter) }
        function onRowsRemoved() { Qt.callLater(page.applyFilter) }
        function onDataChanged() { Qt.callLater(page.applyFilter) }
        function onCountChanged() { Qt.callLater(page.applyFilter) }
    }

    onModelChanged: Qt.callLater(page.applyFilter)
    Component.onCompleted: Qt.callLater(page.applyFilter)
}
