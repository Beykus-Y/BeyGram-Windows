import QtQuick
import BeyGram.Theme 1.0

/// Small muted "typing" line: optional label text plus three pulsing dots.
/// Purely presentational — it never invents presence. Callers drive `active`
/// from real typing updates and may supply a label through `text`.
Item {
    id: root

    property string text: ""
    property bool active: false

    readonly property bool _hasText: text !== ""
    readonly property int _dotSize: 4
    readonly property int _dotGap: 3
    readonly property int _labelGap: 5

    implicitWidth: _hasText
        ? label.implicitWidth + (active ? _labelGap + dots.implicitWidth : 0)
        : (active ? dots.implicitWidth : 0)
    implicitHeight: _hasText ? label.implicitHeight : (active ? 14 : 0)

    Text {
        id: label
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        visible: root._hasText
        text: root.text
        color: BeyGramTheme.muted
        font.pixelSize: 11
        font.weight: Font.Normal
        elide: Text.ElideRight
    }

    Row {
        id: dots
        anchors.left: label.right
        anchors.leftMargin: root._hasText ? root._labelGap : 0
        anchors.verticalCenter: parent.verticalCenter
        visible: root.active
        spacing: root._dotGap

        Repeater {
            model: 3
            delegate: Rectangle {
                required property int index

                width: root._dotSize
                height: root._dotSize
                radius: width / 2
                color: BeyGramTheme.muted
                opacity: 0.3

                SequentialAnimation on opacity {
                    running: root.active
                    loops: Animation.Infinite

                    PauseAnimation { duration: index * 150 }
                    NumberAnimation { to: 1.0; duration: 150; easing.type: Easing.InOutQuad }
                    PauseAnimation { duration: 150 }
                    NumberAnimation { to: 0.3; duration: 150; easing.type: Easing.InOutQuad }
                    PauseAnimation { duration: 450 - index * 150 }

                    onStopped: parent.opacity = 0.3
                }
            }
        }
    }
}
