import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0
import BeyGram.App 1.0

Item {
    id: shell

    property int currentSection: 0

    Shortcut { sequence: "Alt+1"; onActivated: shell.currentSection = 0 }
    Shortcut { sequence: "Alt+2"; onActivated: shell.currentSection = 1 }
    Shortcut { sequence: "Alt+3"; onActivated: shell.currentSection = 2 }
    Shortcut { sequence: "Alt+4"; onActivated: shell.currentSection = 3 }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // ------------------------------------------------------------------
        // Navigation rail / sidebar
        // ------------------------------------------------------------------
        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: 268
            color: Qt.alpha(BeyGramTheme.surface, BeyGramTheme.darkMode ? 0.72 : 0.82)

            Rectangle {
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: 1
                color: BeyGramTheme.line
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 18
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Rectangle {
                        width: 40
                        height: 40
                        radius: 13
                        color: BeyGramTheme.accent
                        Text {
                            anchors.centerIn: parent
                            text: "B"
                            color: BeyGramTheme.onAccent
                            font.pixelSize: 22
                            font.weight: Font.DemiBold
                        }
                    }
                    ColumnLayout {
                        spacing: 1
                        Text {
                            text: "BeyGram"
                            color: BeyGramTheme.text
                            font.pixelSize: 17
                            font.weight: Font.DemiBold
                        }
                        Text {
                            text: "Windows · Phase 1"
                            color: BeyGramTheme.muted
                            font.pixelSize: 11
                        }
                    }
                    Item { Layout.fillWidth: true }
                }

                // Signed-in account
                Rectangle {
                    Layout.fillWidth: true
                    height: 58
                    radius: 13
                    color: Qt.alpha(BeyGramTheme.surface2, BeyGramTheme.darkMode ? 0.55 : 0.7)
                    border.width: 1
                    border.color: Qt.alpha(BeyGramTheme.line, 0.9)

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 10

                        Rectangle {
                            width: 34
                            height: 34
                            radius: 17
                            color: BeyGramTheme.accentSoft
                            Text {
                                anchors.centerIn: parent
                                text: BeyGramApp.accountDisplayName.length > 0
                                      ? BeyGramApp.accountDisplayName.charAt(0).toUpperCase()
                                      : "T"
                                color: BeyGramTheme.accentInk
                                font.pixelSize: 15
                                font.weight: Font.DemiBold
                            }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 1
                            Text {
                                Layout.fillWidth: true
                                text: BeyGramApp.accountDisplayName.length > 0
                                      ? BeyGramApp.accountDisplayName
                                      : "Telegram account"
                                color: BeyGramTheme.text
                                font.pixelSize: 12
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                            }
                            Text {
                                Layout.fillWidth: true
                                text: BeyGramApp.accountUsername.length > 0
                                      ? "@" + BeyGramApp.accountUsername
                                      : BeyGramApp.connectionTitle
                                color: BeyGramTheme.muted
                                font.pixelSize: 10
                                elide: Text.ElideRight
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: BeyGramTheme.line
                    Layout.topMargin: 2
                    Layout.bottomMargin: 2
                }

                NavButton {
                    Layout.fillWidth: true
                    text: "Chats"
                    selected: shell.currentSection === 0
                    onClicked: shell.currentSection = 0
                }
                NavButton {
                    Layout.fillWidth: true
                    text: "Account"
                    selected: shell.currentSection === 1
                    onClicked: shell.currentSection = 1
                }
                NavButton {
                    Layout.fillWidth: true
                    text: "Environment"
                    selected: shell.currentSection === 2
                    onClicked: shell.currentSection = 2
                }
                NavButton {
                    Layout.fillWidth: true
                    text: "Logs"
                    selected: shell.currentSection === 3
                    onClicked: shell.currentSection = 3
                }

                Text {
                    Layout.fillWidth: true
                    Layout.topMargin: 18
                    text: "ANDROID REFERENCE MODULES"
                    color: BeyGramTheme.muted
                    font.pixelSize: 10
                    font.letterSpacing: 1.1
                    font.weight: Font.DemiBold
                }

                NavButton {
                    Layout.fillWidth: true
                    text: "Chats & media"
                    hint: "Phase 2"
                    enabled: false
                }
                NavButton {
                    Layout.fillWidth: true
                    text: "Music & BG Mix"
                    hint: "Phase 4"
                    enabled: false
                }
                NavButton {
                    Layout.fillWidth: true
                    text: "Settings & themes"
                    hint: "Phase 3"
                    enabled: false
                }

                Item { Layout.fillHeight: true }

                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: BeyGramTheme.line
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Text {
                        text: BeyGramTheme.darkMode ? "Dark" : "Light"
                        color: BeyGramTheme.muted
                        font.pixelSize: 12
                    }
                    Item { Layout.fillWidth: true }
                    SecondaryButton {
                        text: BeyGramTheme.darkMode ? "Use light" : "Use dark"
                        onClicked: BeyGramTheme.toggleDarkMode()
                    }
                }

                Text {
                    Layout.fillWidth: true
                    text: "Alt+1/2/3/4 navigate · Ctrl+D theme · Ctrl+R refresh"
                    color: BeyGramTheme.muted
                    font.pixelSize: 10
                    wrapMode: Text.WordWrap
                }
            }
        }

        // ------------------------------------------------------------------
        // Content
        // ------------------------------------------------------------------
        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: shell.currentSection

            ChatsPage {}
            OverviewPage {}
            EnvironmentPage {}
            LogsPage {}
        }
    }
}
