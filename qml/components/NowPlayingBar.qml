// NowPlayingBar — compact now-playing strip for the music queue.
//
// Presentation only. All state lives in the injected `facade`
// (beygram::ui::MusicPlayerFacade): the bar reads its title, transport state,
// playhead, speed, shuffle and repeat mode and asks it to play/pause, seek,
// step through the queue, cycle the repeat mode and change the speed. Nothing
// here loads media or touches the session store; the host calls
// saveSession()/restoreSession() on the facade.
//
// The cover is a placeholder until artwork exists in the facade. The speed
// chip cycles through the named PlaybackSettings stops, and the repeat button
// shows the RepeatOne glyph while repeatMode is "one".
//
// Usage: NowPlayingBar { facade: <MusicPlayerFacade instance> }

import QtQuick
import QtQuick.Layouts
import BeyGram.Theme 1.0
import "../ds"
import "../ds/design.js" as Design

Rectangle {
    id: root

    Accessible.role: Accessible.Grouping
    Accessible.name: qsTr("Now playing")

    /// The MusicPlayerFacade driving this bar. May be null while the page is
    /// still being constructed.
    property var facade: null

    readonly property bool hasFacade: root.facade ? true : false
    readonly property string title: root.hasFacade ? root.facade.title : ""
    readonly property bool playing: root.hasFacade ? root.facade.playing : false
    readonly property int positionMs: root.hasFacade ? root.facade.positionMs : 0
    readonly property int durationMs: root.hasFacade ? root.facade.durationMs : 0
    readonly property real speed: root.hasFacade ? root.facade.speed : 1.0
    readonly property bool shuffle: root.hasFacade ? root.facade.shuffle : false
    readonly property string repeatMode: root.hasFacade ? root.facade.repeatMode : "off"
    readonly property bool hasNext: root.hasFacade ? root.facade.hasNext : false

    // A restored session has no titles, so a current track with an empty title
    // still needs a readable label instead of "Nothing playing".
    readonly property bool hasCurrent: root.hasFacade
                                       && (root.title.length > 0 || root.playing
                                           || root.positionMs > 0 || root.durationMs > 0)
    readonly property string displayTitle: root.title.length > 0
                                           ? root.title
                                           : (root.hasCurrent ? qsTr("Unknown track")
                                                              : qsTr("Nothing playing"))

    // The named PlaybackSettings stops. The facade clamps and rounds whatever
    // this chip writes, so the list only needs to offer the useful values.
    readonly property var speedOptions: [0.5, 0.75, 1.0, 1.25, 1.5, 1.75, 2.0]

    implicitWidth: 560
    implicitHeight: 78
    height: implicitHeight
    color: BeyGramTheme.surface
    clip: true
    // Hidden for a wired-but-empty queue; a host that wants a persistent strip
    // can override `visible`.
    visible: root.hasFacade && (root.hasCurrent || root.hasNext)

    function formatTime(milliseconds) {
        var total = Math.max(0, Math.round(Number(milliseconds) / 1000))
        var minutes = Math.floor(total / 60)
        var seconds = total % 60
        return minutes + ":" + (seconds < 10 ? "0" : "") + seconds
    }

    function speedLabel(value) {
        var speed = Math.round(Number(value) * 100) / 100
        if (!isFinite(speed))
            speed = 1
        return (speed % 1 === 0 ? speed.toFixed(0) : String(speed)) + "\u00D7"
    }

    function cycleSpeed() {
        var current = Number(root.speed)
        if (!isFinite(current))
            current = 1
        var next = root.speedOptions[0]
        for (var i = 0; i < root.speedOptions.length; ++i) {
            if (root.speedOptions[i] > current + 0.001) {
                next = root.speedOptions[i]
                break
            }
        }
        root.facade.speed = next
    }

    function repeatTooltip() {
        if (root.repeatMode === "one")
            return qsTr("Repeat one")
        if (root.repeatMode === "all")
            return qsTr("Repeat all")
        return qsTr("Repeat off")
    }

    // Hairline against the page above; the bar sits on the normal surface.
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 1
        color: BeyGramTheme.line
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: Design.space3
        anchors.rightMargin: Design.space3
        anchors.topMargin: Design.space2
        anchors.bottomMargin: Design.space1
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 48
            spacing: Design.space2

            // Cover placeholder: the facade has no artwork yet.
            Rectangle {
                id: cover

                Layout.alignment: Qt.AlignVCenter
                Layout.preferredWidth: 42
                Layout.preferredHeight: 42
                implicitWidth: 42
                implicitHeight: 42
                radius: Design.radiusSm
                color: BeyGramTheme.accentSoft
                border.width: 1
                border.color: Qt.alpha(BeyGramTheme.accent, 0.24)
                opacity: root.hasCurrent ? 1.0 : 0.6
                antialiasing: true

                DsIcon {
                    anchors.centerIn: parent
                    name: "\uE8D6"
                    size: 20
                    color: BeyGramTheme.accentInk
                    filled: true
                }

                Behavior on opacity {
                    NumberAnimation {
                        duration: Design.motion(Design.motionFast,
                                                BeyGramAppearance.reduceMotion)
                        easing.type: Easing.OutCubic
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 80
                Layout.alignment: Qt.AlignVCenter
                spacing: 0

                Text {
                    Layout.fillWidth: true
                    text: root.displayTitle
                    textFormat: Text.PlainText
                    color: root.hasCurrent ? BeyGramTheme.text : BeyGramTheme.muted
                    font.pixelSize: Design.fontBody
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                    maximumLineCount: 1
                    wrapMode: Text.NoWrap

                    Accessible.role: Accessible.StaticText
                    Accessible.name: text
                }

                Text {
                    Layout.fillWidth: true
                    text: root.formatTime(root.positionMs) + " / "
                          + root.formatTime(root.durationMs)
                    textFormat: Text.PlainText
                    color: BeyGramTheme.muted
                    font.pixelSize: Design.fontCaption
                    elide: Text.ElideRight
                    maximumLineCount: 1
                    wrapMode: Text.NoWrap
                }
            }

            DsIconButton {
                Layout.alignment: Qt.AlignVCenter
                icon: "\uE892"
                size: "sm"
                tooltip: qsTr("Previous")
                enabled: root.hasFacade
                onClicked: {
                    if (root.hasFacade)
                        root.facade.previous()
                }
            }

            DsIconButton {
                Layout.alignment: Qt.AlignVCenter
                icon: root.playing ? "\uE769" : "\uE768"
                tone: "accent"
                size: "md"
                tooltip: root.playing ? qsTr("Pause") : qsTr("Play")
                enabled: root.hasFacade
                onClicked: {
                    if (root.hasFacade)
                        root.facade.playPause()
                }
            }

            DsIconButton {
                Layout.alignment: Qt.AlignVCenter
                icon: "\uE893"
                size: "sm"
                tooltip: qsTr("Next")
                enabled: root.hasFacade && root.hasNext
                onClicked: {
                    if (root.hasFacade)
                        root.facade.next()
                }
            }

            DsChip {
                Layout.alignment: Qt.AlignVCenter
                text: root.speedLabel(root.speed)
                selected: Math.abs(root.speed - 1.0) > 0.001
                enabled: root.hasFacade
                Accessible.description: qsTr("Playback speed")
                onClicked: {
                    if (root.hasFacade)
                        root.cycleSpeed()
                }
            }

            DsIconButton {
                Layout.alignment: Qt.AlignVCenter
                icon: "\uE8B1"
                size: "sm"
                selected: root.shuffle
                tooltip: qsTr("Shuffle")
                enabled: root.hasFacade
                onClicked: {
                    if (root.hasFacade)
                        root.facade.shuffle = !root.facade.shuffle
                }
            }

            DsIconButton {
                Layout.alignment: Qt.AlignVCenter
                icon: root.repeatMode === "one" ? "\uE8ED" : "\uE8EE"
                size: "sm"
                selected: root.repeatMode !== "off"
                tooltip: root.repeatTooltip()
                enabled: root.hasFacade
                onClicked: {
                    if (root.hasFacade)
                        root.facade.cycleRepeat()
                }
            }
        }

        DsSlider {
            id: seekSlider

            Layout.fillWidth: true
            Layout.preferredHeight: 18
            from: 0
            to: Math.max(1, root.durationMs)
            stepSize: 1000
            handleSize: 10
            trackThickness: 3
            enabled: root.hasFacade && root.durationMs > 0
            onMoved: {
                if (root.hasFacade)
                    root.facade.seek(Math.round(value))
            }

            Accessible.name: qsTr("Playback position")
        }
    }

    // Keep the seek handle under the pointer while dragging; resume following
    // the facade once the drag ends.
    Binding {
        target: seekSlider
        property: "value"
        value: root.positionMs
        when: !seekSlider.pressed
    }
}
