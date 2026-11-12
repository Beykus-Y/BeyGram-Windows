// MediaPlaybackOverlay — full-screen video and audio player.
//
// BeyGramPlayback owns the QMediaPlayer and the video output; this file binds
// its properties and calls its invokables. Playback rate is the one piece the
// controller does not expose, so it is applied to the QMediaPlayer directly and
// silently ignored by backends that do not offer it.

import QtQuick
import QtQuick.Layouts
import QtMultimedia
import BeyGram.Theme 1.0
import BeyGram.App 1.0
import "../ds/design.js" as Design

Item {
    id: overlay

    anchors.fill: parent
    visible: BeyGramPlayback.open
    z: 1000
    focus: visible

    /// Offered speeds and their labels. The controller has no rate property,
    /// so the selected index is local and pushed to QMediaPlayer.playbackRate.
    readonly property var rateOptions: [0.5, 0.75, 1.0, 1.25, 1.5, 2.0]
    readonly property var rateLabels: ["0.5x", "0.75x", "1x", "1.25x", "1.5x", "2x"]
    property int rateIndex: 2
    readonly property real rate: overlay.rateIndex >= 0
                                 && overlay.rateIndex < overlay.rateOptions.length
                                 ? overlay.rateOptions[overlay.rateIndex] : 1.0

    /// Last non-zero volume, used by the mute button.
    property real lastVolume: 1.0

    // The only literal colour in the overlay is the scrim; everything else is
    // derived from the active theme so Light, Dark and AMOLED stay coherent.
    readonly property color _scrim: Qt.alpha("#000000", 0.92)
    readonly property color _ink: BeyGramTheme.onAccent
    readonly property color _inkSecondary: Qt.alpha(BeyGramTheme.onAccent, Design.opacitySecondary)
    readonly property color _hairline: Qt.alpha(BeyGramTheme.onAccent, 0.10)
    readonly property color _frameFill: Qt.alpha(BeyGramTheme.onAccent, 0.06)
    readonly property color _frameLine: Qt.alpha(BeyGramTheme.onAccent, 0.14)

    Component.onCompleted: BeyGramPlayback.videoOutput = videoOut

    onRateChanged: overlay.applyRate()

    onVisibleChanged: {
        if (visible) {
            overlay.rateIndex = 2
            overlay.forceActiveFocus()
            overlay.applyRate()
        }
    }

    Keys.onEscapePressed: overlay.closePlayback()
    Keys.onSpacePressed: BeyGramPlayback.toggle()

    // MediaPlaybackController has no close() invokable: stop() keeps the source
    // loaded, so open would stay true. Opening an empty source is the only
    // public way to clear it; the next open() clears the recorded error.
    function closePlayback() {
        BeyGramPlayback.stop()
        if (BeyGramPlayback.open)
            BeyGramPlayback.open("", "")
    }

    function applyRate() {
        if (!BeyGramPlayback.mediaPlayer)
            return
        try {
            BeyGramPlayback.mediaPlayer.playbackRate = overlay.rate
        } catch (error) {
            // A backend without playbackRate simply keeps the default speed.
        }
    }

    function cycleRate() {
        overlay.rateIndex = (overlay.rateIndex + 1) % overlay.rateOptions.length
    }

    function toggleMute() {
        if (BeyGramPlayback.volume > 0.001) {
            overlay.lastVolume = BeyGramPlayback.volume
            BeyGramPlayback.volume = 0
        } else {
            BeyGramPlayback.volume = overlay.lastVolume > 0.001
                                       ? overlay.lastVolume : 1.0
        }
    }

    function formatTime(milliseconds) {
        const total = Math.max(0, Math.round(milliseconds / 1000))
        const minutes = Math.floor(total / 60)
        const seconds = total % 60
        return minutes + ":" + (seconds < 10 ? "0" : "") + seconds
    }

    // Full-bleed scrim. It swallows clicks and wheel events so the page behind
    // the player cannot be used while a track is open.
    Rectangle {
        anchors.fill: parent
        color: overlay._scrim

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.AllButtons
            onWheel: function (wheel) {
                wheel.accepted = true
            }
        }
    }

    Rectangle {
        id: topBar

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: Design.headerHeight
        color: "transparent"

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
                text: BeyGramPlayback.title.length > 0
                      ? BeyGramPlayback.title : qsTr("Playback")
                color: overlay._ink
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
                onClicked: overlay.closePlayback()
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

            VideoOutput {
                id: videoOut

                anchors.fill: parent
                anchors.margins: Design.space1
                fillMode: VideoOutput.PreserveAspectFit
            }

            // A click on the picture toggles playback, as in a normal player.
            MouseArea {
                anchors.fill: parent
                onClicked: BeyGramPlayback.toggle()
            }
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: Design.space2
            width: Math.max(0, Math.min(implicitWidth, parent.width - Design.space8))
            text: BeyGramPlayback.error
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
        height: Design.space4 * 2 + Design.controlHeightSmall
                + Design.space2 + Design.controlHeightLarge
        color: "transparent"

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 1
            color: overlay._hairline
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Design.space4
            spacing: Design.space2

            // Seek row: elapsed, scrubber, duration.
            RowLayout {
                Layout.fillWidth: true
                spacing: Design.space3

                Text {
                    Layout.preferredWidth: Design.space10 + Design.space4
                    Layout.alignment: Qt.AlignVCenter
                    horizontalAlignment: Text.AlignRight
                    text: overlay.formatTime(BeyGramPlayback.position)
                    color: overlay._ink
                    font.pixelSize: Design.fontSmall
                    font.weight: Font.DemiBold
                }

                DsSlider {
                    id: positionSlider

                    Layout.fillWidth: true
                    Layout.preferredHeight: Design.controlHeightSmall
                    from: 0
                    to: Math.max(1, BeyGramPlayback.duration)
                    enabled: BeyGramPlayback.duration > 0
                    onMoved: BeyGramPlayback.seek(value)

                    Accessible.name: qsTr("Playback position")
                }

                Text {
                    Layout.preferredWidth: Design.space10 + Design.space4
                    Layout.alignment: Qt.AlignVCenter
                    text: overlay.formatTime(BeyGramPlayback.duration)
                    color: overlay._inkSecondary
                    font.pixelSize: Design.fontSmall
                }
            }

            // Transport row: play/pause, mute, volume, speed.
            RowLayout {
                Layout.fillWidth: true
                spacing: Design.space2

                DsIconButton {
                    Layout.alignment: Qt.AlignVCenter
                    icon: BeyGramPlayback.playing ? "\uE769" : "\uE768"
                    tone: "accent"
                    size: "lg"
                    tooltip: BeyGramPlayback.playing ? qsTr("Pause") : qsTr("Play")
                    enabled: BeyGramPlayback.open
                    onClicked: BeyGramPlayback.toggle()
                }

                DsIconButton {
                    Layout.alignment: Qt.AlignVCenter
                    icon: BeyGramPlayback.volume <= 0.001 ? "\uE74F" : "\uE767"
                    tone: "soft"
                    tooltip: BeyGramPlayback.volume <= 0.001
                             ? qsTr("Unmute") : qsTr("Mute")
                    onClicked: overlay.toggleMute()
                }

                DsSlider {
                    Layout.preferredWidth: Design.space10 * 2 + Design.space6
                    Layout.preferredHeight: Design.controlHeightSmall
                    Layout.alignment: Qt.AlignVCenter
                    from: 0
                    to: 1
                    stepSize: 0.01
                    value: BeyGramPlayback.volume
                    onMoved: BeyGramPlayback.volume = value

                    Accessible.name: qsTr("Volume")
                }

                Item { Layout.fillWidth: true }

                Text {
                    Layout.alignment: Qt.AlignVCenter
                    text: qsTr("Speed")
                    color: overlay._inkSecondary
                    font.pixelSize: Design.fontCaption
                    font.weight: Font.DemiBold
                }

                DsChip {
                    Layout.alignment: Qt.AlignVCenter
                    text: overlay.rateLabels[overlay.rateIndex]
                    selected: overlay.rateIndex !== 2
                    onClicked: overlay.cycleRate()
                }
            }
        }
    }

    // Keep the seek handle under the pointer while dragging; resume following
    // the player once the drag ends.
    Binding {
        target: positionSlider
        property: "value"
        value: BeyGramPlayback.position
        when: !positionSlider.pressed
    }
}
