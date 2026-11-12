import QtQuick
import QtQuick.Layouts
import BeyGram.Theme 1.0
import "design.js" as Design

// Welcoming centered empty, loading-fallback, or error state with soft badge
// framing and primary + optional secondary actions.
Item {
    id: root

    property string glyph: ""
    property string title: ""
    property string description: ""
    property string actionText: ""
    property string secondaryActionText: ""

    signal actionRequested()
    signal secondaryActionRequested()

    implicitWidth: Design.space10 * 8 // 320
    implicitHeight: column.implicitHeight
    width: implicitWidth
    height: implicitHeight

    ColumnLayout {
        id: column
        anchors.centerIn: parent
        width: Math.min(Design.space10 * 8, Math.max(220, root.width - Design.space4))
        spacing: Design.space3

        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            visible: root.glyph.length > 0
            width: 68
            height: 68
            radius: 22
            color: BeyGramTheme.accentSoft
            border.width: 1
            border.color: Qt.alpha(BeyGramTheme.accent, 0.22)

            Text {
                anchors.centerIn: parent
                text: root.glyph
                horizontalAlignment: Text.AlignHCenter
                color: BeyGramTheme.accentInk
                font.pixelSize: 28
            }
        }

        Text {
            Layout.fillWidth: true
            visible: root.title.length > 0
            text: root.title
            horizontalAlignment: Text.AlignHCenter
            color: BeyGramTheme.text
            font.pixelSize: Design.fontTitle
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
            font.pixelSize: Design.fontBody
            lineHeight: Design.lineNormal
            wrapMode: Text.WordWrap
        }

        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: Design.space1
            spacing: Design.space2
            visible: root.actionText.length > 0 || root.secondaryActionText.length > 0

            DsButton {
                visible: root.actionText.length > 0
                text: root.actionText
                tone: "primary"
                size: "sm"
                onClicked: root.actionRequested()
            }

            DsButton {
                visible: root.secondaryActionText.length > 0
                text: root.secondaryActionText
                tone: "secondary"
                size: "sm"
                onClicked: root.secondaryActionRequested()
            }
        }
    }
}
