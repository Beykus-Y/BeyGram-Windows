import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0
import BeyGram.App 1.0
import "../ds/design.js" as Design

// OverviewPage — the signed-in account at a glance.
//
// Every AppController binding from the previous layout is kept: identity,
// connection state, runtime facts and the four account actions. The page is
// rebuilt from the design system so its surfaces, spacing and type follow the
// same tokens as the rest of BeyGram.
Item {
    id: page

    readonly property string displayName: BeyGramApp.accountDisplayName.length > 0
                                          ? BeyGramApp.accountDisplayName
                                          : "Telegram account"

    readonly property bool connected: BeyGramApp.connectionTitle === "Connected"

    readonly property string initials: {
        var parts = BeyGramApp.accountDisplayName.trim().split(/\s+/)
        var letters = ""
        for (var i = 0; i < parts.length && letters.length < 2; ++i) {
            if (parts[i].length > 0)
                letters += parts[i].charAt(0)
        }
        return letters.length > 0 ? letters.toUpperCase() : "T"
    }

    readonly property string credentialsLabel: BeyGramApp.credentialsSource === "environment"
                                               ? "Environment"
                                               : (BeyGramApp.credentialsSource === "build"
                                                  ? "Build (local)"
                                                  : "DPAPI")

    // Compact label/value/detail stack used inside the hero card.
    component MetaFact: ColumnLayout {
        id: fact

        property string label: ""
        property string value: ""
        property string detail: ""
        property color valueColor: BeyGramTheme.text

        Layout.fillWidth: true
        Layout.preferredWidth: 1
        Layout.minimumWidth: 0
        spacing: Design.space1

        Text {
            Layout.fillWidth: true
            text: fact.label.toUpperCase()
            color: BeyGramTheme.muted
            font.pixelSize: Design.fontCaption
            font.weight: Font.DemiBold
            font.letterSpacing: 0.6
            elide: Text.ElideRight
        }

        Text {
            Layout.fillWidth: true
            text: fact.value
            color: fact.valueColor
            font.pixelSize: Design.fontBody
            font.weight: Font.DemiBold
            textFormat: Text.PlainText
            elide: Text.ElideRight
        }

        Text {
            Layout.fillWidth: true
            visible: text.length > 0
            text: fact.detail
            color: BeyGramTheme.muted
            font.pixelSize: Design.fontCaption
            textFormat: Text.PlainText
            lineHeight: Design.lineNormal
            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            maximumLineCount: 2
            elide: Text.ElideRight
        }
    }

    // One number in the two-column stat grid.
    component StatCard: DsCard {
        id: stat

        property string label: ""
        property string value: ""
        property string hint: ""
        property color valueColor: BeyGramTheme.text

        padding: Design.space4

        content: ColumnLayout {
            Layout.fillWidth: true
            spacing: Design.space1

            Text {
                Layout.fillWidth: true
                text: stat.label.toUpperCase()
                color: BeyGramTheme.muted
                font.pixelSize: Design.fontCaption
                font.weight: Font.DemiBold
                font.letterSpacing: 0.6
                elide: Text.ElideRight
            }

            Text {
                Layout.fillWidth: true
                text: stat.value
                color: stat.valueColor
                font.pixelSize: Design.fontDisplay
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }

            Text {
                Layout.fillWidth: true
                visible: text.length > 0
                text: stat.hint
                color: BeyGramTheme.muted
                font.pixelSize: Design.fontCaption
                lineHeight: Design.lineNormal
                wrapMode: Text.WordWrap
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        DsHeader {
            Layout.fillWidth: true
            title: "Account overview"
            subtitle: page.displayName + " · " + BeyGramApp.stageTitle
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            ColumnLayout {
                width: Math.min(920, Math.max(320, page.width - Design.space8 * 2))
                x: Math.max(Design.space8, (page.width - width) / 2)
                y: Design.space6
                spacing: Design.space5

                // Hero: identity, connection state and the runtime facts that
                // used to live in the old account card.
                DsCard {
                    Layout.fillWidth: true
                    elevated: true
                    padding: Design.space5

                    content: ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Design.space4

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Design.space4

                            DsAvatar {
                                Layout.alignment: Qt.AlignVCenter
                                size: Design.avatarXLarge
                                initials: page.initials
                                accentIndex: 1
                                online: page.connected
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.alignment: Qt.AlignVCenter
                                spacing: Design.space1

                                Text {
                                    Layout.fillWidth: true
                                    text: page.displayName
                                    color: BeyGramTheme.text
                                    font.pixelSize: Design.fontHeadline
                                    font.weight: Font.DemiBold
                                    textFormat: Text.PlainText
                                    elide: Text.ElideRight
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: BeyGramApp.accountUsername.length > 0
                                          ? "@" + BeyGramApp.accountUsername
                                          : "Signed in through the native TDLib session"
                                    color: BeyGramTheme.muted
                                    font.pixelSize: Design.fontSmall
                                    textFormat: Text.PlainText
                                    elide: Text.ElideRight
                                }

                                DsBadge {
                                    text: BeyGramApp.connectionTitle.toUpperCase()
                                    tone: page.connected
                                          ? "ok"
                                          : (BeyGramApp.running ? "accent" : "muted")
                                    dot: true
                                }
                            }
                        }

                        DsDivider { Layout.fillWidth: true }

                        GridLayout {
                            Layout.fillWidth: true
                            columns: 2
                            columnSpacing: Design.space5
                            rowSpacing: Design.space3

                            MetaFact {
                                label: "Authorization"
                                value: BeyGramApp.ready ? "Ready" : "Pending"
                                detail: BeyGramApp.stageDetail
                                valueColor: BeyGramApp.ready
                                            ? BeyGramTheme.okInk
                                            : BeyGramTheme.muted
                            }

                            MetaFact {
                                label: "Transport"
                                value: BeyGramApp.connectionTitle
                                valueColor: page.connected
                                            ? BeyGramTheme.okInk
                                            : BeyGramTheme.accentInk
                            }

                            MetaFact {
                                label: "TDLib runtime"
                                value: BeyGramApp.tdlibAvailable ? "Loaded" : "Not available"
                                detail: BeyGramApp.tdlibPath
                                valueColor: BeyGramApp.tdlibAvailable
                                            ? BeyGramTheme.okInk
                                            : BeyGramTheme.danger
                            }

                            MetaFact {
                                label: "API credentials"
                                value: page.credentialsLabel
                                detail: BeyGramApp.credentialsHint
                                valueColor: BeyGramTheme.okInk
                            }
                        }
                    }
                }

                // Two-column stat grid: chats, unread, folders and contacts.
                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: Design.space3
                    rowSpacing: Design.space3

                    StatCard {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.preferredWidth: 1
                        Layout.minimumWidth: 0
                        label: "Chats"
                        value: BeyGramApp.chatList ? String(BeyGramApp.chatList.rowCount()) : "0"
                        hint: "In the chat list"
                    }

                    StatCard {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.preferredWidth: 1
                        Layout.minimumWidth: 0
                        label: "Unread"
                        value: String(BeyGramApp.chatListControls.totalUnread)
                        hint: "Across every tracked chat"
                        valueColor: BeyGramApp.chatListControls.totalUnread > 0
                                    ? BeyGramTheme.accentInk
                                    : BeyGramTheme.text
                    }

                    StatCard {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.preferredWidth: 1
                        Layout.minimumWidth: 0
                        label: "Folders"
                        value: String(BeyGramApp.chatFolders ? BeyGramApp.chatFolders.rowCount() : 0)
                               ? Math.max(0, BeyGramApp.chatFolders.count - 1)
                               : 0
                        hint: "Custom folders beyond All chats"
                    }

                    StatCard {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.preferredWidth: 1
                        Layout.minimumWidth: 0
                        label: "Contacts"
                        value: String(BeyGramApp.contacts ? BeyGramApp.contacts.names.length : 0)
                        hint: "In the local contact snapshot"
                    }
                }

                // Quick actions.
                Flow {
                    Layout.fillWidth: true
                    spacing: Design.space2

                    DsButton {
                        text: "Refresh"
                        tone: "primary"
                        icon: "\uE72C" // Segoe MDL2 Assets: Refresh
                        onClicked: BeyGramApp.refresh()
                    }

                    DsButton {
                        text: "Copy diagnostics"
                        tone: "secondary"
                        icon: "\uE8C8" // Segoe MDL2 Assets: Copy
                        onClicked: BeyGramApp.copyDiagnostics()
                    }

                    DsButton {
                        text: "Open data folder"
                        tone: "secondary"
                        icon: "\uE838" // Segoe MDL2 Assets: FolderOpen
                        onClicked: BeyGramApp.openDataFolder()
                    }

                    DsButton {
                        text: "Log out"
                        tone: "secondary"
                        icon: "\uE7E8" // Segoe MDL2 Assets: PowerButton
                        onClicked: logoutDialog.open()
                    }
                }

                DsSection {
                    Layout.fillWidth: true
                    title: "Session storage"
                    description: "The TDLib database is encrypted with a key generated on this PC "
                                 + "and wrapped by Windows DPAPI. A lock file prevents a second "
                                 + "BeyGram process from opening the same account. The session is "
                                 + "restored automatically on the next start."
                }

                Item { Layout.preferredHeight: Design.space2 }
            }
        }
    }

    DsDialog {
        id: logoutDialog

        title: "Log out of Telegram?"
        description: "This sends LogOut to Telegram and destroys the local Telegram data for "
                     + "this account. You will need to sign in again with a phone number or QR code."
        primaryText: "Log out"
        secondaryText: "Cancel"
        tone: "danger"

        onAccepted: BeyGramApp.logout()

        Text {
            Layout.fillWidth: true
            text: "API credentials stored with DPAPI are not removed by logout."
            color: BeyGramTheme.muted
            font.pixelSize: Design.fontCaption
            wrapMode: Text.WordWrap
        }
    }
}
