import QtQuick
import BeyGram.Theme 1.0
import "design.js" as Design

// Transient bottom-centre pill. Place it over a page with `anchors.fill: parent`
// and a high `z`, then call `show("Saved", "ok")`.
Item {
    id: root

    property string text: ""
    property string tone: "info" // info | ok | danger
    property int hideDelay: 2500

    function show(message, toastTone) {
        root.text = message
        if (toastTone !== undefined && toastTone.length > 0)
            root.tone = toastTone
        pill.opacity = 1
        hideTimer.restart()
    }

    function hide() {
        pill.opacity = 0
        hideTimer.stop()
    }

    Timer {
        id: hideTimer
        interval: root.hideDelay
        onTriggered: pill.opacity = 0
    }

    Rectangle {
        id: pill
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Design.space6
        implicitWidth: label.implicitWidth + Design.space5 * 2
        width: Math.min(implicitWidth, root.width > 0 ? root.width - Design.space4 * 2
                                                      : implicitWidth)
        height: Design.controlHeight
        radius: Design.radiusPill
        color: Qt.alpha(toneColor, 0.97)
        border.width: 1
        border.color: root.tone === "info" ? Qt.alpha(BeyGramTheme.line, 0.95)
                                           : Qt.alpha(BeyGramTheme.text, 0.10)
        opacity: 0
        visible: opacity > 0
        transform: Translate { y: (1 - pill.opacity) * Design.space3 }

        readonly property color toneColor: root.tone === "ok" ? BeyGramTheme.ok
                                          : root.tone === "danger" ? BeyGramTheme.danger
                                          : BeyGramTheme.surface2

        Behavior on opacity {
            NumberAnimation { duration: Design.motionNormal; easing.type: Easing.OutCubic }
        }

        Accessible.role: Accessible.StaticText
        Accessible.name: label.text

        Text {
            id: label
            anchors.fill: parent
            anchors.leftMargin: Design.space5
            anchors.rightMargin: Design.space5
            text: root.text
            color: root.tone === "ok" ? BeyGramTheme.okInk
                 : root.tone === "danger" ? BeyGramTheme.onAccent
                 : BeyGramTheme.text
            font.pixelSize: Design.fontSmall
            font.weight: Font.DemiBold
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
    }
}
