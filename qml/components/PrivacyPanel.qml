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
// being edited appears in the chip row below it.

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0

Item {
    id: root

    /// The PrivacyFacade this panel renders. Null is allowed and shows the empty
    /// state, so a panel can exist before the account is loaded.
    property var facade: null

    implicitWidth: 520
    implicitHeight: column.implicitHeight

    readonly property bool hasFacade: root.facade ? true : false

    /// The four audiences the facade accepts, in the order the buttons show them.
    readonly property var choices: [
        { value: "everyone", label: "Everyone" },
        { value: "contacts", label: "Contacts" },
        { value: "closeFriends", label: "Close friends" },
        { value: "nobody", label: "Nobody" }
    ]

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

    ColumnLayout {
        id: column
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        spacing: 12

        Text {
            Layout.fillWidth: true
            text: "Privacy"
            color: BeyGramTheme.text
            font.pixelSize: 18
            font.weight: Font.DemiBold
        }

        Text {
            Layout.fillWidth: true
            text: "Choose who can see your information and who can contact you. Changes are sent to Telegram one setting at a time."
            color: BeyGramTheme.muted
            font.pixelSize: 12
            wrapMode: Text.WordWrap
            lineHeight: 1.3
        }

        Card {
            Layout.fillWidth: true
            spacing: 10

            Text {
                Layout.fillWidth: true
                visible: root.names.length === 0
                text: "No privacy settings loaded yet."
                color: BeyGramTheme.muted
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }

            Repeater {
                model: root.names.length

                delegate: ColumnLayout {
                    id: settingRow

                    required property int index

                    Layout.fillWidth: true
                    spacing: 10

                    Rectangle {
                        Layout.fillWidth: true
                        visible: settingRow.index > 0
                        height: 1
                        color: BeyGramTheme.line
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2

                            Text {
                                Layout.fillWidth: true
                                text: root.settingLabel(root.names[settingRow.index])
                                color: BeyGramTheme.text
                                font.pixelSize: 13
                                elide: Text.ElideRight
                            }

                            Text {
                                Layout.fillWidth: true
                                text: root.valueLabel(root.currentValue(settingRow.index))
                                color: BeyGramTheme.muted
                                font.pixelSize: 11
                                elide: Text.ElideRight
                            }
                        }

                        SecondaryButton {
                            visible: !root.isEditing(settingRow.index)
                            text: "Change"
                            Accessible.name: "Change " + root.settingLabel(root.names[settingRow.index])
                            onClicked: {
                                if (root.hasFacade)
                                    root.facade.beginEdit(settingRow.index)
                            }
                        }
                    }

                    // Inline editor: the four audiences plus Save / Cancel. Save
                    // stays disabled until the facade reports a real change, so
                    // the panel cannot send a value the user already has.
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        visible: root.isEditing(settingRow.index)

                        Flow {
                            Layout.fillWidth: true
                            spacing: 8

                            Repeater {
                                model: root.choices

                                delegate: ChoiceChip {
                                    required property var modelData

                                    text: modelData.label
                                    selected: root.hasFacade
                                              && root.facade.editingValue === modelData.value
                                    onClicked: {
                                        if (root.hasFacade)
                                            root.facade.setEditingValue(modelData.value)
                                    }
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Item { Layout.fillWidth: true }

                            SecondaryButton {
                                text: "Cancel"
                                onClicked: {
                                    if (root.hasFacade)
                                        root.facade.cancelEdit()
                                }
                            }

                            PrimaryButton {
                                text: "Save"
                                enabled: root.hasFacade && root.facade.canSave
                                onClicked: {
                                    if (root.hasFacade)
                                        root.facade.markSaved()
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
