// TypingIndicator — small muted "typing" line: optional label plus three
// pulsing accent dots.
//
// Purely presentational: it never invents presence. Callers drive `active`
// from real typing updates and may supply a label through `text`. When `active`
// is false the dots disappear, so the parent can keep the item in a layout
// without a placeholder gap.

import QtQuick
import QtQuick.Layouts
import BeyGram.Theme 1.0
import "../ds/design.js" as Design

Item {
    id: root

    property string text: ""
    property bool active: false

    readonly property bool _hasText: text !== ""
    readonly property int _dotSize: 5
    readonly property int _dotGap: 4
    readonly property int _labelGap: Design.space2

    implicitWidth: content.implicitWidth
    implicitHeight: content.implicitHeight

    RowLayout {
        id: content
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        spacing: root._hasText && root.active ? root._labelGap : 0

        Text {
            id: label
            Layout.alignment: Qt.AlignVCenter
            visible: root._hasText
            text: root.text
            color: BeyGramTheme.muted
            font.pixelSize: Design.fontSmall
            font.weight: Font.Normal
            elide: Text.ElideRight
            maximumLineCount: 1
            wrapMode: Text.NoWrap

            Accessible.role: Accessible.StaticText
            Accessible.name: root.text
        }

        // All three dots share one height, so the Row needs no alignment of its
        // own; the outer layout centres the group against the label.
        Row {
            id: dots
            Layout.alignment: Qt.AlignVCenter
            visible: root.active
            spacing: root._dotGap

            Repeater {
                model: 3

                delegate: Rectangle {
                    id: dot

                    required property int index

                    width: root._dotSize
                    height: root._dotSize
                    radius: width / 2
                    color: BeyGramTheme.accent
                    opacity: 0.35
                    antialiasing: true

                    SequentialAnimation {
                        running: root.active && !BeyGramAppearance.reduceMotion
                        loops: Animation.Infinite

                        PauseAnimation { duration: dot.index * 140 }
                        NumberAnimation {
                            target: dot
                            property: "opacity"
                            to: 1.0
                            duration: 180
                            easing.type: Easing.InOutQuad
                        }
                        PauseAnimation { duration: 120 }
                        NumberAnimation {
                            target: dot
                            property: "opacity"
                            to: 0.35
                            duration: 180
                            easing.type: Easing.InOutQuad
                        }
                        PauseAnimation { duration: 420 - dot.index * 140 }

                        onStopped: dot.opacity = root.active && BeyGramAppearance.reduceMotion ? 0.85 : 0.35
                    }
                }
            }
        }
    }
}
