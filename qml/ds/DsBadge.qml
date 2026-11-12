// DsBadge.qml — compact pill badge for counts, statuses and labels.
//
// The height is fixed at 18 px so badges align in dense rows. Tones resolve to
// BeyGramTheme colours only, so Warm/Graphite and Light/Dark/AMOLED all follow
// without local colour constants.

import QtQuick
import BeyGram.Theme 1.0
import "design.js" as Design

Rectangle {
    id: root

    property string text: ""
    property string tone: "muted" // accent | muted | danger | ok
    property bool dot: false

    readonly property color inkColor: {
        if (root.tone === "accent")
            return BeyGramTheme.accentInk
        if (root.tone === "danger")
            return BeyGramTheme.danger
        if (root.tone === "ok")
            return BeyGramTheme.okInk
        return BeyGramTheme.muted
    }

    readonly property color fillColor: {
        if (root.tone === "accent")
            return BeyGramTheme.accentSoft
        if (root.tone === "danger")
            return Qt.alpha(BeyGramTheme.danger, 0.14)
        if (root.tone === "ok")
            return BeyGramTheme.ok
        return Qt.alpha(BeyGramTheme.surface2, 0.90)
    }

    readonly property color borderColor: {
        if (root.tone === "accent")
            return Qt.alpha(BeyGramTheme.accent, 0.24)
        if (root.tone === "danger")
            return Qt.alpha(BeyGramTheme.danger, 0.35)
        if (root.tone === "ok")
            return Qt.alpha(BeyGramTheme.okInk, 0.24)
        return Qt.alpha(BeyGramTheme.line, 0.80)
    }

    readonly property color dotColor: {
        if (root.tone === "accent")
            return BeyGramTheme.accent
        if (root.tone === "danger")
            return BeyGramTheme.danger
        if (root.tone === "ok")
            return BeyGramTheme.okInk
        return BeyGramTheme.muted
    }

    implicitWidth: content.implicitWidth + 12 // 6 px padding on each side
    implicitHeight: 18
    radius: Design.radiusPill
    color: root.fillColor
    border.width: 1
    border.color: root.borderColor

    Accessible.role: Accessible.StaticText
    Accessible.name: root.text

    Row {
        id: content
        anchors.centerIn: parent
        spacing: Design.space1

        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            visible: root.dot
            width: 6
            height: 6
            radius: width / 2
            color: root.dotColor
            antialiasing: true
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: root.text
            color: root.inkColor
            font.pixelSize: 10
            font.weight: Font.DemiBold
            font.letterSpacing: 0.3
        }
    }
}
