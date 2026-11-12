// DsChip.qml — compact selectable chip with an optional remove affordance.
//
// The parent owns selection: clicked() is emitted on the chip body and the
// parent flips `selected`. When `removable` is true a small × appears on the
// right and emits removed(); clicks on the × never also emit clicked().

import QtQuick
import BeyGram.Theme 1.0
import "design.js" as Design

Item {
    id: root

    property string text: ""
    property bool selected: false
    property bool removable: false

    signal clicked()
    signal removed()

    readonly property bool _hovered: chipMouse.containsMouse || removeMouse.containsMouse
    readonly property color _removeHoverTint: {
        var base = root.selected ? BeyGramTheme.accentInk : BeyGramTheme.muted
        return Qt.rgba(base.r, base.g, base.b, Design.opacityPressed)
    }

    implicitWidth: contentRow.implicitWidth + Design.space3 * 2
    implicitHeight: 28
    opacity: enabled ? 1.0 : Design.opacityDisabled

    activeFocusOnTab: enabled

    Accessible.role: Accessible.CheckBox
    Accessible.name: root.text
    Accessible.checked: root.selected
    Accessible.onPressAction: root.activate()

    Keys.onSpacePressed: root.activate()
    Keys.onReturnPressed: root.activate()
    Keys.onEnterPressed: root.activate()

    function activate() {
        if (root.enabled)
            root.clicked()
    }

    function remove() {
        if (root.enabled)
            root.removed()
    }

    Behavior on opacity {
        NumberAnimation {
            duration: Design.motionFast
            easing.type: Easing.OutCubic
        }
    }

    Rectangle {
        id: background

        anchors.fill: parent
        radius: Design.radiusPill
        color: root.selected
               ? BeyGramTheme.accentSoft
               : (root._hovered ? BeyGramTheme.surface2 : "transparent")
        border.width: 1
        border.color: root.activeFocus
                      ? BeyGramTheme.accent
                      : (root.selected ? "transparent" : BeyGramTheme.line)

        Behavior on color {
            ColorAnimation {
                duration: Design.motionFast
                easing.type: Easing.OutCubic
            }
        }
    }

    MouseArea {
        id: chipMouse

        anchors.fill: parent
        enabled: root.enabled
        hoverEnabled: true
        cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor

        onClicked: {
            root.forceActiveFocus()
            root.activate()
        }
    }

    Row {
        id: contentRow

        anchors.centerIn: parent
        spacing: Design.space1

        Text {
            id: label

            anchors.verticalCenter: parent.verticalCenter
            text: root.text
            color: root.selected ? BeyGramTheme.accentInk : BeyGramTheme.muted
            font.pixelSize: Design.fontSmall
            font.weight: root.selected ? Font.DemiBold : Font.Normal

            Behavior on color {
                ColorAnimation {
                    duration: Design.motionFast
                    easing.type: Easing.OutCubic
                }
            }
        }

        Item {
            id: removeArea

            anchors.verticalCenter: parent.verticalCenter
            visible: root.removable
            width: 16
            height: 16

            Accessible.role: Accessible.Button
            Accessible.name: "Remove"

            Rectangle {
                anchors.centerIn: parent
                width: 14
                height: 14
                radius: width / 2
                color: removeMouse.containsMouse ? root._removeHoverTint : "transparent"

                Behavior on color {
                    ColorAnimation {
                        duration: Design.motionFast
                        easing.type: Easing.OutCubic
                    }
                }

                Text {
                    anchors.centerIn: parent
                    text: "\u00D7"
                    color: root.selected ? BeyGramTheme.accentInk : BeyGramTheme.muted
                    font.pixelSize: Design.fontBody
                    font.weight: Font.DemiBold
                }
            }

            MouseArea {
                id: removeMouse

                anchors.fill: parent
                enabled: root.enabled
                hoverEnabled: true
                cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor

                onClicked: root.remove()
            }
        }
    }
}
