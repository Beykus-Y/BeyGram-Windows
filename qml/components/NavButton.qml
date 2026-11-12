import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0
import "../ds/design.js" as Design

// NavButton — a 44 px rail item: a 36 px icon slot, a 13 px label and an
// optional trailing hint.
//
// Qt marks AbstractButton.icon FINAL, so the string `icon` property lives on
// this Item wrapper instead of the inner AbstractButton (the same pattern as
// DsButton). When no icon is supplied, a glyph is derived from the label so
// the rail keeps its rhythm without every caller passing a code point; labels
// with no mapping fall back to their initial.
//
// selected: accentSoft pill + accentInk label + a 3 px accent bar on the left.
// hover:    surface2 fill.
Item {
    id: root

    property string text: ""
    property string icon: ""      // optional Segoe MDL2 Assets glyph, e.g. "\uE721"
    property bool selected: false
    property string hint: ""      // optional trailing caption

    signal clicked()

    // Segoe MDL2 Assets code points for the built-in rail labels.
    readonly property string _glyph: {
        if (root.icon.length > 0)
            return root.icon

        var key = root.text.toLowerCase()
        if (key.indexOf("chat") === 0)
            return "\uE8BD"   // Message
        if (key.indexOf("profile") === 0)
            return "\uE77B"   // Contact
        if (key.indexOf("setting") === 0)
            return "\uE713"   // Setting
        if (key.indexOf("network") === 0)
            return "\uE968"   // Network
        if (key.indexOf("log") === 0)
            return "\uE7C3"   // Page
        if (key.indexOf("search") === 0)
            return "\uE721"   // Search
        if (key.indexOf("tool") === 0)
            return "\uE90F"   // Repair
        if (key.indexOf("design") === 0)
            return "\uE790"   // Color
        return ""
    }

    // Unknown labels show their initial rather than an empty slot.
    readonly property string _initial: root._glyph.length === 0 && root.text.length > 0
                                       ? root.text.charAt(0).toUpperCase() : ""

    readonly property color _ink: root.selected ? BeyGramTheme.accentInk
                                                : BeyGramTheme.text
    readonly property color _glyphInk: root.selected
                                       ? BeyGramTheme.accentInk
                                       : (button.hovered || button.pressed
                                          ? BeyGramTheme.text : BeyGramTheme.muted)

    implicitWidth: 168
    implicitHeight: 44
    opacity: root.enabled ? 1.0 : Design.opacityDisabled

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
                spacing: Design.space3

                // 36 px icon slot, centred on the 44 px item.
                Item {
                    Layout.preferredWidth: 36
                    Layout.preferredHeight: 36
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
                            ColorAnimation { duration: Design.motionFast; easing.type: Easing.OutCubic }
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
                            ColorAnimation { duration: Design.motionFast; easing.type: Easing.OutCubic }
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
                        ColorAnimation { duration: Design.motionFast; easing.type: Easing.OutCubic }
                    }
                }

                Text {
                    Layout.alignment: Qt.AlignVCenter
                    visible: root.hint.length > 0
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
            radius: Design.radiusPill
            color: root.selected
                   ? BeyGramTheme.accentSoft
                   : (button.hovered ? BeyGramTheme.surface2 : "transparent")
            antialiasing: true

            Behavior on color {
                ColorAnimation { duration: Design.motionFast; easing.type: Easing.OutCubic }
            }

            // Pressed wash, so the pill responds before the click lands.
            Rectangle {
                anchors.fill: parent
                radius: surface.radius
                color: root._ink
                opacity: button.pressed ? Design.opacityPressed : 0
                antialiasing: true

                Behavior on opacity {
                    NumberAnimation { duration: Design.motionFast; easing.type: Easing.OutCubic }
                }
            }

            // 3 px accent bar on the left edge of the selected pill.
            Rectangle {
                width: 3
                height: 20
                radius: 1.5
                x: 0
                anchors.verticalCenter: parent.verticalCenter
                visible: root.selected
                color: BeyGramTheme.accent
                antialiasing: true
            }
        }
    }

    // 2 px halo just outside the pill, matching DsButton's focus treatment.
    Rectangle {
        anchors.fill: parent
        anchors.margins: -3
        radius: Design.radiusPill
        color: "transparent"
        border.width: 2
        border.color: BeyGramTheme.accent
        visible: button.visualFocus && root.enabled
        antialiasing: true
        z: 2
    }
}
