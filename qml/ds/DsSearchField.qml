import QtQuick
import QtQuick.Controls
import BeyGram.Theme 1.0
import "design.js" as Design

// Pill-shaped search input for the BeyGram design system.
//
// The magnifier is drawn from primitives, so the component does not depend on
// an icon font. `accepted` is inherited from TextField, which makes
// `onAccepted:` a direct passthrough. Clearing through the trailing button
// empties the text and emits `cleared()`.
TextField {
    id: control

    /// Emitted when the clear button empties the query.
    signal cleared()

    /// Empties the query, keeps focus and emits `cleared()`.
    function clearText() {
        control.text = ""
        control.forceActiveFocus()
        control.cleared()
    }

    readonly property bool _hasClear: (control.text || "").length > 0 && !control.readOnly
    readonly property color _iconColor: control.activeFocus ? BeyGramTheme.accent : BeyGramTheme.muted

    implicitWidth: 260
    implicitHeight: 40

    leftPadding: 40
    rightPadding: _hasClear ? 38 : Design.space4
    verticalAlignment: TextInput.AlignVCenter
    selectByMouse: true
    color: BeyGramTheme.text
    placeholderTextColor: Qt.alpha(BeyGramTheme.muted, 0.75)
    selectionColor: BeyGramTheme.accent
    selectedTextColor: BeyGramTheme.onAccent
    font.pixelSize: Design.fontBody

    // --- Pill + focus ring ---------------------------------------------------
    background: Rectangle {
        radius: Design.radiusPill
        color: BeyGramTheme.surface2
        border.width: control.activeFocus ? 2 : 1
        border.color: control.activeFocus ? BeyGramTheme.accent : BeyGramTheme.line
        Behavior on border.color { ColorAnimation { duration: Design.motionFast } }
        Behavior on border.width { NumberAnimation { duration: Design.motionFast } }
    }

    // --- Magnifier -----------------------------------------------------------
    Item {
        id: magnifier
        z: 2
        width: 18
        height: 18
        anchors.left: parent.left
        anchors.leftMargin: Design.space3 + 2
        anchors.verticalCenter: parent.verticalCenter
        opacity: control.enabled ? 1 : Design.opacityDisabled

        Rectangle {
            x: 1
            y: 1
            width: 11
            height: 11
            radius: width / 2
            color: "transparent"
            border.width: 1.5
            border.color: control._iconColor
            antialiasing: true
            Behavior on border.color { ColorAnimation { duration: Design.motionFast } }
        }

        Rectangle {
            x: 9.5
            y: 10.5
            width: 6.5
            height: 1.5
            radius: height / 2
            color: control._iconColor
            rotation: 45
            transformOrigin: Item.Left
            antialiasing: true
            Behavior on color { ColorAnimation { duration: Design.motionFast } }
        }
    }

    // --- Clear button --------------------------------------------------------
    Item {
        id: clearButton
        z: 2
        width: 22
        height: 22
        anchors.right: parent.right
        anchors.rightMargin: 10
        anchors.verticalCenter: parent.verticalCenter
        visible: control._hasClear

        Rectangle {
            anchors.fill: parent
            radius: width / 2
            color: clearMouse.containsMouse
                   ? Qt.alpha(BeyGramTheme.muted, Design.opacityHover)
                   : "transparent"
            Behavior on color { ColorAnimation { duration: Design.motionFast } }
        }

        Text {
            anchors.centerIn: parent
            text: "×"
            color: control._iconColor
            font.pixelSize: Design.fontBody + 2
        }

        MouseArea {
            id: clearMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: control.clearText()
        }
    }
}
