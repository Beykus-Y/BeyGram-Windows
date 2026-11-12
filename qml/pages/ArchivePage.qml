pragma ComponentBehavior: Bound

// ArchivePage — archived chats browser.
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
    readonly property bool compact: BeyGramAppearance.listDensity === "Compact"

    function timeLabel(value) {
        var _rev = BeyGramLang.revision
        const ts = Number(value)
        if (!ts || ts <= 0)
            return ""
        return BeyGramLang.formatChatTimestamp(ts)
    }

    function applyFilter() {
        const needle = searchField.text.trim().toLowerCase()
        const items = filteredModel.items
        if (!items || items.count === 0)
            return

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
            title: qsTr("Archived chats")
            subtitle: page.loading && archiveList.count === 0
                      ? qsTr("Loading archived chats")
                      : qsTr("%n chat(s)", "", archiveList.count)

            leading: DsIconButton {
                icon: "\uE72B"
                tooltip: qsTr("Back to chats")
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
                placeholderText: qsTr("Search archived chats")
                Accessible.name: qsTr("Search archived chats")
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

                        DsSpinner {
                            anchors.centerIn: parent
                            running: page.loading && archiveList.count > 0
                            visible: running
                        }
                    }
                }

                DsSpinner {
                    anchors.centerIn: parent
                    running: page.loading && archiveList.count === 0
                    visible: running
                }

                DsEmptyState {
                    anchors.centerIn: parent
                    visible: !page.loading && archiveList.count === 0
                    glyph: "\uD83D\uDDC4"
                    title: page.query.length > 0
                           ? qsTr("No matches")
                           : qsTr("No archived chats")
                    description: page.query.length > 0
                                 ? qsTr("No archived chat matches your search.")
                                 : qsTr("Chats you archive will appear here.")
                    actionText: page.query.length > 0 ? qsTr("Clear search") : ""
                    onActionRequested: searchField.clearText()
                }
            }
        }
    }

    DelegateModel {
        id: filteredModel
        model: page.model
        filterOnGroup: "visible"

        groups: DelegateModelGroup {
            name: "visible"
            includeByDefault: false
        }

        items.onChanged: Qt.callLater(page.applyFilter)

        delegate: Item {
            id: delegateItem

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
            height: page.compact ? Design.rowHeightCompact : Design.rowHeight

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
                               + "?v=" + delegateItem.photoRevision
                             : ""
                onClicked: page.chatSelected(delegateItem.chatId)
            }

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
            text: qsTr("Unarchive")
            glyph: "\uD83D\uDDC4"
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
