// Full-screen media gallery overlay.
//
// The component is passive: it reads and drives the GalleryFacade assigned to
// `facade` and never owns it, so the chat view that opens the gallery keeps
// the single facade instance. While `facade` is null the overlay stays hidden.
//
// The rounded frame stands in for the media of the current item and is
// labelled with the message id it belongs to. The arrows over its sides step
// through the gallery, the bottom bar shows the "i / n" counter and the zoom
// controls, and the header closes the gallery. Escape, the left arrow and the
// right arrow work as keyboard shortcuts.
//
// The facade clamps the zoom to 1.0..5.0 and resets it whenever the current
// item changes, so the overlay never applies a zoom of its own.

import QtQuick
import QtQuick.Layouts
import BeyGram.Theme 1.0
import "../ds/design.js" as Design

Item {
    id: root

    /// The GalleryFacade instance that drives this overlay.
    property var facade: null

    readonly property bool open: root.facade !== null && root.facade.open === true
    readonly property int itemCount: root.facade !== null ? Number(root.facade.count) : 0
    readonly property int itemIndex: root.facade !== null ? Number(root.facade.index) : -1
    readonly property string currentId: root.facade !== null
                                        ? String(root.facade.currentId || "") : ""
    readonly property real zoom: root.facade !== null ? Number(root.facade.zoom) : 1.0

    readonly property bool canGoPrevious: root.itemIndex > 0
    readonly property bool canGoNext: root.itemIndex >= 0
                                      && root.itemIndex + 1 < root.itemCount

    // The only literal colour in the overlay is the scrim; everything else is
    // derived from the active theme so Light, Dark and AMOLED stay coherent.
    readonly property color _scrim: Qt.alpha("#000000", 0.92)
    readonly property color _ink: BeyGramTheme.onAccent
    readonly property color _inkSecondary: Qt.alpha(BeyGramTheme.onAccent, Design.opacitySecondary)
    readonly property color _hairline: Qt.alpha(BeyGramTheme.onAccent, 0.10)
    readonly property color _frameFill: Qt.alpha(BeyGramTheme.onAccent, 0.06)
    readonly property color _frameLine: Qt.alpha(BeyGramTheme.onAccent, 0.14)

    anchors.fill: parent
    visible: root.open
    z: 1000
    focus: root.open

    onVisibleChanged: {
        if (visible)
            root.forceActiveFocus()
    }

    Keys.onEscapePressed: root.closeGallery()
    Keys.onLeftPressed: root.goPrevious()
    Keys.onRightPressed: root.goNext()
    Keys.onPressed: function (event) {
        if (event.key === Qt.Key_Plus || event.key === Qt.Key_Equal) {
            root.zoomIn()
            event.accepted = true
        } else if (event.key === Qt.Key_Minus) {
            root.zoomOut()
            event.accepted = true
        } else if (event.key === Qt.Key_0) {
            root.resetZoom()
            event.accepted = true
        }
    }

    function closeGallery() {
        if (root.facade !== null)
            root.facade.close()
    }

    function goNext() {
        if (root.facade !== null && root.canGoNext)
            root.facade.next()
    }

    function goPrevious() {
        if (root.facade !== null && root.canGoPrevious)
            root.facade.previous()
    }

    function zoomIn() {
        if (root.facade !== null)
            root.facade.setZoom(root.zoom * 1.25)
    }

    function zoomOut() {
        if (root.facade !== null)
            root.facade.setZoom(root.zoom / 1.25)
    }

    function resetZoom() {
        if (root.facade !== null)
            root.facade.resetZoom()
    }

    // Full-bleed scrim. A click outside the media closes the gallery.
    Rectangle {
        anchors.fill: parent
        color: root._scrim

        MouseArea {
            anchors.fill: parent
            onClicked: root.closeGallery()
        }
    }

    Rectangle {
        id: topBar

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: Design.headerHeight
        color: "transparent"

        // Chrome clicks must not fall through to the closing scrim.
        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.AllButtons
        }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 1
            color: root._hairline
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Design.space4
            anchors.rightMargin: Design.space4
            spacing: Design.space3

            Text {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                text: root.currentId.length > 0
                      ? qsTr("Gallery · %1").arg(root.currentId) : qsTr("Gallery")
                color: root._ink
                font.pixelSize: Design.fontTitle
                font.weight: Font.DemiBold
                elide: Text.ElideRight
                maximumLineCount: 1
            }

            DsIconButton {
                Layout.alignment: Qt.AlignVCenter
                icon: "\uE8BB"
                tone: "soft"
                tooltip: qsTr("Close")
                onClicked: root.closeGallery()
            }
        }
    }

    Item {
        id: stage

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: topBar.bottom
        anchors.bottom: bottomBar.top
        clip: true

        Rectangle {
            id: frame

            anchors.fill: parent
            anchors.margins: Design.space6
            radius: Design.radiusMd
            color: root._frameFill
            border.width: 1
            border.color: root._frameLine
            clip: true
            antialiasing: true

            // Zoom scales the placeholder inside the fixed 12 px frame.
            Item {
                id: zoomLayer

                anchors.fill: parent

                transform: Scale {
                    origin.x: zoomLayer.width / 2
                    origin.y: zoomLayer.height / 2
                    xScale: root.zoom
                    yScale: root.zoom
                }

                Column {
                    anchors.centerIn: parent
                    width: Math.max(0, parent.width - Design.space8)
                    spacing: Design.space3

                    Item {
                        width: parent.width
                        height: Design.space10 + Design.space2

                        DsIcon {
                            anchors.centerIn: parent
                            name: "\uE91B"
                            size: Design.space10
                            color: Qt.alpha(BeyGramTheme.onAccent, 0.55)
                        }
                    }

                    Text {
                        width: parent.width
                        horizontalAlignment: Text.AlignHCenter
                        text: qsTr("Media")
                        color: root._inkSecondary
                        font.pixelSize: Design.fontCaption
                        font.weight: Font.DemiBold
                    }

                    Text {
                        width: parent.width
                        horizontalAlignment: Text.AlignHCenter
                        text: root.currentId.length > 0
                              ? qsTr("Message %1").arg(root.currentId)
                              : qsTr("No media")
                        color: root._ink
                        font.pixelSize: Design.fontHeadline
                        font.weight: Font.DemiBold
                        elide: Text.ElideMiddle
                        maximumLineCount: 1
                    }

                    Text {
                        width: parent.width
                        horizontalAlignment: Text.AlignHCenter
                        text: qsTr("Full-size media appears here")
                        color: root._inkSecondary
                        font.pixelSize: Design.fontSmall
                    }
                }
            }

            // Keeps a click on the media from reaching the closing scrim.
            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.AllButtons
                onClicked: function (mouse) {
                    mouse.accepted = true
                }
                onWheel: function (wheel) {
                    if (wheel.angleDelta.y > 0)
                        root.zoomIn()
                    else if (wheel.angleDelta.y < 0)
                        root.zoomOut()
                    wheel.accepted = true
                }
            }
        }

        DsIconButton {
            anchors.left: parent.left
            anchors.leftMargin: Design.space3
            anchors.verticalCenter: parent.verticalCenter
            z: 2
            icon: "\uE76B"
            tone: "soft"
            size: "lg"
            tooltip: qsTr("Previous media")
            enabled: root.canGoPrevious
            onClicked: root.goPrevious()
        }

        DsIconButton {
            anchors.right: parent.right
            anchors.rightMargin: Design.space3
            anchors.verticalCenter: parent.verticalCenter
            z: 2
            icon: "\uE76C"
            tone: "soft"
            size: "lg"
            tooltip: qsTr("Next media")
            enabled: root.canGoNext
            onClicked: root.goNext()
        }
    }

    Rectangle {
        id: bottomBar

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: Design.space4 * 2 + Design.controlHeightLarge
        color: "transparent"

        // Chrome clicks must not fall through to the closing scrim.
        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.AllButtons
        }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 1
            color: root._hairline
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Design.space4
            anchors.rightMargin: Design.space4
            spacing: Design.space2

            Text {
                Layout.alignment: Qt.AlignVCenter
                text: root.open ? (root.itemIndex + 1) + " / " + root.itemCount
                                : "0 / 0"
                color: root._ink
                font.pixelSize: Design.fontSmall
                font.weight: Font.DemiBold
            }

            Item { Layout.fillWidth: true }

            Text {
                Layout.alignment: Qt.AlignVCenter
                text: qsTr("Zoom")
                color: root._inkSecondary
                font.pixelSize: Design.fontCaption
                font.weight: Font.DemiBold
            }

            DsIconButton {
                Layout.alignment: Qt.AlignVCenter
                icon: "\uE8A3"
                tone: "soft"
                tooltip: qsTr("Zoom out")
                enabled: root.zoom > 1.001
                onClicked: root.zoomOut()
            }

            Text {
                Layout.preferredWidth: Design.space10 + Design.space4
                Layout.alignment: Qt.AlignVCenter
                horizontalAlignment: Text.AlignHCenter
                text: Math.round(root.zoom * 100) + "%"
                color: root._ink
                font.pixelSize: Design.fontSmall
                font.weight: Font.DemiBold
            }

            DsIconButton {
                Layout.alignment: Qt.AlignVCenter
                icon: "\uE71F"
                tone: "soft"
                tooltip: qsTr("Zoom in")
                enabled: root.zoom < 4.999
                onClicked: root.zoomIn()
            }

            DsIconButton {
                Layout.alignment: Qt.AlignVCenter
                icon: "\uE72C"
                tone: "soft"
                tooltip: qsTr("Reset zoom")
                enabled: root.zoom > 1.001
                onClicked: root.resetZoom()
            }
        }
    }
}
