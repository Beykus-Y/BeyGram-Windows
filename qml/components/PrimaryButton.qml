import QtQuick
import QtQuick.Controls
import BeyGram.Theme 1.0

Button {
    id: control
    property string tone: "neutral" // neutral | ok | warn | danger | accent

    implicitHeight: 36
    leftPadding: 14
    rightPadding: 14
    hoverEnabled: true
    opacity: enabled ? 1.0 : 0.45
    Accessible.role: Accessible.Button
    Accessible.name: text

    contentItem: Text {
        text: control.text
        color: BeyGramTheme.onAccent
        font.pixelSize: 12
        font.weight: Font.DemiBold
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: 10
        color: {
            let base = BeyGramTheme.accent
            if (control.tone === "ok") base = BeyGramTheme.okInk
            if (control.tone === "warn") base = BeyGramTheme.warnInk
            if (control.tone === "danger") base = BeyGramTheme.danger
            return control.pressed ? Qt.darker(base, 1.15)
                                   : (control.hovered ? Qt.lighter(base, 1.06) : base)
        }
        Behavior on color { ColorAnimation { duration: 120 } }
    }
}
