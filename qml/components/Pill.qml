import QtQuick
import BeyGram.Theme 1.0

Rectangle {
    id: pill
    property string text: ""
    property string tone: "neutral" // neutral | ok | warn | danger | accent

    implicitWidth: label.implicitWidth + 20
    implicitHeight: 22
    radius: 11
    color: {
        if (tone === "ok") return Qt.alpha(BeyGramTheme.ok, BeyGramTheme.darkMode ? 0.9 : 1.0)
        if (tone === "warn") return Qt.alpha(BeyGramTheme.warn, BeyGramTheme.darkMode ? 0.9 : 1.0)
        if (tone === "danger") return Qt.alpha(BeyGramTheme.danger, 0.16)
        if (tone === "accent") return Qt.alpha(BeyGramTheme.accentSoft, 0.95)
        return Qt.alpha(BeyGramTheme.surface2, 0.9)
    }
    border.width: 1
    border.color: {
        if (tone === "danger") return Qt.alpha(BeyGramTheme.danger, 0.35)
        if (tone === "accent") return Qt.alpha(BeyGramTheme.accent, 0.24)
        return Qt.alpha(BeyGramTheme.line, 0.8)
    }

    Text {
        id: label
        anchors.centerIn: parent
        text: pill.text
        font.pixelSize: 10
        font.weight: Font.DemiBold
        font.letterSpacing: 0.4
        color: {
            if (pill.tone === "ok") return BeyGramTheme.okInk
            if (pill.tone === "warn") return BeyGramTheme.warnInk
            if (pill.tone === "danger") return BeyGramTheme.danger
            if (pill.tone === "accent") return BeyGramTheme.accentInk
            return BeyGramTheme.muted
        }
    }
}
