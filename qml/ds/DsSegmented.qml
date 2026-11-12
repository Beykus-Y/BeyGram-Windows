// DsSegmented.qml — segmented control with an animated selection rectangle.
//
// `model` is a plain list of strings and `currentIndex` selects one segment.
// Clicking updates `currentIndex` locally and emits activated(index); a parent
// that binds `currentIndex` can drive the control externally. Left/Right move
// the selection when the control has keyboard focus.

import QtQuick
import BeyGram.Theme 1.0
import "design.js" as Design

Item {
    id: root

    property var model: []
    property int currentIndex: 0
    property int padding: 2

    readonly property int segmentCount: {
        if (!model)
            return 0
        if (typeof model.length === "number")
            return model.length
        if (typeof model.count === "number")
            return model.count
        return 0
    }
    readonly property real segmentWidth: root.segmentCount > 0
                                         ? (root.width - root.padding * 2) / root.segmentCount
                                         : 0
    readonly property bool hasSelection: root.currentIndex >= 0
                                         && root.currentIndex < root.segmentCount

    signal activated(int index)

    implicitWidth: 240
    implicitHeight: Design.controlHeightSmall
    opacity: root.enabled ? 1.0 : Design.opacityDisabled
    activeFocusOnTab: true

    Keys.onLeftPressed: root.activateRelative(-1)
    Keys.onRightPressed: root.activateRelative(1)

    function activateRelative(delta) {
        if (root.segmentCount === 0)
            return

        var base = Math.max(0, Math.min(root.currentIndex, root.segmentCount - 1))
        var next = (base + delta + root.segmentCount) % root.segmentCount
        root.currentIndex = next
        root.activated(next)
    }

    Rectangle {
        anchors.fill: parent
        radius: Design.radiusSm
        color: BeyGramTheme.surface2
        border.width: root.visualFocus ? 1 : 0
        border.color: BeyGramTheme.accent
    }

    Rectangle {
        id: selection

        visible: root.hasSelection
        x: root.padding + Math.max(0, root.currentIndex) * root.segmentWidth
        y: root.padding
        width: root.segmentWidth
        height: root.height - root.padding * 2
        radius: Math.max(0, Design.radiusSm - root.padding)
        color: BeyGramTheme.surface
        border.width: 1
        border.color: BeyGramTheme.line

        Behavior on x {
            NumberAnimation {
                duration: Design.motionFast
                easing.type: Easing.OutCubic
            }
        }
    }

    Repeater {
        model: root.model

        delegate: Item {
            id: segment

            required property int index
            required property string modelData

            x: root.padding + segment.index * root.segmentWidth
            y: root.padding
            width: root.segmentWidth
            height: root.height - root.padding * 2

            Accessible.role: Accessible.RadioButton
            Accessible.name: segment.modelData
            Accessible.checked: root.currentIndex === segment.index

            Text {
                anchors.centerIn: parent
                width: Math.max(0, parent.width - Design.space2)
                text: segment.modelData
                color: root.currentIndex === segment.index
                       ? BeyGramTheme.text
                       : BeyGramTheme.muted
                font.pixelSize: Design.fontSmall
                font.weight: root.currentIndex === segment.index
                             ? Font.DemiBold
                             : Font.Medium
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight

                Behavior on color {
                    ColorAnimation {
                        duration: Design.motionFast
                        easing.type: Easing.OutCubic
                    }
                }
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor

                onClicked: {
                    root.forceActiveFocus()
                    root.currentIndex = segment.index
                    root.activated(segment.index)
                }
            }
        }
    }
}
