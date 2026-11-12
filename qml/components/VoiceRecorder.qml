// VoiceRecorder — the recording bar that replaces the composer while a voice
// message is being recorded. Presentation only: every control drives the
// injected `facade` (VoiceComposerFacade) and the parent reacts to
// facade.sendRequested / facade.cancelRequested to actually send or discard the
// clip. No audio, timer or network state lives here.

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0

Item {
    id: root

    /// VoiceComposerFacade instance. The bar hides itself while the facade is
    /// not recording.
    property var facade: null

    readonly property bool _recording: facade ? facade.recording : false
    readonly property bool _paused: facade ? facade.paused : false
    readonly property bool _locked: facade ? facade.locked : false
    readonly property bool _canSend: _recording && facade.canSend
    readonly property string _elapsedText: facade ? facade.elapsedText : "0:00"
    readonly property var _waveform: facade ? facade.waveform : []

    // Bars shrink to fit the strip so a long envelope never overflows it.
    readonly property int _barGap: 2
    readonly property real _barWidth: _waveform.length > 0
        ? Math.max(1, Math.min(4,
            (waveformStrip.width - (_waveform.length - 1) * _barGap) / _waveform.length))
        : 3

    height: 52
    implicitHeight: 52
    visible: _recording
    clip: true

    Rectangle {
        anchors.fill: parent
        color: BeyGramTheme.surface2

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 1
            color: BeyGramTheme.line
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 14
        anchors.rightMargin: 10
        spacing: 10

        // Recording dot: blinks while live, holds a dim steady dot while paused.
        Rectangle {
            id: dot
            Layout.alignment: Qt.AlignVCenter
            Layout.preferredWidth: 8
            Layout.preferredHeight: 8
            radius: 4
            color: BeyGramTheme.danger

            SequentialAnimation on opacity {
                running: root._recording && !root._paused
                loops: Animation.Infinite

                NumberAnimation { to: 0.25; duration: 600; easing.type: Easing.InOutQuad }
                NumberAnimation { to: 1.0; duration: 600; easing.type: Easing.InOutQuad }

                onStopped: dot.opacity = root._paused ? 0.4 : 1.0
            }
        }

        Text {
            id: timer
            Layout.alignment: Qt.AlignVCenter
            Layout.preferredWidth: 46
            text: root._elapsedText
            color: root._paused ? BeyGramTheme.muted : BeyGramTheme.text
            font.pixelSize: 13
            font.weight: Font.Medium
            horizontalAlignment: Text.AlignLeft
            verticalAlignment: Text.AlignVCenter
        }

        // Live envelope. The faint baseline stands in until samples arrive.
        Item {
            id: waveformStrip
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            Layout.minimumWidth: 40
            Layout.preferredHeight: 26
            clip: true

            Row {
                id: bars
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                spacing: root._barGap

                Repeater {
                    model: root._waveform

                    delegate: Rectangle {
                        required property string modelData

                        width: root._barWidth
                        height: Math.max(2, Math.round(Number(modelData) * waveformStrip.height))
                        radius: width / 2
                        color: BeyGramTheme.accent
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                height: 1
                color: BeyGramTheme.line
                visible: root._waveform.length === 0
            }
        }

        Row {
            id: actions
            Layout.alignment: Qt.AlignVCenter
            spacing: 6

            ToolButton {
                id: pauseButton
                implicitWidth: 30
                implicitHeight: 30
                padding: 0
                focusPolicy: Qt.NoFocus
                enabled: root._recording
                onClicked: root.facade.setPaused(!root.facade.paused)

                background: Rectangle {
                    radius: 15
                    color: pauseButton.hovered
                        ? Qt.alpha(BeyGramTheme.accentInk, 0.12)
                        : "transparent"
                }

                contentItem: Text {
                    text: root._paused ? "\u25B6" : "\u23F8"
                    color: BeyGramTheme.text
                    font.pixelSize: 13
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                ToolTip.visible: hovered
                ToolTip.text: root._paused ? qsTr("Resume recording") : qsTr("Pause recording")
                ToolTip.delay: 600
            }

            ToolButton {
                id: lockButton
                implicitWidth: lockLabel.implicitWidth + 14
                implicitHeight: 22
                padding: 0
                focusPolicy: Qt.NoFocus
                enabled: root._recording
                onClicked: root.facade.setLocked(!root.facade.locked)

                background: Rectangle {
                    radius: 11
                    color: root._locked
                        ? BeyGramTheme.accentSoft
                        : (lockButton.hovered
                            ? Qt.alpha(BeyGramTheme.accentInk, 0.12)
                            : "transparent")
                }

                contentItem: Text {
                    id: lockLabel
                    text: root._locked ? qsTr("LOCKED") : qsTr("LOCK")
                    color: root._locked ? BeyGramTheme.accentInk : BeyGramTheme.muted
                    font.pixelSize: 9
                    font.weight: Font.DemiBold
                    font.letterSpacing: 0.6
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                ToolTip.visible: hovered
                ToolTip.text: root._locked
                    ? qsTr("Recording stays on when you let go")
                    : qsTr("Lock recording")
                ToolTip.delay: 600
            }

            ToolButton {
                id: cancelButton
                implicitWidth: 30
                implicitHeight: 30
                padding: 0
                focusPolicy: Qt.NoFocus
                enabled: root._recording
                onClicked: root.facade.cancel()

                background: Rectangle {
                    radius: 15
                    color: cancelButton.hovered
                        ? Qt.alpha(BeyGramTheme.danger, 0.14)
                        : "transparent"
                }

                contentItem: Text {
                    text: "\u2715"
                    color: cancelButton.hovered ? BeyGramTheme.danger : BeyGramTheme.muted
                    font.pixelSize: 12
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                ToolTip.visible: hovered
                ToolTip.text: qsTr("Cancel recording")
                ToolTip.delay: 600
            }

            ToolButton {
                id: sendButton
                implicitWidth: sendLabel.implicitWidth + 20
                implicitHeight: 26
                padding: 0
                focusPolicy: Qt.NoFocus
                enabled: root._canSend
                opacity: enabled ? 1.0 : 0.45
                onClicked: root.facade.finish()

                background: Rectangle {
                    radius: 13
                    color: sendButton.hovered && sendButton.enabled
                        ? Qt.lighter(BeyGramTheme.accentInk, 1.15)
                        : BeyGramTheme.accentInk
                }

                contentItem: Text {
                    id: sendLabel
                    text: qsTr("SEND")
                    color: BeyGramTheme.onAccent
                    font.pixelSize: 9
                    font.weight: Font.DemiBold
                    font.letterSpacing: 0.6
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                ToolTip.visible: hovered
                ToolTip.text: root._canSend
                    ? qsTr("Send voice message")
                    : qsTr("Record between one and fifty-nine seconds")
                ToolTip.delay: 600
            }
        }
    }
}
