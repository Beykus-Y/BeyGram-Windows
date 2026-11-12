// MediaViewerOverlay — full-screen viewer for downloaded images.
//
// State lives in the BeyGramMediaViewer singleton (open, title, source,
// loading, error); this file only presents it. The scrim dismisses the viewer,
// the frame swallows clicks so only the backdrop closes, and the bottom bar
// owns the local zoom because the controller has no zoom state of its own.

import QtQuick
import QtQuick.Layouts
import QtQuick.Dialogs
import BeyGram.Theme 1.0
import BeyGram.App 1.0
import "../ds/design.js" as Design

Item {
    id: overlay

    anchors.fill: parent
    visible: BeyGramMediaViewer.open
    z: 1000
    focus: visible

    /// Local view zoom, 1.0..5.0. Reset whenever the viewer opens.
    property real zoom: 1.0

    // The only literal colour in the overlay is the scrim; everything else is
    // derived from the active theme so Light, Dark and AMOLED stay coherent.
    readonly property color _scrim: Qt.alpha("#000000", 0.92)
    readonly property color _ink: BeyGramTheme.onAccent
    readonly property color _inkSecondary: Qt.alpha(BeyGramTheme.onAccent, Design.opacitySecondary)
    readonly property color _hairline: Qt.alpha(BeyGramTheme.onAccent, 0.10)
    readonly property color _frameFill: Qt.alpha(BeyGramTheme.onAccent, 0.06)
    readonly property color _frameLine: Qt.alpha(BeyGramTheme.onAccent, 0.14)

    Behavior on zoom {
        enabled: overlay.visible

        NumberAnimation {
            duration: Design.motionNormal
            easing.type: Easing.OutCubic
        }
    }

    onVisibleChanged: {
        if (visible) {
            overlay.forceActiveFocus()
        } else {
            overlay.zoom = 1.0
        }
    }

    Keys.onEscapePressed: BeyGramMediaViewer.close()
    Keys.onPressed: function (event) {
        if (event.key === Qt.Key_Plus || event.key === Qt.Key_Equal) {
            overlay.zoomIn()
            event.accepted = true
        } else if (event.key === Qt.Key_Minus) {
            overlay.zoomOut()
            event.accepted = true
        } else if (event.key === Qt.Key_0) {
            overlay.resetZoom()
            event.accepted = true
        }
    }

    function zoomIn() {
        overlay.zoom = Math.min(5.0, overlay.zoom * 1.25)
    }

    function zoomOut() {
        overlay.zoom = Math.max(1.0, overlay.zoom / 1.25)
    }

    function resetZoom() {
        overlay.zoom = 1.0
    }

    function formatPercent(value) {
        return Math.round(value * 100) + "%"
    }

    // Full-bleed scrim. A click outside the media closes the viewer.
    Rectangle {
        anchors.fill: parent
        color: overlay._scrim

        MouseArea {
            anchors.fill: parent
            onClicked: BeyGramMediaViewer.close()
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
            color: overlay._hairline
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Design.space4
            anchors.rightMargin: Design.space4
            spacing: Design.space3

            Text {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                text: BeyGramMediaViewer.title.length > 0
                      ? BeyGramMediaViewer.title : qsTr("Media")
                color: overlay._ink
                font.pixelSize: Design.fontTitle
                font.weight: Font.DemiBold
                elide: Text.ElideRight
                maximumLineCount: 1
            }

            DsIconButton {
                Layout.alignment: Qt.AlignVCenter
                icon: "\uE74E"
                tone: "soft"
                tooltip: qsTr("Save as")
                enabled: !BeyGramMediaViewer.loading
                         && BeyGramMediaViewer.source.length > 0
                onClicked: saveDialog.open()
            }

            DsIconButton {
                Layout.alignment: Qt.AlignVCenter
                icon: "\uE8BB"
                tone: "soft"
                tooltip: qsTr("Close")
                onClicked: BeyGramMediaViewer.close()
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
            color: overlay._frameFill
            border.width: 1
            border.color: overlay._frameLine
            clip: true
            antialiasing: true

            Image {
                id: viewerImage

                anchors.fill: parent
                anchors.margins: Design.space1
                source: BeyGramMediaViewer.source
                cache: false
                asynchronous: true
                fillMode: Image.PreserveAspectFit
                smooth: true
                visible: status !== Image.Error

                transform: Scale {
                    origin.x: viewerImage.width / 2
                    origin.y: viewerImage.height / 2
                    xScale: overlay.zoom
                    yScale: overlay.zoom
                }
            }

            DsSpinner {
                anchors.centerIn: parent
                running: BeyGramMediaViewer.loading
                visible: running
                color: overlay._ink
            }

            // Keeps a click on the picture from reaching the closing scrim.
            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.AllButtons
                onClicked: function (mouse) {
                    mouse.accepted = true
                }
                onWheel: function (wheel) {
                    if (wheel.angleDelta.y > 0)
                        overlay.zoomIn()
                    else if (wheel.angleDelta.y < 0)
                        overlay.zoomOut()
                    wheel.accepted = true
                }
            }
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: Design.space2
            width: Math.max(0, Math.min(implicitWidth, parent.width - Design.space8))
            text: BeyGramMediaViewer.error
            color: Qt.lighter(BeyGramTheme.danger, 1.3)
            font.pixelSize: Design.fontSmall
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            visible: text.length > 0
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
            color: overlay._hairline
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Design.space4
            anchors.rightMargin: Design.space4
            spacing: Design.space2

            Text {
                Layout.alignment: Qt.AlignVCenter
                text: qsTr("Zoom")
                color: overlay._inkSecondary
                font.pixelSize: Design.fontCaption
                font.weight: Font.DemiBold
            }

            DsIconButton {
                Layout.alignment: Qt.AlignVCenter
                icon: "\uE8A3"
                tone: "soft"
                tooltip: qsTr("Zoom out")
                enabled: overlay.zoom > 1.001
                onClicked: overlay.zoomOut()
            }

            Text {
                Layout.preferredWidth: Design.space10 + Design.space4
                Layout.alignment: Qt.AlignVCenter
                horizontalAlignment: Text.AlignHCenter
                text: overlay.formatPercent(overlay.zoom)
                color: overlay._ink
                font.pixelSize: Design.fontSmall
                font.weight: Font.DemiBold
            }

            DsIconButton {
                Layout.alignment: Qt.AlignVCenter
                icon: "\uE71F"
                tone: "soft"
                tooltip: qsTr("Zoom in")
                enabled: overlay.zoom < 4.999
                onClicked: overlay.zoomIn()
            }

            DsIconButton {
                Layout.alignment: Qt.AlignVCenter
                icon: "\uE72C"
                tone: "soft"
                tooltip: qsTr("Reset zoom")
                enabled: overlay.zoom > 1.001
                onClicked: overlay.resetZoom()
            }

            Item { Layout.fillWidth: true }
        }
    }

    FileDialog {
        id: saveDialog
        title: qsTr("Save media")
        fileMode: FileDialog.SaveFile
        nameFilters: ["Images (*.jpg *.jpeg *.png *.webp)", "All files (*)"]
        onAccepted: BeyGramApp.saveOpenMediaAs(selectedFile)
    }
}
