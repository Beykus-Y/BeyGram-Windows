import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0

Button {
    id: control
    property bool selected: false
    property string hint: ""

    implicitHeight: 40
    padding: 0
    leftPadding: 12
    rightPadding: 12
    hoverEnabled: true
    opacity: enabled ? 1.0 : 0.45
    Accessible.role: Accessible.Button
    Accessible.name: text

    contentItem: RowLayout {
        spacing: 8

        Text {
            text: control.text
            color: control.selected ? BeyGramTheme.accentInk : BeyGramTheme.text
            font.pixelSize: 13
            font.weight: control.selected ? Font.DemiBold : Font.Normal
            elide: Text.ElideRight
            Layout.fillWidth: true
        }

        Text {
            visible: control.hint !== ""
            text: control.hint
            color: BeyGramTheme.muted
            font.pixelSize: 10
        }
    }

    background: Rectangle {
        radius: 10
        color: control.selected
               ? Qt.alpha(BeyGramTheme.accentSoft, 0.95)
               : (control.hovered && control.enabled
                  ? Qt.alpha(BeyGramTheme.surface2, 0.85)
                  : "transparent")
        border.width: control.selected ? 1 : 0
        border.color: Qt.alpha(BeyGramTheme.accent, 0.22)

        Behavior on color { ColorAnimation { duration: 120 } }
    }
}
