// ForwardDialog - modal chat picker for forwarding one or more messages.
//
// The parent owns the data layer and the action: it assigns `model` from
// ChatListModel (roles: chatId, title, initials, photoFileId, photoReady,
// photoRevision, accentIndex), sets `messageCount`, and performs the actual
// `forwardMessages` call once `chatSelected(chatId)` is emitted. This file
// only renders the picker. Filtering is local (title substring), it reads
// nothing but model roles, and it has no network, logging or C++ calls.

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0

Dialog {
    id: root

    property var model: null
    property int messageCount: 0

    signal chatSelected(var chatId)

    readonly property string dialogTitle: messageCount > 1
                                          ? "Forward " + messageCount + " messages"
                                          : "Forward message"

    // Trimmed search text; empty means "show every chat".
    readonly property string query: searchField.text.trim()

    // Same avatar tints as ChatRow so a chat keeps its colour everywhere.
    readonly property var avatarColors: [
        "#C2593F", "#5B4BC4", "#2F8A7B",
        "#A9731D", "#3A78C2", "#B0476E"
    ]

    function matches(title) {
        if (root.query.length === 0)
            return true
        const value = (title === undefined || title === null) ? "" : String(title)
        return value.toLowerCase().indexOf(root.query.toLowerCase()) >= 0
    }

    function choose(chatId) {
        root.chatSelected(chatId)
        root.close()
    }

    parent: Overlay.overlay
    x: parent ? Math.round((parent.width - width) / 2) : 0
    y: parent ? Math.round((parent.height - height) / 2) : 0
    width: 440
    padding: 0
    modal: true
    focus: true
    title: dialogTitle
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    onOpened: {
        searchField.text = ""
        chatList.positionViewAtBeginning()
        searchField.forceActiveFocus()
    }

    Overlay.modal: Rectangle {
        color: Qt.alpha("#000000", 0.35)
    }

    background: Rectangle {
        radius: 16
        color: BeyGramTheme.surface
        border.width: 1
        border.color: Qt.alpha(BeyGramTheme.line, 1.0)
    }

    header: Item {
        implicitHeight: 52

        Text {
            anchors.left: parent.left
            anchors.leftMargin: 18
            anchors.right: parent.right
            anchors.rightMargin: 18
            anchors.verticalCenter: parent.verticalCenter
            text: root.dialogTitle
            color: BeyGramTheme.text
            font.pixelSize: 15
            font.weight: Font.DemiBold
            elide: Text.ElideRight
        }
    }

    contentItem: ColumnLayout {
        spacing: 10
        implicitWidth: 440

        TextField {
            id: searchField
            Layout.fillWidth: true
            Layout.leftMargin: 18
            Layout.rightMargin: 18
            Layout.topMargin: 2
            Layout.preferredHeight: 38
            implicitHeight: 38
            leftPadding: 13
            rightPadding: 13
            placeholderText: "Search chats"
            placeholderTextColor: Qt.alpha(BeyGramTheme.muted, 0.75)
            color: BeyGramTheme.text
            font.pixelSize: 13
            selectByMouse: true
            selectionColor: BeyGramTheme.accent
            selectedTextColor: BeyGramTheme.onAccent
            onTextChanged: chatList.positionViewAtBeginning()
            Keys.onEscapePressed: (event) => {
                root.close()
                event.accepted = true
            }

            background: Rectangle {
                radius: 19
                color: BeyGramTheme.surface2
                border.width: 1
                border.color: searchField.activeFocus ? BeyGramTheme.accent
                                                      : Qt.alpha(BeyGramTheme.line, 0.9)
                Behavior on border.color { ColorAnimation { duration: 120 } }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.leftMargin: 6
            Layout.rightMargin: 6
            Layout.bottomMargin: 10
            Layout.preferredHeight: 300

            ListView {
                id: chatList
                anchors.fill: parent
                clip: true
                spacing: 0
                model: root.model
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                // Local title filtering: a delegate that does not match
                // collapses to zero height instead of being rebuilt, so the
                // picker works with any chat-list model without touching it.
                delegate: Item {
                    id: chatRow

                    required property var chatId
                    required property string title
                    required property string initials
                    required property int photoFileId
                    required property bool photoReady
                    required property int photoRevision
                    required property int accentIndex
                    required property int index

                    readonly property bool matched: root.matches(chatRow.title)

                    width: chatList.width
                    height: matched ? 48 : 0
                    visible: matched

                    Rectangle {
                        anchors.fill: parent
                        anchors.leftMargin: 4
                        anchors.rightMargin: 4
                        radius: 10
                        color: (rowMouse.containsMouse
                                || chatList.currentIndex === chatRow.index)
                               ? BeyGramTheme.surface2
                               : "transparent"
                        Behavior on color { ColorAnimation { duration: 120 } }
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        spacing: 10

                        Avatar {
                            size: 38
                            Layout.alignment: Qt.AlignVCenter
                            fileId: chatRow.photoFileId
                            ready: chatRow.photoReady
                            revision: chatRow.photoRevision
                            initials: chatRow.initials
                            tint: root.avatarColors[Math.abs(chatRow.accentIndex)
                                                    % root.avatarColors.length]
                            ink: "white"
                        }

                        Text {
                            Layout.fillWidth: true
                            text: chatRow.title
                            color: BeyGramTheme.text
                            font.pixelSize: 13
                            elide: Text.ElideRight
                            maximumLineCount: 1
                        }
                    }

                    MouseArea {
                        id: rowMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.choose(chatRow.chatId)
                    }
                }
            }

            Text {
                anchors.centerIn: parent
                width: parent.width - 40
                horizontalAlignment: Text.AlignHCenter
                visible: chatList.count === 0
                text: "No chats available"
                color: BeyGramTheme.muted
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }

            Text {
                anchors.centerIn: parent
                width: parent.width - 40
                horizontalAlignment: Text.AlignHCenter
                visible: chatList.count > 0 && chatList.contentHeight <= 0
                         && root.query.length > 0
                text: "No chats match your search"
                color: BeyGramTheme.muted
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }
        }
    }

    footer: Item {
        implicitHeight: 58

        Rectangle {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 1
            color: Qt.alpha(BeyGramTheme.line, 0.9)
        }

        RowLayout {
            anchors.right: parent.right
            anchors.rightMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8

            SecondaryButton {
                text: "Cancel"
                onClicked: root.close()
            }
        }
    }
}
