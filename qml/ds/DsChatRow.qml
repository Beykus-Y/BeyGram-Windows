import QtQuick
import QtQuick.Layouts
import BeyGram.Theme 1.0
import "design.js" as Design

/// Conversation row for the chat list.
///
/// Presentation only: the parent supplies the already formatted preview and
/// time label, and performs navigation in `clicked()`. Geometry matches
/// DsListRow (72 px tall, 12/10 padding, radiusMd surface).
Item {
    id: root

    property string title: ""
    property string preview: ""
    property string timeText: ""
    property int unread: 0
    property bool pinned: false
    property bool muted: false
    property bool online: false

    property string initials: "?"
    property int accentIndex: 0
    property string imageSource: ""

    property bool selected: false

    signal clicked()

    implicitWidth: 320
    implicitHeight: Design.rowHeight
    height: Design.rowHeight

    Accessible.role: Accessible.ListItem
    Accessible.name: root.title
    Accessible.description: root.preview

    Rectangle {
        id: surface
        anchors.fill: parent
        radius: Design.radiusMd
        color: root.selected
               ? BeyGramTheme.surface2
               : (mouse.containsMouse
                  ? Qt.alpha(BeyGramTheme.surface2, 0.6)
                  : "transparent")

        Behavior on color {
            ColorAnimation {
                duration: Design.motionFast
                easing.type: Easing.OutCubic
            }
        }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }

    RowLayout {
        anchors.fill: parent
        // 12 px horizontal, 10 px vertical padding.
        anchors.leftMargin: Design.space3
        anchors.rightMargin: Design.space3
        anchors.topMargin: 10
        anchors.bottomMargin: 10
        spacing: Design.space3

        DsAvatar {
            size: Design.avatarMedium
            initials: root.initials
            accentIndex: root.accentIndex
            imageSource: root.imageSource
            online: root.online
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            spacing: 2

            Text {
                Layout.fillWidth: true
                text: root.title
                color: root.muted ? BeyGramTheme.muted : BeyGramTheme.text
                font.pixelSize: Design.fontBody
                font.weight: Font.Medium
                elide: Text.ElideRight
            }

            Text {
                Layout.fillWidth: true
                text: root.preview
                color: BeyGramTheme.muted
                font.pixelSize: Design.fontSmall
                elide: Text.ElideRight
            }
        }

        ColumnLayout {
            visible: root.pinned || root.timeText.length > 0 || root.unread > 0
            Layout.alignment: Qt.AlignTop
            spacing: 4

            RowLayout {
                visible: root.pinned || root.timeText.length > 0
                Layout.alignment: Qt.AlignRight
                spacing: Design.space1

                Text {
                    visible: root.pinned
                    text: "\uD83D\uDCCC"
                    font.pixelSize: 10
                }

                Text {
                    visible: text.length > 0
                    text: root.timeText
                    color: root.unread > 0 ? BeyGramTheme.accentInk : BeyGramTheme.muted
                    font.pixelSize: Design.fontCaption
                }
            }

            Rectangle {
                id: badge
                visible: root.unread > 0
                Layout.alignment: Qt.AlignRight
                implicitWidth: Math.max(20, badgeLabel.implicitWidth + 10)
                implicitHeight: 20
                radius: height / 2
                color: BeyGramTheme.accent

                Text {
                    id: badgeLabel
                    anchors.centerIn: parent
                    text: root.unread > 99 ? "99+" : root.unread
                    color: BeyGramTheme.onAccent
                    font.pixelSize: 10
                    font.weight: Font.DemiBold
                }
            }
        }
    }
}
