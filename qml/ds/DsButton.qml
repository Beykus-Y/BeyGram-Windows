import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0
import "design.js" as Design

// DsButton — the single button primitive for the design system.
//
// Qt marks AbstractButton.icon FINAL, so a Button subclass cannot declare the
// string `icon` property this API needs ("Cannot override FINAL property").
// DsButton is therefore a thin Item wrapper around a real Button: interaction,
// focus, keyboard handling and accessibility stay native, while the properties
// below are the design-system contract.
//
// tone:    primary (accent fill) | secondary (line border) | ghost (text only)
//          | danger (danger fill)
// size:    sm (32 px) | md (40 px) | lg (48 px)
// icon:    optional glyph, e.g. a Segoe MDL2 Assets code point ("\uE710").
// loading: swaps the icon for a spinner and blocks activation.
// text:    label, forwarded to the inner Button.
// accessibleName: spoken label; defaults to `text` (set it for icon-only use).
Item {
    id: root

    property alias text: button.text
    property string tone: "primary"   // primary | secondary | ghost | danger
    property string size: "md"        // sm | md | lg
    property string icon: ""          // optional glyph, drawn before the label
    property bool loading: false
    // Spoken label; defaults to `text`. Set it for icon-only buttons.
    property string accessibleName: ""

    signal clicked()

    implicitWidth: button.implicitWidth
    implicitHeight: button.implicitHeight

    // A loading button stays fully opaque; only a real disabled state dims.
    opacity: (enabled || loading) ? 1.0 : Design.opacityDisabled

    Behavior on opacity {
        NumberAnimation { duration: Design.motionFast; easing.type: Easing.OutCubic }
    }

    HoverHandler {
        cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
    }

    Button {
        id: button
        anchors.fill: parent

        // `loading` also drives enabled, so an action cannot fire twice.
        enabled: root.enabled && !root.loading
        hoverEnabled: true
        focusPolicy: Qt.StrongFocus

        onClicked: root.clicked()

        Accessible.role: Accessible.Button
        Accessible.name: root.accessibleName.length > 0 ? root.accessibleName : root.text
        Accessible.description: root.loading ? "Loading" : ""

        readonly property bool _small: root.size === "sm"
        readonly property bool _large: root.size === "lg"
        readonly property bool _filled: root.tone === "primary" || root.tone === "danger"

        // 12 / 13 / 14 px labels; md sits between fontSmall and fontBody.
        readonly property int _fontSize: _small ? Design.fontSmall
                                                : (_large ? Design.fontBody : 13)
        // Glyphs follow control.iconSize: 16 / 18 / 22 px. The spinner uses the
        // same slot, so swapping icon → spinner never changes the button width.
        readonly property int _glyphSize: _small ? Design.iconSize - 2
                                                 : (_large ? Design.iconSizeLarge : Design.iconSize)
        readonly property int _spinnerSize: _glyphSize

        readonly property color _ink: _filled ? BeyGramTheme.onAccent : BeyGramTheme.text

        readonly property color _fill: {
            if (root.tone === "primary")
                return BeyGramTheme.accent
            if (root.tone === "danger")
                return BeyGramTheme.danger
            if (root.tone === "ghost" && (hovered || pressed))
                return BeyGramTheme.surface2
            return "transparent"
        }

        // Hover/pressed wash: lighten filled buttons, tint text buttons.
        readonly property color _stateTint: _filled ? BeyGramTheme.onAccent : BeyGramTheme.text

        implicitHeight: _small ? Design.controlHeightSmall
                               : (_large ? Design.controlHeightLarge : Design.controlHeight)
        leftPadding: _small ? Design.space3 : (_large ? Design.space5 : Design.space4)
        rightPadding: leftPadding
        topPadding: 0
        bottomPadding: 0

        contentItem: Item {
            implicitWidth: layout.implicitWidth
            implicitHeight: layout.implicitHeight

            RowLayout {
                id: layout
                anchors.centerIn: parent
                width: Math.min(implicitWidth, button.availableWidth)
                spacing: (glyph.visible || spinner.visible) && label.visible ? Design.space2 : 0

                Text {
                    id: glyph
                    visible: root.icon.length > 0 && !root.loading
                    text: root.icon
                    color: button._ink
                    font.family: "Segoe MDL2 Assets"
                    font.pixelSize: button._glyphSize
                    Layout.alignment: Qt.AlignVCenter
                }

                Item {
                    id: spinner
                    visible: root.loading
                    Layout.preferredWidth: button._spinnerSize
                    Layout.preferredHeight: button._spinnerSize
                    Layout.alignment: Qt.AlignVCenter

                    Item {
                        id: spinnerArm
                        anchors.fill: parent

                        // Clip half of the ring so it reads as a rotating arc.
                        Item {
                            width: spinner.width / 2
                            height: spinner.height
                            clip: true

                            Rectangle {
                                width: spinner.width
                                height: spinner.height
                                radius: width / 2
                                color: "transparent"
                                border.width: 2
                                border.color: button._ink
                                antialiasing: true
                            }
                        }

                        RotationAnimator on rotation {
                            from: 0
                            to: 360
                            duration: 900
                            loops: Animation.Infinite
                            running: spinner.visible
                        }
                    }
                }

                Text {
                    id: label
                    visible: button.text.length > 0
                    text: button.text
                    color: button._ink
                    font.pixelSize: button._fontSize
                    font.weight: Font.Medium
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                    Layout.maximumWidth: implicitWidth
                    Layout.alignment: Qt.AlignVCenter
                }
            }
        }

        background: Rectangle {
            id: surface
            radius: Design.radiusSm
            color: button._fill
            border.width: root.tone === "secondary" ? 1 : 0
            border.color: BeyGramTheme.line
            antialiasing: true

            Behavior on color {
                ColorAnimation { duration: Design.motionFast; easing.type: Easing.OutCubic }
            }

            Rectangle {
                id: stateLayer
                anchors.fill: parent
                radius: surface.radius
                color: button._stateTint
                opacity: button.pressed ? Design.opacityPressed
                         : button.hovered ? Design.opacityHover
                         : 0
                antialiasing: true

                Behavior on opacity {
                    NumberAnimation { duration: Design.motionFast; easing.type: Easing.OutCubic }
                }
            }

            // 2 px halo with a 2 px offset, so it reads on filled tones too.
            Rectangle {
                anchors.fill: parent
                anchors.margins: -4
                radius: surface.radius + 4
                color: "transparent"
                border.width: 2
                border.color: BeyGramTheme.accent
                opacity: button.activeFocus && button.enabled ? 1 : 0
                visible: opacity > 0
                antialiasing: true
                z: -1

                Behavior on opacity {
                    NumberAnimation { duration: Design.motionFast; easing.type: Easing.OutCubic }
                }
            }
        }
    }
}
