// SessionsPanel - the "active devices" section of the account settings.
//
// The parent owns the data layer: it assigns a SessionsFacade to `facade`, which
// exposes the terminable sessions as `devices` / `sessionIds`, the confirmation
// state, and the terminateRequested signal the parent forwards to TDLib. This
// file only renders the rows and their buttons; it performs no network, logging
// or C++ calls of its own. A row's "Terminate" button and the footer both open a
// confirmation in the facade, and the confirmation row sends it with confirm()
// or dismisses it with cancel().

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0

Card {
    id: panel

    property var facade: null

    readonly property var deviceRows: panel.facade ? panel.facade.devices : []
    readonly property var deviceIds: panel.facade ? panel.facade.sessionIds : []
    readonly property bool confirming: panel.facade ? panel.facade.confirming : false
    readonly property bool running: panel.facade ? panel.facade.running : false
    readonly property bool hasOthers: panel.deviceRows.length > 0

    spacing: 12

    // What the pending confirmation would sign out, in words. "all" is the
    // facade's marker for terminateAllOtherSessions; anything else is a session
    // id that is still on screen.
    function targetLabel() {
        if (!panel.facade)
            return ""
        const target = panel.facade.pendingTarget
        if (target === "all")
            return "all other sessions"
        const index = panel.deviceIds.indexOf(target)
        return index >= 0 ? panel.deviceRows[index] : "this session"
    }

    Text {
        text: "Active devices"
        color: BeyGramTheme.text
        font.pixelSize: 15
        font.weight: Font.DemiBold
    }

    Text {
        Layout.fillWidth: true
        text: panel.hasOthers
              ? "These devices are signed in to your account. Terminating one signs it out."
              : "No other devices are signed in."
        color: BeyGramTheme.muted
        font.pixelSize: 12
        wrapMode: Text.WordWrap
    }

    Repeater {
        model: panel.deviceRows

        delegate: RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Rectangle {
                width: 8
                height: 8
                radius: 4
                color: BeyGramTheme.muted
                Layout.alignment: Qt.AlignVCenter
            }

            Text {
                Layout.fillWidth: true
                text: modelData
                color: BeyGramTheme.text
                font.pixelSize: 13
                elide: Text.ElideRight
            }

            SecondaryButton {
                text: "Terminate"
                enabled: panel.facade !== null && !panel.running && !panel.confirming
                onClicked: panel.facade.requestTerminate(panel.deviceIds[index])
            }
        }
    }

    Rectangle {
        Layout.fillWidth: true
        height: 1
        visible: panel.hasOthers
        color: BeyGramTheme.line
    }

    SecondaryButton {
        Layout.fillWidth: true
        text: "Terminate all other sessions"
        enabled: panel.facade !== null && panel.hasOthers && !panel.running && !panel.confirming
        onClicked: panel.facade.requestTerminateAllOthers()
    }

    // Confirmation row: one target at a time, with the action frozen while it is
    // in flight so the same termination cannot be sent twice.
    Rectangle {
        Layout.fillWidth: true
        visible: panel.confirming
        implicitHeight: confirmRow.implicitHeight + 24
        radius: 12
        color: Qt.alpha(BeyGramTheme.danger, BeyGramTheme.darkMode ? 0.16 : 0.10)
        border.width: 1
        border.color: Qt.alpha(BeyGramTheme.danger, 0.45)

        RowLayout {
            id: confirmRow
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.margins: 12
            spacing: 10

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Text {
                    Layout.fillWidth: true
                    text: panel.running
                          ? "Signing out " + panel.targetLabel() + "..."
                          : "Sign out " + panel.targetLabel() + "?"
                    color: BeyGramTheme.text
                    font.pixelSize: 13
                    font.weight: Font.Medium
                    elide: Text.ElideRight
                }

                Text {
                    Layout.fillWidth: true
                    text: "That device will need to sign in again."
                    color: BeyGramTheme.muted
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                }
            }

            PrimaryButton {
                text: panel.running ? "Working..." : "Confirm"
                tone: "danger"
                enabled: !panel.running
                onClicked: panel.facade.confirm()
            }

            SecondaryButton {
                text: "Cancel"
                enabled: !panel.running
                onClicked: panel.facade.cancel()
            }
        }
    }
}
