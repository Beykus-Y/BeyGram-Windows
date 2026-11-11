// ArchivePage — archived chats browser.
//
// The parent owns the data layer. It assigns `model` from ChatListModel (roles:
// chatId, title, preview, unreadCount, pinned, muted, lastDate, initials,
// accentIndex, isChannel, chatType, photoFileId, photoReady, photoRevision),
// keeps `loading` in sync, and reacts to chatSelected() by opening the chat and
// to backRequested() by leaving the archive. `lastDate` is expected in Unix
// seconds, matching the chat list and message models.
//
// This page only renders the list: no C++ singletons other than theme tokens,
// no network access, no logging. Chat photos are displayed when the model has
// already resolved them (photoReady); requesting them belongs to the parent.

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0

Item {
    id: page

    property var model: null
    property bool loading: false

    signal chatSelected(var chatId)
    signal backRequested()

    readonly property var avatarColors: [
        "#C2593F", "#5B4BC4", "#2F8A7B",
        "#A9731D", "#3A78C2", "#B0476E"
    ]

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

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 14

        // --------------------------------------------------------------
        // Header
        // --------------------------------------------------------------
        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            SecondaryButton {
                text: "Back"
                implicitHeight: 32
                Accessible.name: "Back to chats"
                onClicked: page.backRequested()
            }

            Text {
                Layout.fillWidth: true
                text: "Archived chats"
                color: BeyGramTheme.text
                font.pixelSize: 26
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }

            Text {
                text: archiveList.count
                color: BeyGramTheme.muted
                font.pixelSize: 11
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: BeyGramTheme.line
        }

        // --------------------------------------------------------------
        // List
        // --------------------------------------------------------------
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ListView {
                id: archiveList
                anchors.fill: parent
                clip: true
                spacing: 2
                model: page.model
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                delegate: Item {
                    id: row

                    // Roles are qint64 in ChatListModel; keep ids and dates as
                    // var so large values do not fail the delegate binding.
                    required property var chatId
                    required property string title
                    required property string preview
                    required property var lastDate
                    required property string initials
                    required property int accentIndex
                    required property int photoFileId
                    required property bool photoReady
                    required property int photoRevision

                    width: archiveList.width
                    height: 64
                    Accessible.role: Accessible.ListItem
                    Accessible.name: row.title

                    Rectangle {
                        anchors.fill: parent
                        anchors.leftMargin: 4
                        anchors.rightMargin: 4
                        radius: 10
                        color: hoverArea.containsMouse ? BeyGramTheme.surface2 : "transparent"
                        Behavior on color { ColorAnimation { duration: 120 } }
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        spacing: 12

                        Avatar {
                            size: 42
                            fileId: row.photoFileId
                            ready: row.photoReady
                            revision: row.photoRevision
                            initials: row.initials
                            tint: page.avatarColors[Math.abs(row.accentIndex)
                                                    % page.avatarColors.length]
                            ink: "white"
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            spacing: 2

                            Text {
                                Layout.fillWidth: true
                                text: row.title
                                color: BeyGramTheme.text
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                                maximumLineCount: 1
                            }

                            Text {
                                Layout.fillWidth: true
                                text: row.preview
                                color: BeyGramTheme.muted
                                font.pixelSize: 11
                                elide: Text.ElideRight
                                maximumLineCount: 1
                            }
                        }

                        Text {
                            Layout.alignment: Qt.AlignTop
                            Layout.topMargin: 4
                            text: page.timeLabel(row.lastDate)
                            color: BeyGramTheme.muted
                            font.pixelSize: 10
                        }
                    }

                    MouseArea {
                        id: hoverArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: page.chatSelected(row.chatId)
                    }
                }

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

            Column {
                anchors.centerIn: parent
                width: Math.max(0, Math.min(360, parent.width - 64))
                spacing: 6
                visible: !page.loading && archiveList.count === 0

                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    text: "No archived chats"
                    color: BeyGramTheme.text
                    font.pixelSize: 14
                    font.weight: Font.DemiBold
                }

                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    text: "Chats you archive will appear here."
                    color: BeyGramTheme.muted
                    font.pixelSize: 11
                    wrapMode: Text.WordWrap
                    lineHeight: 1.3
                }
            }
        }
    }
}
