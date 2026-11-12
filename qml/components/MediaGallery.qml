import QtQuick
import QtQuick.Layouts
import BeyGram.Theme 1.0

/// Full-screen media gallery overlay.
///
/// The component is passive: it reads and drives the GalleryFacade assigned to
/// `facade` and never owns it, so the chat view that opens the gallery keeps
/// the single facade instance. While `facade` is null the overlay stays hidden.
///
/// The large rounded placeholder stands in for the media of the current item
/// and is labelled with the message id it belongs to. The arrows over its sides
/// step through the gallery, the bottom bar shows the "i / n" counter and the
/// zoom controls, and the header closes the gallery. Escape, the left arrow and
/// the right arrow work as keyboard shortcuts.
///
/// The facade clamps the zoom to 1.0..5.0 and resets it whenever the current
/// item changes, so the overlay never applies a zoom of its own.
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

    anchors.fill: parent
    visible: root.open
    z: 1000
    focus: root.open

    Keys.onEscapePressed: root.closeGallery()
    Keys.onLeftPressed: root.goPrevious()
    Keys.onRightPressed: root.goNext()

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

    // Backdrop: darkens the chat and closes the gallery on a click outside the
    // media, like the single-media viewer does.
    Rectangle {
        anchors.fill: parent
        color: Qt.alpha("#000000", 0.94)

        MouseArea {
            anchors.fill: parent
            onClicked: root.closeGallery()
        }
    }

    Rectangle {
        id: stage
        anchors.fill: parent
        anchors.topMargin: 56
        anchors.bottomMargin: 76
        color: "transparent"

        Rectangle {
            id: placeholder
            anchors.fill: parent
            anchors.margins: 26
            radius: 18
            color: Qt.alpha("#141414", 0.92)
            border.width: 1
            border.color: Qt.alpha(BeyGramTheme.accent, 0.3)
            clip: true

            // The zoom belongs to the item on screen, so the placeholder scales
            // around its centre; navigation resets it through the facade.
            transform: Scale {
                origin.x: placeholder.width / 2
                origin.y: placeholder.height / 2
                xScale: root.zoom
                yScale: root.zoom
            }

            Column {
                width: parent.width
                anchors.verticalCenter: parent.verticalCenter
                spacing: 8

                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    text: qsTr("MEDIA")
                    color: BeyGramTheme.muted
                    font.pixelSize: 11
                    font.letterSpacing: 1.6
                }

                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    text: root.currentId.length > 0
                          ? qsTr("Message %1").arg(root.currentId)
                          : qsTr("No media")
                    color: BeyGramTheme.text
                    font.pixelSize: 26
                    font.weight: Font.DemiBold
                    elide: Text.ElideMiddle
                }

                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    text: qsTr("Full-size media appears here")
                    color: BeyGramTheme.muted
                    font.pixelSize: 11
                }
            }
        }

        SecondaryButton {
            anchors.left: parent.left
            anchors.leftMargin: 4
            anchors.verticalCenter: parent.verticalCenter
            width: 44
            height: 44
            text: qsTr("‹")
            enabled: root.canGoPrevious
            onClicked: root.goPrevious()
            Accessible.name: qsTr("Previous media")
        }

        SecondaryButton {
            anchors.right: parent.right
            anchors.rightMargin: 4
            anchors.verticalCenter: parent.verticalCenter
            width: 44
            height: 44
            text: qsTr("›")
            enabled: root.canGoNext
            onClicked: root.goNext()
            Accessible.name: qsTr("Next media")
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 56
        color: Qt.alpha("#000000", 0.55)

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 18
            anchors.rightMargin: 12
            spacing: 10

            Text {
                Layout.fillWidth: true
                text: root.currentId.length > 0
                      ? qsTr("Gallery · %1").arg(root.currentId) : qsTr("Gallery")
                color: "white"
                font.pixelSize: 14
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }

            SecondaryButton {
                text: qsTr("Close")
                onClicked: root.closeGallery()
            }
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 76
        color: Qt.alpha("#000000", 0.55)

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 18
            anchors.rightMargin: 18
            spacing: 10

            Text {
                text: root.open ? (root.itemIndex + 1) + " / " + root.itemCount
                                : "0 / 0"
                color: "white"
                font.pixelSize: 13
                font.weight: Font.DemiBold
            }

            Item { Layout.fillWidth: true }

            SecondaryButton {
                Layout.preferredWidth: 40
                text: qsTr("−")
                enabled: root.zoom > 1.0
                onClicked: root.zoomOut()
                Accessible.name: qsTr("Zoom out")
            }

            Text {
                Layout.preferredWidth: 48
                horizontalAlignment: Text.AlignHCenter
                text: Math.round(root.zoom * 100) + "%"
                color: "white"
                font.pixelSize: 12
            }

            SecondaryButton {
                Layout.preferredWidth: 40
                text: qsTr("+")
                enabled: root.zoom < 5.0
                onClicked: root.zoomIn()
                Accessible.name: qsTr("Zoom in")
            }

            SecondaryButton {
                text: qsTr("Reset")
                enabled: root.zoom > 1.0
                onClicked: root.resetZoom()
                Accessible.name: qsTr("Reset zoom")
            }
        }
    }

    Connections {
        target: root.facade

        function onChanged() {
            if (root.open && !root.activeFocus)
                root.forceActiveFocus()
        }
    }
}
