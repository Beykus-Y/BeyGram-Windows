// DsSwitch.qml — pill switch with an optional label on the left.
//
// The track is 40 x 22 px and fully rounded; the knob is 18 px. The knob
// slides and the track/knob colours cross-fade on the shared fast motion
// scale. `checked` and `text` are the normal Switch properties.

import QtQuick
import QtQuick.Controls
import BeyGram.Theme 1.0
import "design.js" as Design

Switch {
    id: root

    property int trackWidth: 40
    property int trackHeight: 22
    property int knobSize: 18

    readonly property real knobInset: Math.max(0, (root.trackHeight - root.knobSize) / 2)

    hoverEnabled: true
    spacing: Design.space2
    leftPadding: 0
    rightPadding: 0
    topPadding: 0
    bottomPadding: 0
    implicitHeight: root.trackHeight
    implicitWidth: root.trackWidth
                   + (root.text.length > 0 ? root.contentItem.implicitWidth + root.spacing : 0)
    opacity: root.enabled ? 1.0 : Design.opacityDisabled
    activeFocusOnTab: true
    Accessible.name: root.text

    contentItem: Text {
        text: root.text
        visible: root.text.length > 0
        x: 0
        y: 0
        width: visible ? Math.max(0, root.width - root.indicator.width - root.spacing) : 0
        height: root.height
        color: BeyGramTheme.text
        font.pixelSize: 13
        font.weight: Font.Medium
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    indicator: Item {
        id: track

        x: root.width - width
        y: (root.height - height) / 2
        width: root.trackWidth
        height: root.trackHeight

        Rectangle {
            id: trackBackground

            anchors.fill: parent
            radius: Design.radiusPill
            color: root.checked ? BeyGramTheme.accent : BeyGramTheme.surface2
            border.width: root.visualFocus ? 2 : 1
            border.color: root.checked || root.visualFocus
                          ? BeyGramTheme.accent
                          : BeyGramTheme.line

            Behavior on color {
                ColorAnimation {
                    duration: Design.motionFast
                    easing.type: Easing.OutCubic
                }
            }

            Behavior on border.color {
                ColorAnimation {
                    duration: Design.motionFast
                    easing.type: Easing.OutCubic
                }
            }
        }

        Rectangle {
            id: knob

            width: root.knobSize
            height: root.knobSize
            radius: width / 2
            y: (parent.height - height) / 2
            x: root.checked ? parent.width - width - root.knobInset : root.knobInset
            color: root.checked ? BeyGramTheme.onAccent : BeyGramTheme.muted

            Behavior on x {
                NumberAnimation {
                    duration: Design.motionFast
                    easing.type: Easing.OutCubic
                }
            }

            Behavior on color {
                ColorAnimation {
                    duration: Design.motionFast
                    easing.type: Easing.OutCubic
                }
            }
        }
    }
}
