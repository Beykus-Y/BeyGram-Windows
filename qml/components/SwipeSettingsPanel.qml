// SwipeSettingsPanel - the chat-list swipe gesture editor.
//
// The parent owns the facade: it assigns `facade` (a SwipeSettingsFacade) and
// this panel only renders the pair and writes the user's choices back through
// setLeft()/setRight()/reset(). The facade owns the rule that the two
// directions never share an action, so the panel shows whatever it reports;
// picking the action the other direction already uses makes the two swap.
//
// `actions` is the facade's list of stored action names, not labels. The panel
// translates them for display and keeps the raw names for the facade, so a
// localized string is never sent back as a setting. A null facade is allowed
// and leaves the pickers disabled, so the panel can exist before an account is
// loaded.
//
// Layout comes from the design system: a DsSection with a DsCard body, a
// caption and a DsSegmented per direction, and a DsButton to reset.

import QtQuick
import QtQuick.Layouts
import BeyGram.Theme 1.0
import "../ds/design.js" as Design

Item {
    id: root

    /// The SwipeSettingsFacade this panel renders. Null shows the disabled form.
    property var facade: null

    readonly property bool hasFacade: root.facade ? true : false
    readonly property var actions: root.hasFacade ? root.facade.actions : []
    readonly property var actionLabels: {
        // Built with an explicit loop rather than Array.prototype.map: a
        // QStringList from C++ is a sequence type, and only indexing and
        // length are guaranteed on it in every Qt version this tree supports.
        var labels = []
        for (var i = 0; i < root.actions.length; ++i)
            labels.push(root.actionLabel(root.actions[i]))
        return labels
    }

    readonly property int leftIndex: root.hasFacade
                                       ? root.indexOfAction(root.facade.leftAction)
                                       : -1
    readonly property int rightIndex: root.hasFacade
                                        ? root.indexOfAction(root.facade.rightAction)
                                        : -1

    implicitWidth: 520
    implicitHeight: column.implicitHeight

    // The stored names are stable and lowercase, so the labels can change with
    // the interface language without touching what is saved.
    function actionLabel(action) {
        switch (String(action || "")) {
        case "none":
            return qsTr("Off")
        case "archive":
            return qsTr("Archive")
        case "mute":
            return qsTr("Mute")
        case "read":
            return qsTr("Mark as read")
        case "pin":
            return qsTr("Pin")
        case "delete":
            return qsTr("Delete")
        default:
            return String(action || "")
        }
    }

    function indexOfAction(action) {
        for (var i = 0; i < root.actions.length; ++i) {
            if (root.actions[i] === action)
                return i
        }
        return -1
    }

    ColumnLayout {
        id: column
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: Design.space4

        DsSection {
            Layout.fillWidth: true
            title: qsTr("Chat list swipes")
            description: qsTr("Choose what a horizontal swipe does on a chat. Picking the action the other direction already uses swaps the two.")

            content: DsCard {
                Layout.fillWidth: true
                padding: Design.space4

                content: ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Design.space3

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Swipe left")
                        color: BeyGramTheme.muted
                        font.pixelSize: Design.fontCaption
                        font.weight: Font.DemiBold
                    }

                    DsSegmented {
                        id: leftPicker
                        Layout.fillWidth: true
                        enabled: root.hasFacade
                        model: root.actionLabels
                        Accessible.name: qsTr("Action for swiping left")

                        onActivated: function (index) {
                            if (root.hasFacade && index >= 0 && index < root.actions.length)
                                root.facade.setLeft(root.actions[index])
                        }
                    }

                    DsDivider { Layout.fillWidth: true }

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Swipe right")
                        color: BeyGramTheme.muted
                        font.pixelSize: Design.fontCaption
                        font.weight: Font.DemiBold
                    }

                    DsSegmented {
                        id: rightPicker
                        Layout.fillWidth: true
                        enabled: root.hasFacade
                        model: root.actionLabels
                        Accessible.name: qsTr("Action for swiping right")

                        onActivated: function (index) {
                            if (root.hasFacade && index >= 0 && index < root.actions.length)
                                root.facade.setRight(root.actions[index])
                        }
                    }

                    DsDivider { Layout.fillWidth: true }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Design.space2

                        Item { Layout.fillWidth: true }

                        DsButton {
                            text: qsTr("Reset to defaults")
                            tone: "secondary"
                            size: "sm"
                            enabled: root.hasFacade
                            accessibleName: qsTr("Reset swipe actions to their defaults")

                            onClicked: {
                                if (root.hasFacade)
                                    root.facade.reset()
                            }
                        }
                    }
                }
            }
        }
    }

    // DsSegmented writes currentIndex itself when a segment is clicked, which
    // replaces a plain property binding. Binding elements survive that write,
    // so the facade stays the source of truth after a pick and after a swap
    // that moves both sides at once.
    Binding {
        target: leftPicker
        property: "currentIndex"
        value: root.leftIndex
        when: root.hasFacade
        restoreMode: Binding.RestoreNone
    }

    Binding {
        target: rightPicker
        property: "currentIndex"
        value: root.rightIndex
        when: root.hasFacade
        restoreMode: Binding.RestoreNone
    }
}
