// SessionsPanel - the "active devices" section of the account settings.
//
// The parent owns the data layer: it assigns a SessionsFacade to `facade`, which
// exposes the terminable sessions as `devices` / `sessionIds`, the confirmation
// state, and the terminateRequested signal the parent forwards to TDLib. This
// file only renders the rows and their buttons; it performs no network, logging
// or C++ calls of its own. A row's "Terminate" button and the footer both open a
// confirmation in the facade, and the DsDialog sends it with confirm() or
// dismisses it with cancel().
//
// Layout comes from the design system: one DsSection, one DsCard, DsListRow per
// device and DsDialog for the destructive confirmation.

import QtQuick
import QtQuick.Layouts
import BeyGram.Theme 1.0
import "../ds/design.js" as Design

Item {
    id: panel

    property var facade: null

    implicitWidth: 520
    implicitHeight: content.implicitHeight

    readonly property var deviceRows: panel.facade ? panel.facade.devices : []
    readonly property var deviceIds: panel.facade ? panel.facade.sessionIds : []
    readonly property bool confirming: panel.facade ? panel.facade.confirming : false
    readonly property bool running: panel.facade ? panel.facade.running : false
    readonly property bool hasOthers: panel.deviceRows.length > 0

    // What the pending confirmation would sign out, in words. "all" is the
    // facade's marker for terminateAllOtherSessions; anything else is a session
    // id that is still on screen.
    function targetLabel() {
        if (!panel.facade)
            return ""
        const target = panel.facade.pendingTarget
        if (target === "all")
            return qsTr("all other sessions")
        const index = panel.deviceIds.indexOf(target)
        return index >= 0 ? panel.deviceRows[index] : qsTr("this session")
    }

    ColumnLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        spacing: Design.space4

        DsSection {
            Layout.fillWidth: true
            title: qsTr("Active devices")
            description: qsTr("Every device signed in to your account appears here. Terminating one signs it out.")

            content: DsCard {
                Layout.fillWidth: true

                content: ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    DsListRow {
                        Layout.fillWidth: true
                        visible: !panel.hasOthers
                        title: qsTr("No other devices are signed in.")
                        muted: true
                        hoverEnabled: false
                    }

                    Repeater {
                        model: panel.deviceRows

                        delegate: ColumnLayout {
                            id: deviceRow

                            required property int index
                            required property string modelData

                            Layout.fillWidth: true
                            spacing: 0

                            DsListRow {
                                Layout.fillWidth: true
                                title: deviceRow.modelData

                                trailing: DsButton {
                                    text: qsTr("Terminate")
                                    tone: "danger"
                                    size: "sm"
                                    enabled: panel.facade !== null && !panel.running && !panel.confirming
                                    onClicked: panel.facade.requestTerminate(panel.deviceIds[deviceRow.index])
                                }
                            }

                            DsDivider {
                                Layout.fillWidth: true
                                inset: Design.space3
                                visible: deviceRow.index < panel.deviceRows.length - 1
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.margins: Design.space3
                        spacing: Design.space2
                        visible: panel.hasOthers

                        Item { Layout.fillWidth: true }

                        DsButton {
                            text: qsTr("Terminate all other sessions")
                            tone: "danger"
                            size: "sm"
                            enabled: panel.facade !== null && panel.hasOthers
                                     && !panel.running && !panel.confirming
                            onClicked: panel.facade.requestTerminateAllOthers()
                        }
                    }
                }
            }
        }
    }

    // Confirmation: one target at a time. While the facade reports the action as
    // running the dialog stays open and busy, so the same termination cannot be
    // sent twice; the caller's cancel() clears `confirming` and closes it.
    DsDialog {
        id: confirmDialog

        title: panel.running
               ? qsTr("Signing out %1...").arg(panel.targetLabel())
               : qsTr("Sign out %1?").arg(panel.targetLabel())
        description: qsTr("That device will need to sign in again.")
        primaryText: panel.running ? qsTr("Working...") : qsTr("Sign out")
        secondaryText: qsTr("Cancel")
        tone: "danger"
        busy: panel.running

        onAccepted: {
            if (panel.facade)
                panel.facade.confirm()
        }
        onRejected: {
            if (panel.facade)
                panel.facade.cancel()
        }
    }

    // The facade owns `confirming`; the dialog mirrors it. A Binding element is
    // used because the dialog closes itself through its own `open` property.
    Binding {
        target: confirmDialog
        property: "visible"
        value: panel.confirming
        restoreMode: Binding.RestoreNone
    }
}
