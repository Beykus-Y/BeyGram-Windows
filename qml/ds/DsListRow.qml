import QtQuick
import QtQuick.Layouts
import BeyGram.Theme 1.0
import "design.js" as Design

/// Generic list row used by settings, search results and pickers.
Item {
    id: root

    property string title: ""
    property string subtitle: ""

    property alias leading: leadingSlot.data
    property alias trailing: trailingSlot.data

    property bool selected: false
    property int unread: 0
    property bool muted: false
    property bool hoverEnabled: true

    signal clicked()

    readonly property bool compact: BeyGramAppearance.listDensity === "Compact"
    readonly property int rowHeightPx: compact ? Design.rowHeightCompact : Design.rowHeight

    implicitWidth: 320
    implicitHeight: rowHeightPx
    height: rowHeightPx

    Accessible.role: Accessible.ListItem
    Accessible.name: root.title
    Accessible.description: root.subtitle

    Rectangle {
        id: surface
        anchors.fill: parent
        radius: Design.rowRadius(BeyGramAppearance.designStyle)
        color: root.selected
               ? BeyGramTheme.surface2
               : (root.hoverEnabled && mouse.containsMouse
                  ? Qt.alpha(BeyGramTheme.surface2, 0.70)
                  : "transparent")

        Behavior on color {
            ColorAnimation {
                duration: Design.motion(Design.motionFast, BeyGramAppearance.reduceMotion)
                easing.type: Easing.OutCubic
            }
        }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: root.hoverEnabled
        cursorShape: root.hoverEnabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: root.clicked()
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Design.space3
        anchors.rightMargin: Design.space3
        anchors.topMargin: root.compact ? 6 : 9
        anchors.bottomMargin: root.compact ? 6 : 9
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
