// DsTabs.qml — horizontal design-system tab strip.
//
// `model` is a plain list of labels. Clicking a tab updates `currentIndex`
// locally (like TabBar) and emits activated(index); a parent that binds
// `currentIndex` can drive the strip externally and update its own state from
// onActivated. The 2 px accent underline slides between tabs with the shared
// motion scale.

import QtQuick
import BeyGram.Theme 1.0
import "design.js" as Design

Item {
    id: root

    property var model: []
    property int currentIndex: 0

    signal activated(int index)

    implicitWidth: tabsRow.implicitWidth
    implicitHeight: Design.controlHeightSmall + 2
    opacity: enabled ? 1.0 : Design.opacityDisabled

    activeFocusOnTab: enabled

    Behavior on opacity {
        NumberAnimation {
            duration: Design.motionFast
            easing.type: Easing.OutCubic
        }
    }

    Keys.onLeftPressed: root.activateRelative(-1)
    Keys.onRightPressed: root.activateRelative(1)

    function activateRelative(delta) {
        if (!root.enabled || repeater.count === 0)
            return

        var base = Math.max(0, Math.min(root.currentIndex, repeater.count - 1))
        var next = (base + delta + repeater.count) % repeater.count
        root.currentIndex = next
        root.activated(next)
    }

    Flickable {
        id: flick

        anchors.fill: parent
        contentWidth: tabsRow.implicitWidth
        contentHeight: flick.height
        flickableDirection: Flickable.HorizontalFlick
        boundsBehavior: Flickable.StopAtBounds
        interactive: contentWidth > width
        clip: true

        Row {
            id: tabsRow

            spacing: Design.space1

            Repeater {
                id: repeater

                model: root.model

                delegate: Item {
                    id: tabItem

                    required property int index
                    required property var modelData

                    readonly property bool current: root.currentIndex === tabItem.index
                    readonly property bool hovered: tabMouse.containsMouse

                    width: pill.width
                    height: root.height

                    Rectangle {
                        id: pill

                        width: tabLabel.implicitWidth + Design.space3 * 2
                        height: Design.controlHeightSmall
                        radius: Design.radiusPill
                        color: tabItem.current
                               ? BeyGramTheme.accentSoft
                               : (tabItem.hovered ? BeyGramTheme.surface2 : "transparent")
                        border.width: root.activeFocus && tabItem.current ? 1 : 0
                        border.color: BeyGramTheme.accent

                        Accessible.role: Accessible.PageTab
                        Accessible.name: tabItem.modelData
                        Accessible.checked: tabItem.current

                        Behavior on color {
                            ColorAnimation {
                                duration: Design.motionFast
                                easing.type: Easing.OutCubic
                            }
                        }

                        Text {
                            id: tabLabel

                            anchors.centerIn: parent
                            text: tabItem.modelData
                            color: tabItem.current ? BeyGramTheme.accentInk : BeyGramTheme.muted
                            font.pixelSize: Design.fontSmall
                            font.weight: tabItem.current ? Font.DemiBold : Font.Medium

                            Behavior on color {
                                ColorAnimation {
                                    duration: Design.motionFast
                                    easing.type: Easing.OutCubic
                                }
                            }
                        }
                    }

                    MouseArea {
                        id: tabMouse

                        anchors.fill: pill
                        enabled: root.enabled
                        hoverEnabled: true
                        cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor

                        onClicked: {
                            root.forceActiveFocus()
                            root.currentIndex = tabItem.index
                            root.activated(tabItem.index)
                        }
                    }
                }
            }
        }

        Rectangle {
            id: indicator

            readonly property Item target: root.currentIndex >= 0
                                           && root.currentIndex < repeater.count
                                           ? repeater.itemAt(root.currentIndex)
                                           : null

            x: indicator.target ? indicator.target.x : 0
            y: Design.controlHeightSmall
            width: indicator.target ? indicator.target.width : 0
            height: 2
            radius: 1
            color: BeyGramTheme.accent
            visible: indicator.target !== null

            Behavior on x {
                NumberAnimation {
                    duration: Design.motionNormal
                    easing.type: Easing.OutCubic
                }
            }

            Behavior on width {
                NumberAnimation {
                    duration: Design.motionNormal
                    easing.type: Easing.OutCubic
                }
            }
        }
    }
}
