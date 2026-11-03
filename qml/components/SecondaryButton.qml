import QtQuick
import QtQuick.Controls
import BeyGram.Theme 1.0

Button {
    id: control

    implicitHeight: 36
    leftPadding: 14
    rightPadding: 14
    hoverEnabled: true
    opacity: enabled ? 1.0 : 0.45
    Accessible.role: Accessible.Button
    Accessible.name: text

    contentItem: Text {
        text: control.text
        color: BeyGramTheme.text
        font.pixelSize: 12
        font.weight: Font.Medium
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: 10
        color: control.pressed
               ? Qt.alpha(BeyGramTheme.surface2, 1.0)
               : (control.hovered ? Qt.alpha(BeyGramTheme.surface2, 0.75) : "transparent")
        border.width: 1
        border.color: Qt.alpha(BeyGramTheme.line, 1.0)
        Behavior on color { ColorAnimation { duration: 120 } }
    }
}
