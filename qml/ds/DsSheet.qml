// DsSheet - bottom sheet for the BeyGram design system.
//
// A modal popup that slides up from the bottom edge of the window. The panel is
// rounded at the top, capped at 80% of the parent height, and scrolls its
// content when it does not fit. The default `content` property holds the
// controls. Pressing Escape or tapping the scrim calls the inherited close().

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0
import "design.js" as Design

Popup {
    id: root

    property string title: ""
    default property alias content: contentColumn.data

    property int maxHeight: parent ? Math.round(parent.height * 0.8) : 0
    readonly property int sheetHeight: Math.min(Math.round(panelColumn.implicitHeight),
                                                maxHeight)

    function _slideIn() {
        if (!root.visible)
            return
        slideIn.from = panelHost.height
        slideIn.start()
    }

    parent: Overlay.overlay
    x: 0
    y: 0
    width: parent ? parent.width : 0
    height: parent ? parent.height : 0
    padding: 0
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape

    onOpened: Qt.callLater(root._slideIn)
    onClosed: {
        slideIn.stop()
        // Park the panel off-screen again for the next open.
        panel.y = Qt.binding(function() { return panelHost.height })
    }

    Overlay.modal: Rectangle {
        color: Qt.alpha("#000000", Design.opacityScrim)
    }

    background: null

    contentItem: Item {
        id: overlayLayer

        // Tapping the dimmed area dismisses the sheet. The popup spans the whole
        // window, so Popup.CloseOnPressOutside can never fire.
        MouseArea {
            anchors.fill: parent
            onClicked: root.close()
        }

        Item {
            id: panelHost
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: root.sheetHeight

            Rectangle {
                id: panel
                width: parent.width
                height: parent.height
                y: parent.height
                radius: Design.radiusXl
                color: BeyGramTheme.surface

                // Square off the bottom edge: the sheet is flush with the window.
                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: Design.radiusXl
                    color: BeyGramTheme.surface
                }

                // Absorb presses so they never reach the scrim handler above.
                MouseArea {
                    anchors.fill: parent
                }

                ColumnLayout {
                    id: panelColumn
                    anchors.fill: parent
                    spacing: 0

                    Item {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 22

                        Rectangle {
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.verticalCenter: parent.verticalCenter
                            width: 36
                            height: 4
                            radius: 2
                            color: Qt.alpha(BeyGramTheme.muted, 0.4)
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        Layout.leftMargin: Design.space5
                        Layout.rightMargin: Design.space5
                        Layout.bottomMargin: Design.space3
                        visible: root.title.length > 0
                        text: root.title
                        color: BeyGramTheme.text
                        font.pixelSize: Design.fontTitle
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                    }

                    ScrollView {
                        id: scrollView
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.leftMargin: Design.space5
                        Layout.rightMargin: Design.space5
                        Layout.topMargin: Design.space2
                        Layout.bottomMargin: Design.space5
                        padding: 0
                        clip: true
                        background: null
                        contentWidth: scrollView.availableWidth
                        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                        ColumnLayout {
                            id: contentColumn
                            width: scrollView.availableWidth
                            spacing: Design.space3
                        }
                    }
                }
            }
        }

        NumberAnimation {
            id: slideIn
            target: panel
            property: "y"
            to: 0
            duration: Design.motionNormal
            easing.type: Easing.OutCubic
        }
    }
}
