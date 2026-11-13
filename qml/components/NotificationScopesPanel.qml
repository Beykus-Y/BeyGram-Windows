// NotificationScopesPanel - the notification-scope list and its editor.
//
// The parent owns the data layer: it assigns a NotificationScopesFacade to
// `facade`, publishes the scopes with facade.setScopes(), and sends whatever
// facade.saveRequested() announces. This file only renders. The list selects a
// scope; the switches below edit that scope's draft; Save hands the draft back
// to the facade, which emits only when something actually changed.
//
// Layout comes from the design system: one DsSection, one DsCard, DsListRow for
// each scope and each switch, DsSwitch for the three flags and DsBadge for the
// muted marker.

import QtQuick
import QtQuick.Layouts
import BeyGram.Theme 1.0
import "../ds/design.js" as Design

Item {
    id: root

    /// A NotificationScopesFacade, or null before the controller is available.
    property var facade: null

    implicitWidth: 520
    implicitHeight: content.implicitHeight

    readonly property bool ready: root.facade !== null && root.facade !== undefined
    readonly property var scopeIds: root.ready ? root.facade.ids : []
    readonly property var scopeTitles: root.ready ? root.facade.titles : []
    readonly property int scopeCount: root.scopeIds.length
    readonly property int selectedIndex: root.ready ? Number(root.facade.selectedIndex) : -1
    readonly property bool muted: root.ready ? Boolean(root.facade.muted) : false
    readonly property bool preview: root.ready ? Boolean(root.facade.preview) : true
    readonly property bool sound: root.ready ? Boolean(root.facade.sound) : true

    /// The stored title, or a readable stand-in when the payload carried none.
    /// Scope "0" is Telegram's account-wide default.
    function scopeTitle(index) {
        const title = index >= 0 && index < root.scopeTitles.length
                    ? String(root.scopeTitles[index] || "") : ""
        if (title.length > 0)
            return title

        const id = index >= 0 && index < root.scopeIds.length
                 ? String(root.scopeIds[index] || "") : ""
        return id === "0" ? qsTr("Default notifications")
                          : qsTr("Scope %1").arg(id)
    }

    /// The selected scope's draft in words. The facade exposes the flags of the
    /// selected row only, so this summary belongs to that row.
    function draftSummary() {
        if (root.selectedIndex < 0)
            return ""
        if (root.muted)
            return qsTr("Muted")

        const parts = [qsTr("Notifications on")]
        if (!root.preview)
            parts.push(qsTr("preview off"))
        if (!root.sound)
            parts.push(qsTr("sound off"))
        return parts.join(" - ")
    }

    ColumnLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        spacing: Design.space4

        DsSection {
            Layout.fillWidth: true
            title: qsTr("Notification scopes")
            description: qsTr("Choose a scope, then decide whether it is muted, shows message previews and plays a sound. Changes are sent when you save.")

            content: DsCard {
                Layout.fillWidth: true

                content: ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    DsListRow {
                        Layout.fillWidth: true
                        visible: root.scopeCount === 0
                        title: root.ready
                               ? qsTr("No notification scopes are loaded yet.")
                               : qsTr("Notification scopes are not available yet.")
                        muted: true
                        hoverEnabled: false
                    }

                    Repeater {
                        model: root.scopeCount

                        delegate: ColumnLayout {
                            id: scopeRow

                            required property int index

                            Layout.fillWidth: true
                            spacing: 0

                            DsListRow {
                                Layout.fillWidth: true
                                title: root.scopeTitle(scopeRow.index)
                                subtitle: scopeRow.index === root.selectedIndex
                                          ? root.draftSummary() : ""
                                selected: scopeRow.index === root.selectedIndex
                                onClicked: if (root.ready) root.facade.select(scopeRow.index)
                            }

                            DsDivider {
                                Layout.fillWidth: true
                                inset: Design.space3
                                visible: scopeRow.index < root.scopeCount - 1
                            }
                        }
                    }

                    DsDivider {
                        Layout.fillWidth: true
                        inset: Design.space3
                        visible: root.scopeCount > 0 && root.selectedIndex >= 0
                    }

                    // Editor for the selected scope. Preview and sound are
                    // disabled while the scope is muted, like the global
                    // notification settings; the facade keeps their stored
                    // values so unmuting restores them.
                    ColumnLayout {
                        id: editor

                        Layout.fillWidth: true
                        spacing: 0
                        visible: root.selectedIndex >= 0

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.leftMargin: Design.space3
                            Layout.rightMargin: Design.space3
                            Layout.topMargin: Design.space3
                            Layout.bottomMargin: Design.space1
                            spacing: Design.space2

                            Text {
                                Layout.fillWidth: true
                                text: root.selectedIndex >= 0
                                      ? qsTr("Settings for %1").arg(root.scopeTitle(root.selectedIndex))
                                      : ""
                                color: BeyGramTheme.text
                                font.pixelSize: Design.fontBodyLarge
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                            }

                            DsBadge {
                                Layout.alignment: Qt.AlignVCenter
                                visible: root.muted
                                text: qsTr("Muted")
                                tone: "muted"
                            }
                        }

                        DsListRow {
                            Layout.fillWidth: true
                            title: qsTr("Mute notifications")
                            subtitle: qsTr("No alerts for chats in this scope.")

                            trailing: DsSwitch {
                                id: muteSwitch

                                enabled: root.ready
                                Accessible.name: qsTr("Mute notifications for %1").arg(root.scopeTitle(root.selectedIndex))
                                onToggled: if (root.ready) root.facade.setMuted(muteSwitch.checked)
                            }
                        }

                        // A Binding element keeps the switch on the facade
                        // value even after the control writes `checked` itself.
                        Binding {
                            target: muteSwitch
                            property: "checked"
                            value: root.muted
                            restoreMode: Binding.RestoreNone
                        }

                        DsDivider {
                            Layout.fillWidth: true
                            inset: Design.space3
                        }

                        DsListRow {
                            Layout.fillWidth: true
                            title: qsTr("Show message preview")
                            subtitle: qsTr("Include the message text in the alert.")

                            trailing: DsSwitch {
                                id: previewSwitch

                                enabled: root.ready && !root.muted
                                Accessible.name: qsTr("Show message preview for %1").arg(root.scopeTitle(root.selectedIndex))
                                onToggled: if (root.ready) root.facade.setPreview(previewSwitch.checked)
                            }
                        }

                        Binding {
                            target: previewSwitch
                            property: "checked"
                            value: root.preview
                            restoreMode: Binding.RestoreNone
                        }

                        DsDivider {
                            Layout.fillWidth: true
                            inset: Design.space3
                        }

                        DsListRow {
                            Layout.fillWidth: true
                            title: qsTr("Play notification sound")
                            subtitle: qsTr("A short sound for each alert.")

                            trailing: DsSwitch {
                                id: soundSwitch

                                enabled: root.ready && !root.muted
                                Accessible.name: qsTr("Play notification sound for %1").arg(root.scopeTitle(root.selectedIndex))
                                onToggled: if (root.ready) root.facade.setSound(soundSwitch.checked)
                            }
                        }

                        Binding {
                            target: soundSwitch
                            property: "checked"
                            value: root.sound
                            restoreMode: Binding.RestoreNone
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.margins: Design.space3
                            spacing: Design.space2

                            Item { Layout.fillWidth: true }

                            // save() itself is a no-op while the draft is
                            // clean, so this button can stay enabled without
                            // ever sending the same scope twice.
                            DsButton {
                                text: qsTr("Save")
                                tone: "primary"
                                size: "sm"
                                enabled: root.ready && root.selectedIndex >= 0
                                accessibleName: qsTr("Save notification scope")
                                onClicked: if (root.ready) root.facade.save()
                            }
                        }
                    }
                }
            }
        }
    }
}
