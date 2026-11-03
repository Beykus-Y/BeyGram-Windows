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
                        text: "Desktop foundation"
                        color: BeyGramTheme.text
                        font.pixelSize: 30
                        font.weight: Font.DemiBold
                    }
                    Text {
                        text: "Native C++20 and Qt Quick shell for the BeyGram Windows port"
                        color: BeyGramTheme.muted
                        font.pixelSize: 13
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                }
                Pill { text: "PHASE 0"; tone: "accent" }
                Pill {
                    text: BeyGramApp.ready ? "AUTHORIZED" : "NO ACCOUNT"
                    tone: BeyGramApp.ready ? "ok" : "neutral"
                }
            }

            Card {
                Layout.fillWidth: true
                spacing: 14

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: "Telegram connection"
                        color: BeyGramTheme.text
                        font.pixelSize: 16
                        font.weight: Font.DemiBold
                        Layout.fillWidth: true
                    }
                    Pill {
                        text: BeyGramApp.running ? "CORE RUNNING" : "STOPPED"
                        tone: BeyGramApp.running ? "ok" : "neutral"
                    }
                }

                Rectangle { Layout.fillWidth: true; height: 1; color: BeyGramTheme.line }

                StatusRow {
                    Layout.fillWidth: true
                    label: "TDLib runtime"
                    value: BeyGramApp.tdlibAvailable ? "Available" : "Missing"
                    tone: BeyGramApp.tdlibAvailable ? "ok" : "danger"
                    detail: BeyGramApp.tdlibDetail
                }

                StatusRow {
                    Layout.fillWidth: true
                    label: "Telegram API credentials"
                    value: BeyGramApp.credentialsConfigured ? "Configured" : "Not configured"
                    tone: BeyGramApp.credentialsConfigured ? "ok" : "warn"
                    detail: BeyGramApp.credentialsConfigured
                            ? "Loaded from the process environment; never written to the log or the repository."
                            : BeyGramApp.credentialsHint
                }

                StatusRow {
                    Layout.fillWidth: true
                    label: "Authorization"
                    value: BeyGramApp.stageTitle
                    tone: BeyGramApp.ready ? "ok" : (BeyGramApp.running ? "accent" : "neutral")
                    detail: BeyGramApp.stageDetail
                }

                StatusRow {
                    Layout.fillWidth: true
                    visible: BeyGramApp.running
                    label: "Transport"
                    value: BeyGramApp.connectionTitle
                    tone: BeyGramApp.ready ? "ok" : "accent"
                }

                StatusRow {
                    Layout.fillWidth: true
                    visible: BeyGramApp.lastError !== ""
                    label: "Last error"
                    value: "Reported"
                    tone: "danger"
                    detail: BeyGramApp.lastError
                }

                Flow {
                    Layout.fillWidth: true
                    spacing: 8

                    PrimaryButton {
                        text: BeyGramApp.running ? "Restart Telegram" : "Start Telegram"
                        enabled: BeyGramApp.canStart || BeyGramApp.running
                        onClicked: {
                            if (BeyGramApp.running)
                                BeyGramApp.stopTelegram()
                            BeyGramApp.startTelegram()
                        }
                    }
                    SecondaryButton {
                        text: "Stop"
                        enabled: BeyGramApp.running
                        onClicked: BeyGramApp.stopTelegram()
                    }
                    SecondaryButton {
                        text: "Refresh"
                        onClicked: BeyGramApp.refresh()
                    }
                    SecondaryButton {
                        text: "Open data folder"
                        onClicked: BeyGramApp.openDataFolder()
                    }
                    SecondaryButton {
                        text: "Copy diagnostics"
                        onClicked: BeyGramApp.copyDiagnostics()
                    }
                }

                Text {
                    Layout.fillWidth: true
                    text: "Phase 0 does not implement a login form. If TDLib and credentials are present, the core can reach the phone, code or password stage; the UI shows that state without offering input yet."
                    color: BeyGramTheme.muted
                    font.pixelSize: 11
                    wrapMode: Text.WordWrap
                    lineHeight: 1.3
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
                        text: "What this build does not do"
                        color: BeyGramTheme.text
                        font.pixelSize: 15
                        font.weight: Font.DemiBold
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }

                    Column {
                        Layout.fillWidth: true
                        spacing: 7

                        Repeater {
                            model: [
                                "No Telegram messages, chats, profiles or media are loaded.",
                                "No login form, multi-account switcher or notifications.",
                                "No calls, stories, gifts, secret chats or proxy management.",
                                "No music player, playlists or BG Beat / BG Mix audio.",
                                "No fake data: empty states describe the real state."
                            ]
                            delegate: RowLayout {
                                width: parent.width
                                spacing: 8
                                Text {
                                    text: "•"
                                    color: BeyGramTheme.accent
                                    font.pixelSize: 14
                                }
                                Text {
                                    text: modelData
                                    color: BeyGramTheme.muted
                                    font.pixelSize: 12
                                    wrapMode: Text.WordWrap
                                    lineHeight: 1.3
                                    Layout.fillWidth: true
                                    Layout.preferredWidth: 1
                                }
                            }
                        }
                    }
                }

                Card {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    Layout.minimumWidth: 0
                    Layout.alignment: Qt.AlignTop
                    spacing: 10

                    Text {
                        text: "Next: TDLib and authorization"
                        color: BeyGramTheme.text
                        font.pixelSize: 15
                        font.weight: Font.DemiBold
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                    Text {
                        text: "Phase 1 wires the real authorization flow (phone, code, two-step password, QR), keeps the session across restarts, and introduces the account-scoped TDLib hosts that the chat repository will use."
                        color: BeyGramTheme.muted
                        font.pixelSize: 12
                        wrapMode: Text.WordWrap
                        lineHeight: 1.35
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                    }
                    Flow {
                        Layout.fillWidth: true
                        spacing: 6
                        Pill { text: "TDLib JSON" }
                        Pill { text: "Auth state machine" }
                        Pill { text: "DPAPI storage" }
                    }
                }
            }

            Card {
                Layout.fillWidth: true
                spacing: 10

                Text {
                    text: "Porting map"
                    color: BeyGramTheme.text
                    font.pixelSize: 15
                    font.weight: Font.DemiBold
                }
                Text {
                    text: "docs/PORTING_AUDIT.md classifies every Android module as direct port, platform adaptation, QML rewrite or special case. docs/PORTING_ROADMAP.md orders the work. The Android tree under app/ is preserved unchanged as the functional reference."
                    color: BeyGramTheme.muted
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                    lineHeight: 1.35
                    Layout.fillWidth: true
                }
                Flow {
                    Layout.fillWidth: true
                    spacing: 6
                    Pill { text: "A · algorithms" }
                    Pill { text: "B · platform" }
                    Pill { text: "C · QML" }
                    Pill { text: "D · special cases" }
                }
            }

            Item { Layout.fillWidth: true; height: 8 }
        }
    }
}
