import QtQuick
import QtQuick.Layouts
import BeyGram.Theme 1.0
import "design.js" as Design

// Centred empty/placeholder block: a large muted glyph, a title, a description
// and an optional single action. Anchor it to fill the free area, or centre it
// with `anchors.centerIn: parent`.
Item {
    id: root

    property string glyph: ""
    property string title: ""
    property string description: ""
    property string actionText: ""

    signal actionRequested()

    implicitWidth: Design.space10 * 7 // 280: description measure
    implicitHeight: column.implicitHeight
    width: implicitWidth
    height: implicitHeight

    ColumnLayout {
        id: column
        anchors.centerIn: parent
        width: Math.min(Design.space10 * 7, root.width)
        spacing: Design.space3

        Text {
            Layout.fillWidth: true
            visible: root.glyph.length > 0
            text: root.glyph
            horizontalAlignment: Text.AlignHCenter
            color: BeyGramTheme.muted
            opacity: Design.opacitySecondary
            font.pixelSize: Design.fontDisplay + Design.space3 // 40
        }

        Text {
            Layout.fillWidth: true
            visible: root.title.length > 0
            text: root.title
            horizontalAlignment: Text.AlignHCenter
            color: BeyGramTheme.text
            font.pixelSize: Design.fontBodyLarge
            font.weight: Font.DemiBold
            lineHeight: Design.lineTight
            wrapMode: Text.WordWrap
        }

        Text {
            Layout.fillWidth: true
            visible: root.description.length > 0
            text: root.description
            horizontalAlignment: Text.AlignHCenter
            color: BeyGramTheme.muted
            font.pixelSize: Design.fontSmall
            lineHeight: Design.lineNormal
            wrapMode: Text.WordWrap
        }

        Rectangle {
            id: action
            Layout.alignment: Qt.AlignHCenter
            visible: root.actionText.length > 0
            implicitWidth: actionLabel.implicitWidth + Design.space5 * 2
            implicitHeight: Design.controlHeightSmall
            radius: Design.radiusPill
            color: actionArea.pressed ? Qt.darker(BeyGramTheme.accent, 1.12)
                                      : (actionArea.containsMouse ? Qt.lighter(BeyGramTheme.accent, 1.06)
                                                                  : BeyGramTheme.accent)
            Behavior on color { ColorAnimation { duration: Design.motionFast } }

            Accessible.role: Accessible.Button
            Accessible.name: root.actionText

            Text {
                id: actionLabel
                anchors.centerIn: parent
                text: root.actionText
                color: BeyGramTheme.onAccent
                font.pixelSize: Design.fontSmall
                font.weight: Font.DemiBold
            }

            MouseArea {
                id: actionArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.actionRequested()
            }
        }
    }
}
