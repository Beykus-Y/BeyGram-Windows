// CallOverlay - the call screen for one private call.

import QtQuick
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
            return qsTr("Incoming call")
        if (overlay.phase === "connecting")
            return qsTr("Calling…")
        if (overlay.phase === "active")
            return qsTr("In call")
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
            text: overlay.peer.length > 0 ? overlay.peer : qsTr("Unknown")
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
                text: qsTr("Mute")
                selected: overlay.facade !== null && overlay.facade.muted
                enabled: overlay.facade !== null && overlay.open
                onClicked: overlay.facade.setMuted(!overlay.facade.muted)
            }

            ChoiceChip {
                Layout.fillWidth: true
                text: qsTr("Speaker")
                selected: overlay.facade !== null && overlay.facade.speaker
                enabled: overlay.facade !== null && overlay.open
                onClicked: overlay.facade.setSpeaker(!overlay.facade.speaker)
            }

            ChoiceChip {
                Layout.fillWidth: true
                text: qsTr("Video")
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
                text: qsTr("Decline")
                visible: overlay.phase === "ringing"
                onClicked: overlay.facade.decline()
            }

            PrimaryButton {
                Layout.fillWidth: true
                tone: "ok"
                text: qsTr("Accept")
                visible: overlay.phase === "ringing"
                onClicked: overlay.facade.accept()
            }

            PrimaryButton {
                Layout.fillWidth: true
                tone: "danger"
                text: qsTr("End call")
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
