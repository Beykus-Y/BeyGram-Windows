import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0
import BeyGram.App 1.0

Item {
    id: overlay
    anchors.fill: parent
    visible: BeyGramPlayback.open
    z: 1000
    focus: visible

    Keys.onEscapePressed: BeyGramPlayback.stop()

    Rectangle {
        anchors.fill: parent
        color: Qt.alpha("#000000", 0.94)
    }

    VideoOutput {
        anchors.fill: parent
        anchors.topMargin: 58
        anchors.bottomMargin: 86
        source: BeyGramPlayback.mediaPlayer
        fillMode: VideoOutput.PreserveAspectFit
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 58
        color: Qt.alpha("#000000", 0.55)

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 18
            anchors.rightMargin: 12
            spacing: 10

            Text {
                Layout.fillWidth: true
                text: BeyGramPlayback.title
                color: "white"
                font.pixelSize: 14
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }
            SecondaryButton {
                text: "Close"
                onClicked: BeyGramPlayback.stop()
            }
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 86
        color: Qt.alpha("#000000", 0.65)

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 14
            spacing: 8

            Slider {
                id: positionSlider
                Layout.fillWidth: true
                from: 0
                to: Math.max(1, BeyGramPlayback.duration)
                value: BeyGramPlayback.position
                enabled: BeyGramPlayback.duration > 0
                onMoved: BeyGramPlayback.seek(value)
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                PrimaryButton {
                    text: BeyGramPlayback.playing ? "Pause" : "Play"
                    enabled: BeyGramPlayback.open
                    onClicked: BeyGramPlayback.toggle()
                }
                Text {
                    text: formatTime(BeyGramPlayback.position)
                          + " / " + formatTime(BeyGramPlayback.duration)
                    color: "white"
                    font.pixelSize: 11
                }
                Item { Layout.fillWidth: true }
                Text {
                    text: BeyGramPlayback.error
                    color: "#FF8A80"
                    font.pixelSize: 11
                    visible: text.length > 0
                }
            }
        }
    }

    function formatTime(milliseconds) {
        const total = Math.max(0, Math.round(milliseconds / 1000))
        const minutes = Math.floor(total / 60)
        const seconds = total % 60
        return minutes + ":" + (seconds < 10 ? "0" : "") + seconds
    }

    Connections {
        target: BeyGramPlayback
        function onStateChanged() {
            if (BeyGramPlayback.open)
                overlay.forceActiveFocus()
        }
    }
}
