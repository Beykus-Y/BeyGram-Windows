pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0
import "../ds/design.js" as Design

// NavButton — compact desktop rail destination row used when the navigation
// rail is expanded. Preserves the (text / icon / selected / hint / clicked)
// contract while honoring DesignStyle, AMOLED contrast and reduceMotion.
Item {
    id: root

    property string text: ""
    property string icon: ""      // Segoe MDL2 Assets glyph, e.g. "\uE8BD"
    property bool selected: false
    property string hint: ""      // optional trailing shortcut / caption
    property int badge: 0         // optional unread badge count

    signal clicked()

    readonly property string _glyph: {
        if (root.icon.length > 0)
            return root.icon

        var key = root.text.toLowerCase()
        if (key.indexOf("chat") === 0)
            return "\uE8BD"
        if (key.indexOf("search") === 0)
            return "\uE721"
        if (key.indexOf("profile") === 0)
            return "\uE77B"
        if (key.indexOf("setting") === 0)
            return "\uE713"
        return ""
    }

    readonly property string _initial: root._glyph.length === 0 && root.text.length > 0
                                       ? root.text.charAt(0).toUpperCase() : ""

    readonly property color _ink: root.selected ? BeyGramTheme.accentInk
                                                : BeyGramTheme.text
    readonly property color _glyphInk: root.selected
                                       ? BeyGramTheme.accentInk
                                       : (button.hovered || button.pressed
                                          ? BeyGramTheme.text : BeyGramTheme.muted)
    readonly property int _radius: BeyGramAppearance.designStyle === "Material3"
                                   ? Design.radiusPill
                                   : Design.rowRadius(BeyGramAppearance.designStyle)
    readonly property int _animFast: Design.motion(Design.motionFast, BeyGramAppearance.reduceMotion)

    implicitWidth: 192
    implicitHeight: 42
    opacity: root.enabled ? 1.0 : Design.opacityDisabled

    Behavior on opacity {
        NumberAnimation { duration: root._animFast; easing.type: Easing.OutCubic }
    }

    AbstractButton {
        id: button
        anchors.fill: parent
        padding: 0
        hoverEnabled: true
        focusPolicy: Qt.StrongFocus

        Accessible.role: Accessible.Button
        Accessible.name: root.text
        Accessible.checked: root.selected

        onClicked: root.clicked()

        HoverHandler {
            cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        }

        contentItem: Item {
            implicitWidth: layout.implicitWidth
            implicitHeight: layout.implicitHeight

            RowLayout {
                id: layout
                anchors.fill: parent
                anchors.leftMargin: Design.space2
                anchors.rightMargin: Design.space3
                spacing: Design.space2 + 2

                Item {
                    Layout.preferredWidth: 32
                    Layout.preferredHeight: 32
                    Layout.alignment: Qt.AlignVCenter

                    Text {
                        anchors.centerIn: parent
                        visible: root._glyph.length > 0
                        text: root._glyph
                        color: root._glyphInk
                        font.family: "Segoe MDL2 Assets"
                        font.pixelSize: Design.iconSize
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        Accessible.ignored: true

                        Behavior on color {
                            ColorAnimation { duration: root._animFast; easing.type: Easing.OutCubic }
                        }
                    }

                    Text {
                        anchors.centerIn: parent
                        visible: root._initial.length > 0
                        text: root._initial
                        color: root._glyphInk
                        font.pixelSize: Design.fontBody
                        font.weight: Font.DemiBold
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        Accessible.ignored: true

                        Behavior on color {
                            ColorAnimation { duration: root._animFast; easing.type: Easing.OutCubic }
                        }
                    }
                }

                Text {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignVCenter
                    text: root.text
                    color: root._ink
                    font.pixelSize: 13
                    font.weight: root.selected ? Font.DemiBold : Font.Medium
                    elide: Text.ElideRight
                    Accessible.ignored: true

                    Behavior on color {
                        ColorAnimation { duration: root._animFast; easing.type: Easing.OutCubic }
                    }
                }

                Rectangle {
                    id: badgePill
                    Layout.alignment: Qt.AlignVCenter
                    visible: root.badge > 0
                    implicitWidth: Math.max(20, badgeText.implicitWidth + 10)
                    implicitHeight: 18
                    radius: height / 2
                    color: BeyGramTheme.accent
                    antialiasing: true

                    Text {
                        id: badgeText
                        anchors.centerIn: parent
                        text: root.badge > 99 ? "99+" : String(root.badge)
                        color: BeyGramTheme.onAccent
                        font.pixelSize: 10
                        font.weight: Font.DemiBold
                        Accessible.ignored: true
                    }
                }

                Text {
                    Layout.alignment: Qt.AlignVCenter
                    visible: root.hint.length > 0 && root.badge <= 0
                    text: root.hint
                    color: BeyGramTheme.muted
                    font.pixelSize: 10
                    elide: Text.ElideRight
                    Accessible.ignored: true
                }
            }
        }

        background: Rectangle {
            id: surface
            radius: root._radius
            color: root.selected
                   ? BeyGramTheme.accentSoft
                   : (button.hovered ? BeyGramTheme.surface2 : "transparent")
            border.width: root.selected && BeyGramTheme.modeName === "Amoled" ? 1 : 0
            border.color: Qt.alpha(BeyGramTheme.accent, 0.35)
            antialiasing: true

            Behavior on color {
                ColorAnimation { duration: root._animFast; easing.type: Easing.OutCubic }
            }

            Rectangle {
                anchors.fill: parent
                radius: surface.radius
                color: root._ink
                opacity: button.pressed ? Design.opacityPressed : 0
                antialiasing: true

                Behavior on opacity {
                    NumberAnimation { duration: root._animFast; easing.type: Easing.OutCubic }
                }
            }

            Rectangle {
                width: 3
                height: 18
                radius: 1.5
                x: 2
                anchors.verticalCenter: parent.verticalCenter
                visible: root.selected && BeyGramAppearance.designStyle !== "Material3"
                color: BeyGramTheme.accent
                antialiasing: true
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: -3
        radius: root._radius + 3
        color: "transparent"
        border.width: 2
        border.color: BeyGramTheme.accent
        visible: button.visualFocus && root.enabled
        antialiasing: true
        z: 2
    }
}
