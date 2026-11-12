import QtQuick
import QtQuick.Layouts
import BeyGram.Theme 1.0
import "design.js" as Design

/// Conversation row for the chat list.
///
/// Follows Android's clean readable hierarchy (14-15 px DemiBold title,
/// 12 px preview, crisp time & unread badge, subtle accent selection bar),
/// and respects Comfortable vs Compact density plus reduced motion.
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

    readonly property bool compact: BeyGramAppearance.listDensity === "Compact"
    readonly property int rowHeightPx: compact ? Design.rowHeightCompact : Design.rowHeight

    implicitWidth: 320
    implicitHeight: rowHeightPx
    height: rowHeightPx

    Accessible.role: Accessible.ListItem
    Accessible.name: root.title
    Accessible.description: root.preview

    Rectangle {
        id: surface
        anchors.fill: parent
        radius: Design.rowRadius(BeyGramAppearance.designStyle)
        color: {
            if (root.selected) {
                return BeyGramAppearance.designStyle === "Signature"
                       ? Qt.alpha(BeyGramTheme.accent, BeyGramTheme.darkMode ? 0.16 : 0.12)
                       : BeyGramTheme.surface2
            }
            if (mouse.containsMouse)
                return Qt.alpha(BeyGramTheme.surface2, 0.72)
            return "transparent"
        }
        border.width: root.selected ? 1 : 0
        border.color: root.selected
                      ? Qt.alpha(BeyGramTheme.accent, BeyGramTheme.modeName === "Amoled" ? 0.45 : 0.24)
                      : "transparent"

        Behavior on color {
            ColorAnimation {
                duration: Design.motion(Design.motionFast, BeyGramAppearance.reduceMotion)
                easing.type: Easing.OutCubic
            }
        }

        // Active accent bar along the leading edge on desktop.
        Rectangle {
            visible: root.selected
            width: 3
            height: parent.height * 0.48
            radius: 2
            anchors.left: parent.left
            anchors.leftMargin: 3
            anchors.verticalCenter: parent.verticalCenter
            color: BeyGramTheme.accent
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
        anchors.leftMargin: Design.space3
        anchors.rightMargin: Design.space3
        anchors.topMargin: root.compact ? 6 : 9
        anchors.bottomMargin: root.compact ? 6 : 9
        spacing: Design.space3

        DsAvatar {
            size: root.compact ? 38 : Design.avatarMedium
            initials: root.initials
            accentIndex: root.accentIndex
            imageSource: root.imageSource
            online: root.online
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            spacing: root.compact ? 1 : 3

            RowLayout {
                Layout.fillWidth: true
                spacing: Design.space1

                Text {
                    Layout.fillWidth: true
                    text: root.title
                    color: BeyGramTheme.text
                    font.pixelSize: root.compact ? Design.fontBody : Design.fontBodyLarge
                    font.weight: root.unread > 0 || root.selected ? Font.DemiBold : Font.Medium
                    elide: Text.ElideRight
                }

                Text {
                    visible: root.muted
                    text: "\uE74F" // Mute icon in Segoe MDL2 Assets
                    color: BeyGramTheme.muted
                    font.family: "Segoe MDL2 Assets"
                    font.pixelSize: 11
                }
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
            Layout.alignment: Qt.AlignVCenter
            spacing: root.compact ? 3 : 5

            RowLayout {
                visible: root.pinned || root.timeText.length > 0
                Layout.alignment: Qt.AlignRight
                spacing: Design.space1

                Text {
                    visible: root.pinned && root.unread === 0
                    text: "\uE840" // Pin icon
                    color: BeyGramTheme.muted
                    font.family: "Segoe MDL2 Assets"
                    font.pixelSize: 10
                }

                Text {
                    visible: text.length > 0
                    text: root.timeText
                    color: root.unread > 0 && !root.muted ? BeyGramTheme.accentInk : BeyGramTheme.muted
                    font.pixelSize: Design.fontCaption
                    font.weight: root.unread > 0 ? Font.Medium : Font.Normal
                }
            }

            Rectangle {
                id: badge
                visible: root.unread > 0
                Layout.alignment: Qt.AlignRight
                implicitWidth: Math.max(20, badgeLabel.implicitWidth + 11)
                implicitHeight: 20
                radius: height / 2
                color: root.muted ? BeyGramTheme.surface2 : BeyGramTheme.accent
                border.width: root.muted ? 1 : 0
                border.color: BeyGramTheme.line

                Text {
                    id: badgeLabel
                    anchors.centerIn: parent
                    text: root.unread > 999 ? "999+" : root.unread
                    color: root.muted ? BeyGramTheme.muted : BeyGramTheme.onAccent
                    font.pixelSize: 10
                    font.weight: Font.DemiBold
                }
            }
        }
    }
}
