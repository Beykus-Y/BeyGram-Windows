import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0
import BeyGram.App 1.0

Item {
    id: page

    ScrollView {
        anchors.fill: parent
        clip: true
        contentWidth: availableWidth
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            width: Math.max(560, page.width - 64)
            x: 32
            y: 28
            spacing: 18

            RowLayout {
                Layout.fillWidth: true
                spacing: 12
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4
                    Text {
                        text: "Account overview"
                        color: BeyGramTheme.text
                        font.pixelSize: 26
                        font.weight: Font.DemiBold
                    }
                    Text {
                        text: "Signed in through the native TDLib session"
                        color: BeyGramTheme.muted
                        font.pixelSize: 13
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                }
            }

            Card {
                Layout.fillWidth: true
                spacing: 14

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 14

                    Rectangle {
                        width: 54
                        height: 54
                        radius: 27
                        color: BeyGramTheme.accentSoft
                        Text {
                            anchors.centerIn: parent
                            text: BeyGramApp.accountDisplayName.length > 0
                                  ? BeyGramApp.accountDisplayName.charAt(0).toUpperCase()
                                  : "T"
                            color: BeyGramTheme.accentInk
                            font.pixelSize: 24
                            font.weight: Font.DemiBold
                        }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Text {
                            Layout.fillWidth: true
                            text: BeyGramApp.accountDisplayName.length > 0
                                  ? BeyGramApp.accountDisplayName
                                  : "Telegram account"
                            color: BeyGramTheme.text
                            font.pixelSize: 19
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                        }
                        Text {
                            Layout.fillWidth: true
                            text: BeyGramApp.accountUsername.length > 0
                                  ? "@" + BeyGramApp.accountUsername
                                  : "Signed in"
                            color: BeyGramTheme.muted
                            font.pixelSize: 12
                        }
                    }
                    Pill {
                        text: BeyGramApp.connectionTitle.toUpperCase()
                        tone: BeyGramApp.connectionTitle === "Connected" ? "ok" : "warn"
                    }
                }

                Rectangle { Layout.fillWidth: true; height: 1; color: BeyGramTheme.line }

                StatusRow {
                    Layout.fillWidth: true
                    label: "Authorization"
                    value: "Ready"
                    tone: "ok"
                    detail: BeyGramApp.stageDetail
                }
                StatusRow {
                    Layout.fillWidth: true
                    label: "Transport"
                    value: BeyGramApp.connectionTitle
                    tone: BeyGramApp.connectionTitle === "Connected" ? "ok" : "accent"
                }
                StatusRow {
                    Layout.fillWidth: true
                    label: "TDLib runtime"
                    value: "Loaded"
                    tone: "ok"
                    detail: BeyGramApp.tdlibPath
                }
                StatusRow {
                    Layout.fillWidth: true
                    label: "API credentials"
                    value: BeyGramApp.credentialsSource === "environment" ? "Environment"
                         : BeyGramApp.credentialsSource === "build" ? "Build (local)"
                         : "DPAPI"
                    tone: "ok"
                    detail: BeyGramApp.credentialsHint
                }

                Flow {
                    Layout.fillWidth: true
                    spacing: 8
                    SecondaryButton {
                        text: "Refresh"
                        onClicked: BeyGramApp.refresh()
                    }
                    SecondaryButton {
                        text: "Copy diagnostics"
                        onClicked: BeyGramApp.copyDiagnostics()
                    }
                    SecondaryButton {
                        text: "Open data folder"
                        onClicked: BeyGramApp.openDataFolder()
                    }
                    SecondaryButton {
                        text: "Log out"
                        onClicked: logoutDialog.open()
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 18

                Card {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    Layout.minimumWidth: 0
                    Layout.alignment: Qt.AlignTop
                    spacing: 10

                    Text {
                        text: "Session storage"
                        color: BeyGramTheme.text
                        font.pixelSize: 15
                        font.weight: Font.DemiBold
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                    Text {
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        text: "The TDLib database is encrypted with a key generated on this PC and wrapped by Windows DPAPI. A lock file prevents a second BeyGram process from opening the same account. The session is restored automatically on the next start."
                        color: BeyGramTheme.muted
                        font.pixelSize: 12
                        wrapMode: Text.WordWrap
                        lineHeight: 1.35
                    }
                }
            }

            Item { Layout.fillWidth: true; height: 8 }
        }
    }

    Dialog {
        id: logoutDialog
        title: "Log out of Telegram?"
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.NoButton
        width: 430

        ColumnLayout {
            spacing: 12
            Text {
                Layout.fillWidth: true
                text: "This sends LogOut to Telegram and destroys the local Telegram data for this account. You will need to sign in again with a phone number or QR code."
                color: BeyGramTheme.text
                font.pixelSize: 12
                wrapMode: Text.WordWrap
                lineHeight: 1.35
            }
            Text {
                Layout.fillWidth: true
                text: "API credentials stored with DPAPI are not removed by logout."
                color: BeyGramTheme.muted
                font.pixelSize: 11
                wrapMode: Text.WordWrap
            }
            RowLayout {
                Layout.alignment: Qt.AlignRight
                spacing: 8
                SecondaryButton {
                    text: "Cancel"
                    onClicked: logoutDialog.close()
                }
                PrimaryButton {
                    text: "Log out"
                    tone: "danger"
                    onClicked: {
                        logoutDialog.close()
                        BeyGramApp.logout()
                    }
                }
            }
        }
    }
}
