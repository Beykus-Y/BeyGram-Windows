// PrivacyPanel - the privacy settings list with one inline audience editor.
//
// The parent owns the facade: it assigns `facade` (a PrivacyFacade), loads the
// account's settings through facade.setSettings() and sends whatever
// facade.saveRequested() announces. This file only renders: it has no network,
// no logging and no state of its own beyond what the facade already holds, so
// closing and reopening the panel cannot lose or invent an edit.
//
// The TDLib setting names are the payload of saveRequested(), not something a
// person should have to read, so the list shows a prettified label
// ("userPrivacySettingShowStatus" -> "Show status") and keeps the raw name out
// of the way. The value column always shows the confirmed audience; the draft
// being edited appears in the segmented control below it.
//
// Layout comes from the design system: a DsSection per group, a DsCard for the
// list, DsListRow for each setting and DsSegmented for the audience.

import QtQuick
import QtQuick.Layouts
import BeyGram.Theme 1.0
import "../ds/design.js" as Design

Item {
    id: root

    /// The PrivacyFacade this panel renders. Null is allowed and shows the empty
    /// state, so a panel can exist before the account is loaded.
    property var facade: null

    implicitWidth: 520
    implicitHeight: column.implicitHeight

    readonly property bool hasFacade: root.facade ? true : false

    /// The four audiences the facade accepts, in the order the control shows them.
    readonly property var choices: [
        { value: "everyone", label: qsTr("Everyone") },
        { value: "contacts", label: qsTr("Contacts") },
        { value: "closeFriends", label: qsTr("Close friends") },
        { value: "nobody", label: qsTr("Nobody") }
    ]

    /// The labels alone, which is the shape DsSegmented takes as its model.
    readonly property var choiceLabels: root.choices.map(function (choice) {
        return choice.label
    })

    readonly property var names: root.hasFacade ? root.facade.settingNames : []

    // "userPrivacySettingShowStatus" -> "Show status".
    function settingLabel(name) {
        var text = String(name || "")
        var prefix = "userPrivacySetting"
        if (text.indexOf(prefix) === 0)
            text = text.substring(prefix.length)
        text = text.replace(/([a-z0-9])([A-Z])/g, "$1 $2")
        if (text.length === 0)
            return String(name || "")
        return text.charAt(0).toUpperCase() + text.substring(1).toLowerCase()
    }

    function valueLabel(value) {
        var key = String(value || "")
        for (var i = 0; i < root.choices.length; ++i) {
            if (root.choices[i].value === key)
                return root.choices[i].label
        }
        return key
    }

    function currentValue(index) {
        if (!root.hasFacade)
            return ""
        var values = root.facade.settingValues
        return index >= 0 && index < values.length ? values[index] : ""
    }

    function isEditing(index) {
        return root.hasFacade && root.facade.editingIndex === index
    }

    /// Position of the draft audience in `choices`, for the segmented control.
    function editingChoiceIndex() {
        if (!root.hasFacade)
            return 0
        var value = root.facade.editingValue
        for (var i = 0; i < root.choices.length; ++i) {
            if (root.choices[i].value === value)
                return i
        }
        return 0
    }

    ColumnLayout {
        id: column
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        spacing: Design.space4

        DsSection {
            Layout.fillWidth: true
            title: qsTr("Privacy")
            description: qsTr("Choose who can see your information and who can contact you. Changes are sent to Telegram one setting at a time.")

            content: DsCard {
                Layout.fillWidth: true

                content: ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    DsListRow {
                        Layout.fillWidth: true
                        visible: root.names.length === 0
                        title: qsTr("No privacy settings loaded yet.")
                        muted: true
                        hoverEnabled: false
                    }

                    Repeater {
                        model: root.names.length

                        delegate: ColumnLayout {
                            id: settingRow

                            required property int index

                            Layout.fillWidth: true
                            spacing: Design.space2

                            DsListRow {
                                Layout.fillWidth: true
                                title: root.settingLabel(root.names[settingRow.index])
                                subtitle: root.valueLabel(root.currentValue(settingRow.index))

                                trailing: DsButton {
                                    visible: !root.isEditing(settingRow.index)
                                    text: qsTr("Change")
                                    tone: "secondary"
                                    size: "sm"
                                    accessibleName: qsTr("Change %1").arg(root.settingLabel(root.names[settingRow.index]))
                                    onClicked: {
                                        if (root.hasFacade)
                                            root.facade.beginEdit(settingRow.index)
                                    }
                                }
                            }

                            // Inline editor: the four audiences plus Save / Cancel.
                            // Save stays disabled until the facade reports a real
                            // change, so the panel cannot send a value the user
                            // already has.
                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.leftMargin: Design.space3
                                Layout.rightMargin: Design.space3
                                Layout.bottomMargin: Design.space3
                                spacing: Design.space2
                                visible: root.isEditing(settingRow.index)

                                DsSegmented {
                                    id: audienceSegment

                                    Layout.fillWidth: true
                                    model: root.choiceLabels

                                    onActivated: function (index) {
                                        if (root.hasFacade)
                                            root.facade.setEditingValue(root.choices[index].value)
                                    }
                                }

                                // The control writes currentIndex itself on a
                                // click, so a Binding element (which survives
                                // that write) keeps it in step with the facade.
                                Binding {
                                    target: audienceSegment
                                    property: "currentIndex"
                                    value: root.editingChoiceIndex()
                                    restoreMode: Binding.RestoreNone
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: Design.space2

                                    Item { Layout.fillWidth: true }

                                    DsButton {
                                        text: qsTr("Cancel")
                                        tone: "secondary"
                                        size: "sm"
                                        onClicked: {
                                            if (root.hasFacade)
                                                root.facade.cancelEdit()
                                        }
                                    }

                                    DsButton {
                                        text: qsTr("Save")
                                        tone: "primary"
                                        size: "sm"
                                        enabled: root.hasFacade && root.facade.canSave
                                        onClicked: {
                                            if (root.hasFacade)
                                                root.facade.markSaved()
                                        }
                                    }
                                }
                            }

                            DsDivider {
                                Layout.fillWidth: true
                                Layout.topMargin: Design.space1
                                inset: Design.space3
                                visible: settingRow.index < root.names.length - 1
                            }
                        }
                    }
                }
            }
        }
    }
}
