// CallOverlay - the call screen for one private call.
//
// The host owns the call: it assigns `facade` (a CallsFacade) and keeps it
// alive, and the facade is the only thing this file talks to. Nothing here
// sends a request; the buttons call the facade's invokables and the facade
// turns them into answerRequested()/endRequested() for the transport owner.
//
// The overlay is visible exactly while facade.active is true, so a call that
// ended closes the screen on its own. The timer reads facade.elapsed, which the
// owner pushes as the call runs; this file never starts a clock of its own, so
// it cannot drift from the call.
//
// The video chip is a state, not a switch: a call's media type is fixed when it
// starts, and CallsFacade exposes video as a read-only fact, so the chip only
// reports whether this call carries video.

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0

Item {
    id: overlay

    property var facade: null

    readonly property bool open: overlay.facade !== null && overlay.facade.active
    readonly property string phase: overlay.facade !== null ? overlay.facade.phase : "idle"
    readonly property string peer: overlay.facade !== null ? overlay.facade.peerName : ""

    anchors.fill: parent
    visible: open
    z: 1100
    focus: visible

    // The overlay takes the keyboard when it appears so Escape declines or ends
    // the call, and gives it back when the call is over.
    onVisibleChanged: {
        if (visible)
            overlay.forceActiveFocus()
    }

    function clock(seconds) {
        const total = Math.max(0, Math.floor(seconds))
        const minutes = Math.floor(total / 60)
        const rest = total % 60
        return (minutes < 10 ? "0" : "") + minutes + ":"
             + (rest < 10 ? "0" : "") + rest
    }

    function phaseLabel() {
        if (overlay.phase === "ringing")
            return "Incoming call"
        if (overlay.phase === "connecting")
            return "Calling..."
        if (overlay.phase === "active")
            return "In call"
        return ""
    }

    function initials() {
        const name = overlay.peer.trim()
        if (name.length === 0)
            return "?"
        return name.charAt(0).toUpperCase()
    }

    Keys.onEscapePressed: {
        if (overlay.facade === null)
            return
        if (overlay.phase === "ringing")
            overlay.facade.decline()
        else
            overlay.facade.end()
    }

    Rectangle {
        anchors.fill: parent
        color: Qt.alpha("#000000", 0.55)
    }

    // Swallows clicks so the page behind the call cannot be used mid-call.
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        onWheel: function(wheel) { wheel.accepted = true }
    }

    Card {
        anchors.centerIn: parent
        width: 340
        padding: 22
        spacing: 14

        Avatar {
            Layout.alignment: Qt.AlignHCenter
            size: 64
            initials: overlay.initials()
        }

        Text {
            Layout.fillWidth: true
            text: overlay.peer.length > 0 ? overlay.peer : "Unknown"
            color: BeyGramTheme.text
            font.pixelSize: 18
            font.weight: Font.DemiBold
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
        }

        Text {
            Layout.fillWidth: true
            text: overlay.phaseLabel()
            color: BeyGramTheme.muted
            font.pixelSize: 12
            horizontalAlignment: Text.AlignHCenter
        }

        Text {
            Layout.fillWidth: true
            text: overlay.clock(overlay.facade !== null ? overlay.facade.elapsed : 0)
            color: BeyGramTheme.text
            font.pixelSize: 30
            font.weight: Font.Light
            font.letterSpacing: 1
            horizontalAlignment: Text.AlignHCenter
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignHCenter
            spacing: 8

            ChoiceChip {
                Layout.fillWidth: true
                text: "Mute"
                selected: overlay.facade !== null && overlay.facade.muted
                enabled: overlay.facade !== null && overlay.open
                onClicked: overlay.facade.setMuted(!overlay.facade.muted)
            }

            ChoiceChip {
                Layout.fillWidth: true
                text: "Speaker"
                selected: overlay.facade !== null && overlay.facade.speaker
                enabled: overlay.facade !== null && overlay.open
                onClicked: overlay.facade.setSpeaker(!overlay.facade.speaker)
            }

            // Read-only state chip: the media type is fixed when the call starts.
            ChoiceChip {
                Layout.fillWidth: true
                text: "Video"
                selected: overlay.facade !== null && overlay.facade.video
                enabled: false
                opacity: 0.7
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            PrimaryButton {
                Layout.fillWidth: true
                tone: "danger"
                text: "Decline"
                visible: overlay.phase === "ringing"
                onClicked: overlay.facade.decline()
            }

            PrimaryButton {
                Layout.fillWidth: true
                tone: "ok"
                text: "Accept"
                visible: overlay.phase === "ringing"
                onClicked: overlay.facade.accept()
            }

            PrimaryButton {
                Layout.fillWidth: true
                tone: "danger"
                text: "End call"
                visible: overlay.phase === "connecting" || overlay.phase === "active"
                onClicked: overlay.facade.end()
            }
        }
    }

    Connections {
        target: overlay.facade

        function onChanged() {
            if (overlay.open)
                overlay.forceActiveFocus()
        }
    }
}
