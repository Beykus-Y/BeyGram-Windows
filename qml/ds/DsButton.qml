import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0
import "design.js" as Design

// DsButton — the single button primitive for the design system.
Item {
    id: root

    property alias text: button.text
    property string tone: "primary"   // primary | secondary | ghost | danger
    property string size: "md"        // sm | md | lg
    property string icon: ""          // optional Segoe MDL2 glyph
    property bool loading: false
    property string accessibleName: ""

    signal clicked()

    implicitWidth: button.implicitWidth
    implicitHeight: button.implicitHeight

    opacity: (enabled || loading) ? 1.0 : Design.opacityDisabled

    Behavior on opacity {
        NumberAnimation {
            duration: Design.motion(Design.motionFast, BeyGramAppearance.reduceMotion)
            easing.type: Easing.OutCubic
        }
    }

    HoverHandler {
        cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
    }

    Button {
        id: button
        anchors.fill: parent

        enabled: root.enabled && !root.loading
        hoverEnabled: true
        focusPolicy: Qt.StrongFocus

        onClicked: root.clicked()

        Accessible.role: Accessible.Button
        Accessible.name: root.accessibleName.length > 0 ? root.accessibleName : root.text
        Accessible.description: root.loading ? qsTr("Loading") : ""

        readonly property bool _small: root.size === "sm"
        readonly property bool _large: root.size === "lg"
        readonly property bool _filled: root.tone === "primary" || root.tone === "danger"

        readonly property int _fontSize: _small ? Design.fontSmall
                                                : (_large ? Design.fontBody : 13)
        readonly property int _glyphSize: _small ? Design.iconSize - 2
                                                 : (_large ? Design.iconSizeLarge : Design.iconSize)
        readonly property int _spinnerSize: _glyphSize

        readonly property color _ink: _filled ? BeyGramTheme.onAccent : BeyGramTheme.text

        readonly property color _fill: {
            if (root.tone === "primary")
                return BeyGramTheme.accent
            if (root.tone === "danger")
                return BeyGramTheme.danger
            if (root.tone === "secondary" && BeyGramTheme.modeName === "Amoled")
                return BeyGramTheme.surface
            if (root.tone === "ghost" && (hovered || pressed))
                return BeyGramTheme.surface2
            return "transparent"
        }

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
            radius: Math.min(Design.buttonRadius(BeyGramAppearance.designStyle), height / 2)
            color: button._fill
            border.width: root.tone === "secondary" ? 1 : 0
            border.color: BeyGramTheme.line
            antialiasing: true

            Behavior on color {
                ColorAnimation {
                    duration: Design.motion(Design.motionFast, BeyGramAppearance.reduceMotion)
                    easing.type: Easing.OutCubic
                }
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
                    NumberAnimation {
                        duration: Design.motion(Design.motionFast, BeyGramAppearance.reduceMotion)
                        easing.type: Easing.OutCubic
                    }
                }
            }

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
                    NumberAnimation {
                        duration: Design.motion(Design.motionFast, BeyGramAppearance.reduceMotion)
                        easing.type: Easing.OutCubic
                    }
                }
            }
        }
    }
}
