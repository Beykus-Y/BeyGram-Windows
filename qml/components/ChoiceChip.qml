import QtQuick
import QtQuick.Controls
import BeyGram.Theme 1.0

Button {
    id: control

    property bool selected: false

    implicitHeight: 32
    leftPadding: 13
    rightPadding: 13
    hoverEnabled: true
    checkable: false
    Accessible.role: Accessible.RadioButton
    Accessible.name: text
    Accessible.checked: selected

    contentItem: Text {
        text: control.text
        color: control.selected ? BeyGramTheme.accentInk : BeyGramTheme.muted
        font.pixelSize: 12
        font.weight: control.selected ? Font.DemiBold : Font.Normal
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    background: Rectangle {
        radius: 16
        color: control.selected
               ? BeyGramTheme.accentSoft
               : (control.hovered ? BeyGramTheme.surface2 : "transparent")
        border.width: 1
        border.color: control.selected ? "transparent" : BeyGramTheme.line
        Behavior on color { ColorAnimation { duration: 120 } }
    }
}
