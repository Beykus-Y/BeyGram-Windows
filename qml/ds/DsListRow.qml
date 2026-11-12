import QtQuick
import QtQuick.Layouts
import BeyGram.Theme 1.0
import "design.js" as Design

/// Generic list row used by settings, search results and pickers.
///
/// The row owns only the shell: hover/selection surface, padding and the
/// title/subtitle stack. Anything visual goes into the `leading` and
/// `trailing` slots, which are aliases for the slot items' `data` property:
///
///     DsListRow {
///         title: "Privacy"
///         leading: DsAvatar { size: Design.avatarSmall; initials: "P" }
///         trailing: IconChevron { }
///     }
Item {
    id: root

    property string title: ""
    property string subtitle: ""

    /// Slot for a leading item (avatar, icon, ...).
    property alias leading: leadingSlot.data
    /// Slot for a trailing item (time label, chevron, switch, ...).
    property alias trailing: trailingSlot.data

    property bool selected: false
    /// Unread counter; 0 hides the badge.
    property int unread: 0
    property bool muted: false
    property bool hoverEnabled: true

    signal clicked()

    implicitWidth: 320
    implicitHeight: Design.rowHeight
    height: Design.rowHeight

    Accessible.role: Accessible.ListItem
    Accessible.name: root.title
    Accessible.description: root.subtitle

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

    // Declared before the content so interactive slot items stay on top of it.
    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: root.hoverEnabled
        cursorShape: root.hoverEnabled ? Qt.PointingHandCursor : Qt.ArrowCursor
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

        Item {
            id: leadingSlot
            Layout.alignment: Qt.AlignVCenter
            implicitWidth: childrenRect.width
            implicitHeight: childrenRect.height
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            spacing: 2

            Text {
                Layout.fillWidth: true
                visible: text.length > 0
                text: root.title
                color: root.muted ? BeyGramTheme.muted : BeyGramTheme.text
                font.pixelSize: Design.fontBody
                font.weight: Font.Medium
                elide: Text.ElideRight
            }

            Text {
                Layout.fillWidth: true
                visible: text.length > 0
                text: root.subtitle
                color: BeyGramTheme.muted
                font.pixelSize: Design.fontSmall
                elide: Text.ElideRight
            }
        }

        Rectangle {
            id: badge
            visible: root.unread > 0
            Layout.alignment: Qt.AlignVCenter
            implicitWidth: Math.max(18, badgeLabel.implicitWidth + 10)
            implicitHeight: 18
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

        Item {
            id: trailingSlot
            Layout.alignment: Qt.AlignVCenter
            implicitWidth: childrenRect.width
            implicitHeight: childrenRect.height
        }
    }
}
