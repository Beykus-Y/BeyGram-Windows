pragma ComponentBehavior: Bound

// GroupAdminPanel - the administrator list and the permission switches for the
// selected member.
//
// The parent owns the facade: it assigns `facade` (a GroupAdminFacade), loads
// the administrators through facade.setMembers(ids, names, statuses) and sends
// whatever facade.rightsSaveRequested() announces. This file only renders the
// rows and calls the facade; it performs no network, logging or C++ calls of
// its own.
//
// The facade reports the owner as read-only, so the switches and Save are
// disabled for that row and the footer explains why. Selecting another member
// starts a fresh draft in the facade, which is why the panel never keeps a
// second copy of the rights.
//
// Layout comes from the design system: a DsSection per group, a DsCard for each
// list, DsListRow for members and rights, and DsSwitch for each permission.

import QtQuick
import QtQuick.Layouts
import BeyGram.Theme 1.0
import "../ds/design.js" as Design

Item {
    id: root

    /// The GroupAdminFacade this panel renders. Null is allowed and shows the
    /// empty state, so the panel can exist before the account is loaded.
    property var facade: null

    implicitWidth: 520
    implicitHeight: content.implicitHeight

    readonly property bool hasFacade: root.facade ? true : false
    readonly property var ids: root.hasFacade ? root.facade.memberIds : []
    readonly property var names: root.hasFacade ? root.facade.memberNames : []
    readonly property var statuses: root.hasFacade ? root.facade.memberStatuses : []
    readonly property int selectedIndex: root.hasFacade ? root.facade.selectedIndex : -1
    readonly property bool hasSelection: root.selectedIndex >= 0
                                          && root.selectedIndex < root.ids.length
    readonly property bool editable: root.hasFacade && root.facade.selectedEditable

    /// The label a member row shows: the name the caller supplied, or the id
    /// when the name is empty (the parser leaves names empty when it has no
    /// user object to resolve).
    function displayName(index) {
        if (index < 0 || index >= root.ids.length)
            return ""
        const name = String(root.names[index] || "").trim()
        return name.length > 0 ? name : String(root.ids[index] || "")
    }

    function initials(name) {
        const text = String(name || "").trim()
        if (text.length === 0)
            return "?"
        const parts = text.split(/\s+/)
        if (parts.length === 1)
            return parts[0].substring(0, 2).toUpperCase()
        return (parts[0].charAt(0) + parts[parts.length - 1].charAt(0)).toUpperCase()
    }

    function statusAt(index) {
        if (index < 0 || index >= root.statuses.length)
            return ""
        return String(root.statuses[index] || "")
    }

    function statusLabel(status) {
        return String(status || "").trim().toLowerCase() === "creator"
                ? qsTr("Owner") : qsTr("Administrator")
    }

    // One permission row: the title and explanation on the left, the switch on
    // the right. The Binding keeps the switch in step with the facade after a
    // click, because assigning `checked` directly would replace the binding.
    component PermissionRow: DsListRow {
        id: permission

        property string rightName: ""
        property bool rightValue: false
        property bool switchEnabled: true

        signal rightToggled(bool value)

        hoverEnabled: false

        trailing: DsSwitch {
            id: toggle

            enabled: permission.switchEnabled
            onToggled: permission.rightToggled(checked)
            Accessible.name: permission.title
            Accessible.description: permission.subtitle

            Binding {
                target: toggle
                property: "checked"
                value: permission.rightValue
                restoreMode: Binding.RestoreNone
            }
        }
    }

    ColumnLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        spacing: Design.space4

        DsSection {
            Layout.fillWidth: true
            title: qsTr("Administrators")
            description: qsTr("Choose an administrator to review or change what they can do in this group.")

            content: DsCard {
                Layout.fillWidth: true

                content: ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    DsListRow {
                        Layout.fillWidth: true
                        visible: root.ids.length === 0
                        title: qsTr("No administrators loaded yet.")
                        muted: true
                        hoverEnabled: false
                    }

                    Repeater {
                        model: root.ids.length

                        delegate: ColumnLayout {
                            id: memberRow

                            required property int index

                            Layout.fillWidth: true
                            spacing: 0

                            DsListRow {
                                Layout.fillWidth: true
                                selected: root.selectedIndex === memberRow.index
                                title: root.displayName(memberRow.index)
                                subtitle: root.statusLabel(root.statusAt(memberRow.index))

                                leading: DsAvatar {
                                    initials: root.initials(root.displayName(memberRow.index))
                                    accentIndex: memberRow.index
                                    size: Design.avatarSmall
                                }

                                onClicked: {
                                    if (root.hasFacade)
                                        root.facade.selectMember(memberRow.index)
                                }
                            }

                            DsDivider {
                                Layout.fillWidth: true
                                inset: Design.space3
                                visible: memberRow.index < root.ids.length - 1
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.margins: Design.space3
                        spacing: Design.space2
                        visible: root.ids.length > 0 && !root.hasSelection

                        Text {
                            Layout.fillWidth: true
                            text: qsTr("Select an administrator to edit permissions.")
                            color: BeyGramTheme.muted
                            font.pixelSize: Design.fontSmall
                            wrapMode: Text.WordWrap
                        }
                    }
                }
            }
        }

        DsSection {
            Layout.fillWidth: true
            visible: root.hasSelection
            title: qsTr("Permissions")
            description: root.hasSelection
                         ? qsTr("Choose what %1 can do in this group.").arg(root.displayName(root.selectedIndex))
                         : ""

            content: DsCard {
                Layout.fillWidth: true

                content: ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    PermissionRow {
                        id: changeInfoRow

                        Layout.fillWidth: true
                        rightName: "canChangeInfo"
                        rightValue: root.hasFacade ? root.facade.canChangeInfo : false
                        switchEnabled: root.editable
                        title: qsTr("Change group info")
                        subtitle: qsTr("Edit the group name, photo and description.")

                        onRightToggled: function (value) {
                            if (root.hasFacade)
                                root.facade.setRight(changeInfoRow.rightName, value)
                        }
                    }

                    DsDivider {
                        Layout.fillWidth: true
                        inset: Design.space3
                    }

                    PermissionRow {
                        id: deleteMessagesRow

                        Layout.fillWidth: true
                        rightName: "canDeleteMessages"
                        rightValue: root.hasFacade ? root.facade.canDeleteMessages : false
                        switchEnabled: root.editable
                        title: qsTr("Delete messages")
                        subtitle: qsTr("Delete messages sent by other members.")

                        onRightToggled: function (value) {
                            if (root.hasFacade)
                                root.facade.setRight(deleteMessagesRow.rightName, value)
                        }
                    }

                    DsDivider {
                        Layout.fillWidth: true
                        inset: Design.space3
                    }

                    PermissionRow {
                        id: banUsersRow

                        Layout.fillWidth: true
                        rightName: "canBanUsers"
                        rightValue: root.hasFacade ? root.facade.canBanUsers : false
                        switchEnabled: root.editable
                        title: qsTr("Ban users")
                        subtitle: qsTr("Ban, restrict or remove members.")

                        onRightToggled: function (value) {
                            if (root.hasFacade)
                                root.facade.setRight(banUsersRow.rightName, value)
                        }
                    }

                    DsDivider {
                        Layout.fillWidth: true
                        inset: Design.space3
                    }

                    PermissionRow {
                        id: inviteUsersRow

                        Layout.fillWidth: true
                        rightName: "canInviteUsers"
                        rightValue: root.hasFacade ? root.facade.canInviteUsers : false
                        switchEnabled: root.editable
                        title: qsTr("Invite users")
                        subtitle: qsTr("Add members and create invite links.")

                        onRightToggled: function (value) {
                            if (root.hasFacade)
                                root.facade.setRight(inviteUsersRow.rightName, value)
                        }
                    }

                    DsDivider {
                        Layout.fillWidth: true
                        inset: Design.space3
                    }

                    PermissionRow {
                        id: pinMessagesRow

                        Layout.fillWidth: true
                        rightName: "canPinMessages"
                        rightValue: root.hasFacade ? root.facade.canPinMessages : false
                        switchEnabled: root.editable
                        title: qsTr("Pin messages")
                        subtitle: qsTr("Pin important messages in the group.")

                        onRightToggled: function (value) {
                            if (root.hasFacade)
                                root.facade.setRight(pinMessagesRow.rightName, value)
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.margins: Design.space3
                        spacing: Design.space2

                        Text {
                            Layout.fillWidth: true
                            text: root.editable
                                  ? qsTr("Changes are sent to Telegram when you save.")
                                  : qsTr("The group owner's rights cannot be changed.")
                            color: BeyGramTheme.muted
                            font.pixelSize: Design.fontSmall
                            lineHeight: Design.lineNormal
                            wrapMode: Text.WordWrap
                        }

                        DsButton {
                            text: qsTr("Save")
                            tone: "primary"
                            size: "sm"
                            enabled: root.hasFacade && root.hasSelection && root.editable
                            onClicked: {
                                if (root.hasFacade)
                                    root.facade.save()
                            }
                        }
                    }
                }
            }
        }
    }
}
