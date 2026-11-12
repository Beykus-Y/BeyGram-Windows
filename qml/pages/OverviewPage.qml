import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQml
import BeyGram.Theme 1.0
import BeyGram.App 1.0
import "../ds/design.js" as Design

// OverviewPage — friendly User Profile screen matching Android BeyGram's
// ProfileScreen visual DNA. Technical diagnostics live in EnvironmentPage /
// Settings -> Diagnostics.
Item {
    id: page

    signal openSavedMessagesRequested()
    signal openAppearanceRequested()
    signal openSettingsRequested()
    signal openDiagnosticsRequested()

    property int chatCount: 0
    property int unreadMessagesCount: 0
    property int unreadChatsCount: 0
    property int customFolderCount: 0
    property string sessionBio: ""
    property bool phoneRevealed: false

    readonly property string displayName: BeyGramApp.accountDisplayName.length > 0
                                          ? BeyGramApp.accountDisplayName
                                          : qsTr("Telegram Account")
    readonly property string usernameText: BeyGramApp.accountUsername.length > 0
                                           ? "@" + BeyGramApp.accountUsername
                                           : ""
    readonly property string initials: {
        const text = page.displayName.trim()
        if (text.length === 0)
            return "B"
        const parts = text.split(/\s+/)
        if (parts.length >= 2)
            return (Array.from(parts[0])[0] + Array.from(parts[1])[0]).toUpperCase()
        return Array.from(text)[0].toUpperCase()
    }
    readonly property int avatarAccent: {
        let sum = 0
        for (let i = 0; i < page.displayName.length; ++i)
            sum += page.displayName.charCodeAt(i)
        return sum % 6
    }

    function recomputeChatStats() {
        const n = chatStats.count
        let unreadMsgs = 0
        let unreadChats = 0
        for (let i = 0; i < n; ++i) {
            const obj = chatStats.objectAt(i)
            if (!obj)
                continue
            const c = Number(obj["unreadCount"]) || 0
            if (c > 0) {
                unreadMsgs += c
                unreadChats += 1
            }
        }
        page.chatCount = n
        page.unreadMessagesCount = unreadMsgs
        page.unreadChatsCount = unreadChats
    }

    function recomputeFolderStats() {
        const n = folderStats.count
        let custom = 0
        for (let i = 0; i < n; ++i) {
            const obj = folderStats.objectAt(i)
            if (obj && !obj["isMain"])
                custom += 1
        }
        page.customFolderCount = custom
    }

    function openEditProfile() {
        const parts = page.displayName.trim().split(/\s+/)
        const first = parts.length > 0 ? parts[0] : ""
        const last = parts.length > 1 ? parts.slice(1).join(" ") : ""
        editDialog.firstName = first
        editDialog.lastName = last
        editDialog.username = BeyGramApp.accountUsername
        editDialog.bio = page.sessionBio
        editDialog.saving = false
        editDialog.errorText = ""
        if (BeyGramApp.profileEditor) {
            BeyGramApp.profileEditor.load(first, last, BeyGramApp.accountUsername, page.sessionBio)
        }
        editDialog.open()
    }

    // Reactive chat list & folder counters (avoids non-notify rowCount()).
    Instantiator {
        id: chatStats
        model: BeyGramApp.chatList
        delegate: QtObject {
            required property int unreadCount
            onUnreadCountChanged: page.recomputeChatStats()
        }
        onObjectAdded: page.recomputeChatStats()
        onObjectRemoved: page.recomputeChatStats()
        onCountChanged: page.recomputeChatStats()
    }

    Instantiator {
        id: folderStats
        model: BeyGramApp.chatFolders
        delegate: QtObject {
            required property bool isMain
            onIsMainChanged: page.recomputeFolderStats()
        }
        onObjectAdded: page.recomputeFolderStats()
        onObjectRemoved: page.recomputeFolderStats()
        onCountChanged: page.recomputeFolderStats()
    }

    Connections {
        target: BeyGramApp
        function onOwnProfileSaved() {
            if (BeyGramApp.profileEditor)
                BeyGramApp.profileEditor.markSaved()
            editDialog.saving = false
            editDialog.close()
            BeyGramApp.refresh()
            profileToast.show(qsTr("Profile updated"), "ok")
        }
    }

    Component.onCompleted: {
        recomputeChatStats()
        recomputeFolderStats()
    }

    component StatPill: ColumnLayout {
        id: stat
        property string value: "0"
        property string label: ""
        property color valueColor: BeyGramTheme.text

        Layout.fillWidth: true
        spacing: 2

        Text {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: stat.value
            color: stat.valueColor
            font.pixelSize: Design.fontHeadline
            font.weight: Font.Bold
        }

        Text {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: stat.label
            color: BeyGramTheme.muted
            font.pixelSize: Design.fontCaption
            elide: Text.ElideRight
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        DsHeader {
            Layout.fillWidth: true
            title: qsTr("Profile")
            subtitle: qsTr("Your Telegram account and quick shortcuts")
        }

        ScrollView {
            id: profileScroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth
            contentHeight: profileColumn.y + profileColumn.implicitHeight + Design.space6
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            ColumnLayout {
                id: profileColumn
                width: Math.min(640, Math.max(320, profileScroll.availableWidth - Design.space8 * 2))
                x: Math.round((profileScroll.availableWidth - width) / 2)
                y: Design.space6
                spacing: Design.space5

                // --- Hero Profile Card ---
                DsCard {
                    Layout.fillWidth: true
                    elevated: true
                    padding: Design.space6

                    content: ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Design.space4

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Design.space2

                            DsAvatar {
                                Layout.alignment: Qt.AlignHCenter
                                size: 96
                                initials: page.initials
                                accentIndex: page.avatarAccent
                                online: BeyGramApp.ready && BeyGramApp.running
                            }

                            Text {
                                Layout.fillWidth: true
                                Layout.topMargin: Design.space1
                                horizontalAlignment: Text.AlignHCenter
                                text: page.displayName
                                color: BeyGramTheme.text
                                font.pixelSize: Design.fontHeadline
                                font.weight: Font.Bold
                                wrapMode: Text.WordWrap
                            }

                            RowLayout {
                                Layout.alignment: Qt.AlignHCenter
                                spacing: Design.space2

                                Text {
                                    text: page.usernameText.length > 0
                                          ? page.usernameText
                                          : qsTr("No username set")
                                    color: page.usernameText.length > 0
                                           ? BeyGramTheme.accentInk
                                           : BeyGramTheme.muted
                                    font.pixelSize: Design.fontBody
                                    font.weight: Font.Medium
                                }

                                DsBadge {
                                    readonly property string rawConn: BeyGramApp.connectionTitle
                                    text: BeyGramApp.ready && BeyGramApp.running
                                          ? qsTr("Online")
                                          : (rawConn === "Connecting to Telegram"
                                             ? qsTr("Connecting to Telegram")
                                             : (rawConn === "Updating..."
                                                ? qsTr("Updating...")
                                                : (rawConn === "Waiting for network"
                                                   ? qsTr("Waiting for network")
                                                   : (rawConn === "Connected"
                                                      ? qsTr("Connected")
                                                      : rawConn))))
                                    tone: BeyGramApp.ready && BeyGramApp.running ? "ok" : "muted"
                                    dot: true
                                }
                            }

                            Text {
                                Layout.fillWidth: true
                                Layout.topMargin: Design.space1
                                horizontalAlignment: Text.AlignHCenter
                                text: page.sessionBio.length > 0
                                      ? page.sessionBio
                                      : qsTr("Add a bio or update your name and @username using Edit profile.")
                                color: page.sessionBio.length > 0 ? BeyGramTheme.text : BeyGramTheme.muted
                                font.pixelSize: Design.fontSmall
                                lineHeight: Design.lineNormal
                                wrapMode: Text.WordWrap
                            }
                        }

                        RowLayout {
                            Layout.alignment: Qt.AlignHCenter
                            spacing: Design.space2

                            DsButton {
                                text: qsTr("Edit profile")
                                icon: "\uE70F"
                                tone: "primary"
                                enabled: BeyGramApp.ready
                                onClicked: page.openEditProfile()
                            }

                            DsButton {
                                text: page.phoneRevealed ? qsTr("Hide details") : qsTr("Account info")
                                tone: "secondary"
                                onClicked: page.phoneRevealed = !page.phoneRevealed
                            }
                        }

                        // Optional click-to-reveal account detail row
                        Rectangle {
                            Layout.fillWidth: true
                            visible: page.phoneRevealed
                            implicitHeight: accountInfoRow.implicitHeight + Design.space3 * 2
                            radius: Design.radiusMd
                            color: BeyGramTheme.surface
                            border.width: 1
                            border.color: BeyGramTheme.line

                            RowLayout {
                                id: accountInfoRow
                                anchors.fill: parent
                                anchors.margins: Design.space3
                                spacing: Design.space3

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2

                                    Text {
                                        text: qsTr("Username")
                                        color: BeyGramTheme.muted
                                        font.pixelSize: Design.fontCaption
                                    }

                                    Text {
                                        text: page.usernameText.length > 0
                                              ? page.usernameText
                                              : qsTr("Not set")
                                        color: BeyGramTheme.text
                                        font.pixelSize: Design.fontSmall
                                        font.weight: Font.DemiBold
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2

                                    Text {
                                        text: qsTr("Phone number")
                                        color: BeyGramTheme.muted
                                        font.pixelSize: Design.fontCaption
                                    }

                                    Text {
                                        text: qsTr("Managed in Telegram")
                                        color: BeyGramTheme.muted
                                        font.pixelSize: Design.fontSmall
                                    }
                                }
                            }
                        }
                    }
                }

                // --- Compact Card 1: Conversation Stats ---
                DsCard {
                    Layout.fillWidth: true
                    padding: Design.space4

                    content: ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Design.space3

                        Text {
                            Layout.fillWidth: true
                            text: qsTr("Your conversations")
                            color: BeyGramTheme.text
                            font.pixelSize: Design.fontBodyLarge
                            font.weight: Font.DemiBold
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Design.space2

                            StatPill {
                                value: String(page.chatCount)
                                label: qsTr("%n chat(s)", "", page.chatCount)
                            }

                            DsDivider {
                                vertical: true
                                Layout.preferredHeight: 34
                            }

                            StatPill {
                                value: String(page.unreadMessagesCount)
                                label: page.unreadChatsCount > 0
                                       ? qsTr("%n unread chat(s)", "", page.unreadChatsCount)
                                       : qsTr("All caught up")
                                valueColor: page.unreadMessagesCount > 0
                                            ? BeyGramTheme.accentInk
                                            : BeyGramTheme.text
                            }

                            DsDivider {
                                vertical: true
                                Layout.preferredHeight: 34
                            }

                            StatPill {
                                value: String(page.customFolderCount)
                                label: qsTr("%n folder(s)", "", page.customFolderCount)
                            }
                        }
                    }
                }

                // --- Compact Card 2: Quick Actions ---
                DsCard {
                    Layout.fillWidth: true
                    padding: Design.space2

                    content: ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0

                        DsListRow {
                            Layout.fillWidth: true
                            title: qsTr("Saved Messages")
                            subtitle: qsTr("Your personal cloud notes and bookmarks")
                            leading: DsIcon {
                                name: "\uE728"
                                color: BeyGramTheme.accentInk
                            }
                            trailing: DsIcon {
                                name: "\uE76C"
                                size: 14
                                color: BeyGramTheme.muted
                            }
                            onClicked: page.openSavedMessagesRequested()
                        }

                        DsDivider { Layout.fillWidth: true; inset: Design.space3 }

                        DsListRow {
                            Layout.fillWidth: true
                            title: qsTr("Appearance & Themes")
                            subtitle: qsTr("Presets, AMOLED dark mode, Design Style and wallpapers")
                            leading: DsIcon {
                                name: "\uE790"
                                color: BeyGramTheme.accentInk
                            }
                            trailing: DsIcon {
                                name: "\uE76C"
                                size: 14
                                color: BeyGramTheme.muted
                            }
                            onClicked: page.openAppearanceRequested()
                        }

                        DsDivider { Layout.fillWidth: true; inset: Design.space3 }

                        DsListRow {
                            Layout.fillWidth: true
                            title: qsTr("Settings")
                            subtitle: qsTr("Language, notifications, network and privacy")
                            leading: DsIcon {
                                name: "\uE713"
                                color: BeyGramTheme.accentInk
                            }
                            trailing: DsIcon {
                                name: "\uE76C"
                                size: 14
                                color: BeyGramTheme.muted
                            }
                            onClicked: page.openSettingsRequested()
                        }

                        DsDivider { Layout.fillWidth: true; inset: Design.space3 }

                        DsListRow {
                            Layout.fillWidth: true
                            title: qsTr("Log out")
                            subtitle: qsTr("Sign out of this Windows device")
                            leading: DsIcon {
                                name: "\uE7E8"
                                color: BeyGramTheme.danger
                            }
                            onClicked: logoutDialog.open()
                        }
                    }
                }

                // Quiet diagnostics link at the bottom
                RowLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: Design.space1

                    Item { Layout.fillWidth: true }

                    DsButton {
                        text: qsTr("System & Diagnostics")
                        tone: "ghost"
                        size: "sm"
                        onClicked: page.openDiagnosticsRequested()
                    }

                    Item { Layout.fillWidth: true }
                }
            }
        }
    }

    ProfileEditDialog {
        id: editDialog
        onSaveRequested: function(firstName, lastName, username, bio) {
            page.sessionBio = bio
            editDialog.saving = true
            if (BeyGramApp.profileEditor) {
                BeyGramApp.profileEditor.firstName = firstName
                BeyGramApp.profileEditor.lastName = lastName
                BeyGramApp.profileEditor.username = username
                BeyGramApp.profileEditor.bio = bio
                if (!BeyGramApp.profileEditor.save()) {
                    editDialog.saving = false
                    editDialog.errorText = qsTr("First name cannot be empty.")
                }
            } else {
                BeyGramApp.saveOwnProfile(firstName, lastName, username, bio)
            }
        }
    }

    DsDialog {
        id: logoutDialog

        title: qsTr("Log out of Telegram?")
        description: qsTr("This signs out your account and removes local Telegram session data from this PC. You can sign back in anytime with a QR code or phone number.")
        primaryText: qsTr("Log out")
        secondaryText: qsTr("Cancel")
        tone: "danger"

        onAccepted: BeyGramApp.logout()
    }

    DsToast {
        id: profileToast
        anchors.fill: parent
        z: 100
    }
}
