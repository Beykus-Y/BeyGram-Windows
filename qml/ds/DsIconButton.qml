import QtQuick
import QtQuick.Controls
import BeyGram.Theme 1.0
import "design.js" as Design

// DsIconButton — a square icon action with a pill-shaped hover wash.
//
// tone:  plain (transparent) | soft (surface fill) | accent (accent tint)
//        | danger (danger ink)
// size:  sm (28 px) | md (36 px) | lg (44 px)
// badge: a count in the top-right corner; 0 hides it.
//
// AbstractButton.icon is FINAL in Qt, so a string glyph cannot be declared as
// an `icon` property on an AbstractButton-derived type. This wrapper exposes
// the requested `icon` API; the inner AbstractButton owns focus, hover,
// pressed state, clicks and accessibility.
Item {
    id: control

    property string icon: ""
    property string tone: "plain" // plain | soft | accent | danger
    property string size: "md"    // sm | md | lg
    property bool selected: false
    property string tooltip: ""
    property int badge: 0

    signal clicked()

    readonly property bool _small: control.size === "sm"
    readonly property bool _large: control.size === "lg"
    readonly property int _diameter: _small ? 28 : (_large ? 44 : 36)
    readonly property int _glyphSize: _small ? 14
                                             : (_large ? Design.iconSizeLarge : Design.iconSize)

    readonly property bool _accented: control.selected || control.tone === "accent"
    readonly property color _fill: {
        if (_accented)
            return Qt.alpha(BeyGramTheme.accentSoft, 0.95)
        if (control.tone === "soft")
            return Qt.alpha(BeyGramTheme.surface2, 0.9)
        return "transparent"
    }
    readonly property color _wash: {
        if (_accented)
            return BeyGramTheme.accentInk
        if (control.tone === "danger")
            return BeyGramTheme.danger
        return BeyGramTheme.text
    }
    readonly property color _ink: {
        if (_accented)
            return BeyGramTheme.accentInk
        if (control.tone === "danger")
            return BeyGramTheme.danger
        if (control.tone === "soft")
            return BeyGramTheme.text
        return (button.hovered || button.pressed) ? BeyGramTheme.text : BeyGramTheme.muted
    }
    readonly property bool _bordered: _accented || control.tone === "soft"
    readonly property color _border: {
        if (_accented)
            return Qt.alpha(BeyGramTheme.accent, 0.24)
        if (control.tone === "soft")
            return Qt.alpha(BeyGramTheme.line, 0.8)
        return "transparent"
    }

    implicitWidth: _diameter
    implicitHeight: _diameter
    opacity: enabled ? 1.0 : Design.opacityDisabled

    Behavior on opacity {
        NumberAnimation { duration: Design.motionFast; easing.type: Easing.OutCubic }
    }

    AbstractButton {
        id: button
        anchors.fill: parent
        padding: 0
        hoverEnabled: true
        focusPolicy: Qt.StrongFocus

        Accessible.role: Accessible.Button
        Accessible.name: control.tooltip !== "" ? control.tooltip : control.icon
        Accessible.checked: control.selected

        onClicked: control.clicked()

        HoverHandler {
            cursorShape: control.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        }

        background: Rectangle {
            radius: Design.radiusPill
            color: control._fill
            border.width: control._bordered ? 1 : 0
            border.color: control._border
            antialiasing: true

            Rectangle {
                anchors.fill: parent
                radius: parent.radius
                color: control._wash
                opacity: !control.enabled ? 0
                         : button.pressed ? Design.opacityPressed
                         : button.hovered ? Design.opacityHover
                         : 0
                antialiasing: true

                Behavior on opacity {
                    NumberAnimation {
                        duration: Design.motionFast
                        easing.type: Easing.OutCubic
                    }
                }
                Behavior on color {
                    ColorAnimation { duration: Design.motionFast; easing.type: Easing.OutCubic }
                }
            }

            Behavior on color {
                ColorAnimation { duration: Design.motionFast; easing.type: Easing.OutCubic }
            }
        }

        // A direct child keeps the glyph centred without depending on the
        // contentItem being resized by the control.
        DsIcon {
            anchors.centerIn: parent
            z: 1
            name: control.icon
            size: control._glyphSize
            color: control._ink
            filled: control.tone !== "plain" || control.selected
                    || button.hovered || button.pressed
        }

        ToolTip.visible: hovered && control.tooltip !== ""
        ToolTip.text: control.tooltip
        ToolTip.delay: 600
    }

    // 2 px halo just outside the fill, matching DsButton's focus treatment.
    Rectangle {
        anchors.fill: parent
        anchors.margins: -3
        radius: Design.radiusPill
        color: "transparent"
        border.width: 2
        border.color: BeyGramTheme.accent
        visible: button.visualFocus && control.enabled
        antialiasing: true
        z: 2
    }

    Rectangle {
        visible: control.badge > 0
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: -Design.space1
        anchors.rightMargin: -Design.space1
        width: Math.max(Design.space4, badgeLabel.implicitWidth + Design.space2)
        height: Design.space4
        radius: Design.radiusPill
        color: BeyGramTheme.danger
        antialiasing: true
        z: 3

        Text {
            id: badgeLabel
            anchors.centerIn: parent
            text: control.badge > 99 ? "99+" : control.badge.toString()
            color: BeyGramTheme.onAccent
            font.pixelSize: 9
            font.weight: Font.Bold
            Accessible.ignored: true
        }
    }
}
