// ChatFolderTabs.qml — horizontal folder tab strip for the chat list.
//
// Folder filtering is wired by the parent page: ChatsPage.qml owns
// `currentFolderId`, passes it in here, and reacts to folderSelected(folderId)
// by switching the chat list it shows. This component only renders the folders
// supplied by ChatFolderListModel (roles: folderId, title, icon, unreadCount,
// isMain); it never filters, fetches, or calls into C++ itself.

import QtQuick
import QtQuick.Controls
import BeyGram.Theme 1.0

Item {
    id: root

    property var model: null
    property var currentFolderId: 0

    signal folderSelected(var folderId)

    implicitHeight: 38

    ListView {
        id: list

        anchors.fill: parent
        orientation: ListView.Horizontal
        clip: true
        spacing: 6
        boundsBehavior: Flickable.StopAtBounds
        interactive: contentWidth > width
        model: root.model

        delegate: Item {
            id: tab

            readonly property int folderId: model && model.folderId !== undefined
                                            ? Number(model.folderId) : 0
            readonly property string folderTitle: model && model.title !== undefined
                                                  ? String(model.title) : ""
            readonly property int unread: model && model.unreadCount !== undefined
                                          ? Number(model.unreadCount) : 0
            readonly property bool current: tab.folderId === root.currentFolderId

            width: pill.width
            height: Math.max(list.height, 30)

            Rectangle {
                id: pill

                anchors.verticalCenter: parent.verticalCenter
                width: content.width + 24
                height: 30
                radius: 16
                color: tab.current ? BeyGramTheme.accentSoft : BeyGramTheme.surface2

                Accessible.role: Accessible.PageTab
                Accessible.name: tab.folderTitle
                Accessible.checked: tab.current

                Behavior on color { ColorAnimation { duration: 120 } }

                Item {
                    id: content

                    anchors.centerIn: parent
                    width: label.width + (badge.visible ? 6 + badge.width : 0)
                    height: 16

                    Text {
                        id: label

                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        width: Math.min(implicitWidth, 132)
                        text: tab.folderTitle
                        elide: Text.ElideRight
                        color: tab.current ? BeyGramTheme.accentInk : BeyGramTheme.muted
                        font.pixelSize: 12
                        font.weight: Font.DemiBold

                        Behavior on color { ColorAnimation { duration: 120 } }
                    }

                    Rectangle {
                        id: badge

                        anchors.left: label.right
                        anchors.leftMargin: 6
                        anchors.verticalCenter: parent.verticalCenter
                        visible: tab.unread > 0
                        width: Math.max(16, badgeLabel.implicitWidth + 8)
                        height: 16
                        radius: 8
                        color: tab.current ? BeyGramTheme.accentInk : BeyGramTheme.accent

                        Behavior on color { ColorAnimation { duration: 120 } }

                        Text {
                            id: badgeLabel

                            anchors.centerIn: parent
                            text: tab.unread > 99 ? "99+" : tab.unread
                            color: tab.current ? BeyGramTheme.accentSoft : BeyGramTheme.onAccent
                            font.pixelSize: 10
                            font.weight: Font.DemiBold
                        }
                    }
                }
            }

            MouseArea {
                anchors.fill: pill
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.folderSelected(tab.folderId)
            }
        }
    }
}
