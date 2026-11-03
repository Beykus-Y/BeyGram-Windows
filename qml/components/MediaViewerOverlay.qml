import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import BeyGram.Theme 1.0
import BeyGram.App 1.0

Item {
    id: overlay
    anchors.fill: parent
    visible: BeyGramMediaViewer.open
    z: 1000
    focus: visible

    Keys.onEscapePressed: BeyGramMediaViewer.close()

    Rectangle {
        anchors.fill: parent
        color: Qt.alpha("#000000", 0.92)

        MouseArea {
            anchors.fill: parent
            onClicked: BeyGramMediaViewer.close()
        }
    }

    Image {
        id: viewerImage
        anchors.fill: parent
        anchors.margins: 34
        source: BeyGramMediaViewer.source
        cache: false
        asynchronous: true
        fillMode: Image.PreserveAspectFit
        smooth: true
    }

    BusyIndicator {
        anchors.centerIn: parent
        running: BeyGramMediaViewer.loading
        visible: running
        implicitWidth: 36
        implicitHeight: 36
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 60
        color: Qt.alpha("#000000", 0.55)

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 18
            anchors.rightMargin: 12
            spacing: 10

            Text {
                Layout.fillWidth: true
                text: BeyGramMediaViewer.title
                color: "white"
                font.pixelSize: 14
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }

            SecondaryButton {
                text: "Save as"
                enabled: !BeyGramMediaViewer.loading
                         && BeyGramMediaViewer.source.length > 0
                onClicked: saveDialog.open()
            }
            SecondaryButton {
                text: "Close"
                onClicked: BeyGramMediaViewer.close()
            }
        }
    }

    Text {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 24
        text: BeyGramMediaViewer.error
        color: "#FF8A80"
        font.pixelSize: 12
        visible: text.length > 0
    }

    FileDialog {
        id: saveDialog
        title: "Save media"
        fileMode: FileDialog.SaveFile
        nameFilters: ["Images (*.jpg *.jpeg *.png *.webp)", "All files (*)"]
        onAccepted: BeyGramApp.saveOpenMediaAs(selectedFile)
    }

    Connections {
        target: BeyGramMediaViewer
        function onStateChanged() {
            if (BeyGramMediaViewer.open)
                overlay.forceActiveFocus()
        }
    }
}
