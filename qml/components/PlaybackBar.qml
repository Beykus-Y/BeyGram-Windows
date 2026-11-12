// PlaybackBar — bottom strip for the currently open video or audio track.
//
// Presentation only. All state lives in the injected `facade`
// (beygram::ui::PlaybackFacade): the bar reads its title, position, duration,
// volume and rate and asks it to play/pause, seek, change the volume, change
// the rate and step through the queue. Nothing here loads media, touches TDLib
// or logs: the facade only announces playRequested()/pauseRequested() and the
// owner acts on them.

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0

Rectangle {
    id: bar

    /// The PlaybackFacade driving this bar. May be null while the page is
    /// still being constructed.
    property var facade

    readonly property bool hasFacade: bar.facade ? true : false
    readonly property bool isActive: bar.hasFacade ? bar.facade.active : false
    readonly property string title: bar.hasFacade ? bar.facade.title : ""
    readonly property bool playing: bar.hasFacade ? bar.facade.playing : false
    readonly property int positionMs: bar.hasFacade ? bar.facade.positionMs : 0
    readonly property int durationMs: bar.hasFacade ? bar.facade.durationMs : 0
    readonly property int volume: bar.hasFacade ? bar.facade.volume : 100
    readonly property real rate: bar.hasFacade ? bar.facade.rate : 1.0
    readonly property bool hasNext: bar.hasFacade ? bar.facade.hasNext : false

    /// Offered playback speeds; the facade clamps to 0.25..3.0.
    readonly property var rateOptions: [0.5, 0.75, 1.0, 1.25, 1.5, 2.0]
    readonly property var rateLabels: ["0.5x", "0.75x", "1x", "1.25x", "1.5x", "2x"]

    /// Index of the offered speed closest to the current rate, so a rate set
    /// from elsewhere still shows a sensible label.
    readonly property int rateIndex: {
        var nearest = 0
        var best = Number.MAX_VALUE
        for (var i = 0; i < bar.rateOptions.length; ++i) {
            var distance = Math.abs(bar.rateOptions[i] - bar.rate)
            if (distance < best) {
                best = distance
                nearest = i
            }
        }
        return nearest
    }

    visible: bar.isActive
    implicitHeight: 88
    height: 88
    color: BeyGramTheme.surface2
    clip: true

    function formatTime(milliseconds) {
        var total = Math.max(0, Math.round(milliseconds / 1000))
        var minutes = Math.floor(total / 60)
        var seconds = total % 60
        return minutes + ":" + (seconds < 10 ? "0" : "") + seconds
    }

    // Separator against the content above, matching the muted line used by the
    // pinned message banner and the selection toolbar.
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 1
        color: BeyGramTheme.line
    }

    // Compact flat button shared by the transport actions.
    component BarButton: ToolButton {
        id: action

        implicitWidth: 32
        implicitHeight: 26
        padding: 0
        hoverEnabled: true
        focusPolicy: Qt.NoFocus
        opacity: enabled ? 1.0 : 0.4

        Accessible.role: Accessible.Button
        Accessible.name: action.text

        contentItem: Text {
            text: action.text
            color: action.hovered ? BeyGramTheme.text : BeyGramTheme.muted
            font.pixelSize: 12
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        background: Rectangle {
            radius: 9
            color: !action.enabled ? "transparent"
                 : action.pressed ? Qt.alpha(BeyGramTheme.accentInk, 0.18)
                 : action.hovered ? Qt.alpha(BeyGramTheme.accentInk, 0.10)
                 : "transparent"
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 14
        anchors.rightMargin: 14
        anchors.topMargin: 8
        anchors.bottomMargin: 8
        spacing: 4

        // Title and the elapsed/total time.
        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Text {
                Layout.fillWidth: true
                text: bar.title
                color: BeyGramTheme.text
                font.pixelSize: 12
                font.weight: Font.DemiBold
                elide: Text.ElideRight
                maximumLineCount: 1
                wrapMode: Text.NoWrap
            }

            Text {
                Layout.alignment: Qt.AlignVCenter
                text: bar.formatTime(bar.positionMs) + " / " + bar.formatTime(bar.durationMs)
                color: BeyGramTheme.muted
                font.pixelSize: 11
            }
        }

        Slider {
            id: seekSlider
            Layout.fillWidth: true
            from: 0
            to: Math.max(1, bar.durationMs)
            value: bar.positionMs
            enabled: bar.durationMs > 0
            onMoved: {
                if (bar.hasFacade)
                    bar.facade.seek(Math.round(value))
            }

            Accessible.role: Accessible.Slider
            Accessible.name: qsTr("Playback position")
        }

        // Transport, queue stepping, volume and speed.
        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            BarButton {
                text: bar.playing ? "\u2016" : "\u25B6"
                implicitWidth: 34
                onClicked: {
                    if (bar.hasFacade)
                        bar.facade.playPause()
                }
                Accessible.name: bar.playing ? qsTr("Pause") : qsTr("Play")
            }

            BarButton {
                text: qsTr("Prev")
                onClicked: {
                    if (bar.hasFacade)
                        bar.facade.previous()
                }
            }

            BarButton {
                text: qsTr("Next")
                enabled: bar.hasNext
                onClicked: {
                    if (bar.hasFacade)
                        bar.facade.next()
                }
            }

            Item { Layout.fillWidth: true }

            Text {
                Layout.alignment: Qt.AlignVCenter
                text: qsTr("Vol")
                color: BeyGramTheme.muted
                font.pixelSize: 10
                font.weight: Font.DemiBold
            }

            Slider {
                id: volumeSlider
                Layout.preferredWidth: 84
                from: 0
                to: 100
                stepSize: 1
                value: bar.volume
                onMoved: {
                    if (bar.hasFacade)
                        bar.facade.setVolume(Math.round(value))
                }

                Accessible.role: Accessible.Slider
                Accessible.name: qsTr("Volume")
            }

            ComboBox {
                id: rateBox
                Layout.preferredWidth: 76
                implicitHeight: 26
                model: bar.rateLabels
                currentIndex: bar.rateIndex
                focusPolicy: Qt.NoFocus
                palette.text: BeyGramTheme.text
                palette.windowText: BeyGramTheme.text
                palette.button: BeyGramTheme.surface2
                palette.buttonText: BeyGramTheme.text
                palette.base: BeyGramTheme.surface2
                palette.window: BeyGramTheme.surface
                palette.highlight: BeyGramTheme.accentSoft
                palette.highlightedText: BeyGramTheme.accentInk
                onActivated: function(index) {
                    if (bar.hasFacade)
                        bar.facade.setRate(bar.rateOptions[index])
                }

                Accessible.role: Accessible.ComboBox
                Accessible.name: qsTr("Playback speed")
            }
        }
    }
}
