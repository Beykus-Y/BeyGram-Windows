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
            width: Math.min(760, page.width - 64)
            x: 32
            y: 28
            spacing: 18

            Text {
                text: "Settings"
                color: BeyGramTheme.text
                font.pixelSize: 26
                font.weight: Font.DemiBold
            }
            Text {
                Layout.fillWidth: true
                text: "Appearance and desktop behaviour are stored locally for this Windows account."
                color: BeyGramTheme.muted
                font.pixelSize: 13
                wrapMode: Text.WordWrap
            }

            Card {
                Layout.fillWidth: true
                spacing: 14

                Text {
                    text: "Theme"
                    color: BeyGramTheme.text
                    font.pixelSize: 15
                    font.weight: Font.DemiBold
                }

                Text {
                    text: "Colour family"
                    color: BeyGramTheme.muted
                    font.pixelSize: 12
                }
                Flow {
                    Layout.fillWidth: true
                    spacing: 8

                    ChoiceChip {
                        text: "Warm"
                        selected: BeyGramTheme.presetName === "Warm"
                        onClicked: BeyGramTheme.presetName = "Warm"
                    }
                    ChoiceChip {
                        text: "Graphite"
                        selected: BeyGramTheme.presetName === "Graphite"
                        onClicked: BeyGramTheme.presetName = "Graphite"
                    }
                }

                Rectangle { Layout.fillWidth: true; height: 1; color: BeyGramTheme.line }

                Text {
                    text: "Brightness"
                    color: BeyGramTheme.muted
                    font.pixelSize: 12
                }
                Flow {
                    Layout.fillWidth: true
                    spacing: 8

                    ChoiceChip {
                        text: "Follow system"
                        selected: BeyGramTheme.modeName === "FollowSystem"
                        onClicked: BeyGramTheme.modeName = "FollowSystem"
                    }
                    ChoiceChip {
                        text: "Light"
                        selected: BeyGramTheme.modeName === "Light"
                        onClicked: BeyGramTheme.modeName = "Light"
                    }
                    ChoiceChip {
                        text: "Dark"
                        selected: BeyGramTheme.modeName === "Dark"
                        onClicked: BeyGramTheme.modeName = "Dark"
                    }
                    ChoiceChip {
                        text: "AMOLED"
                        selected: BeyGramTheme.modeName === "Amoled"
                        onClicked: BeyGramTheme.modeName = "Amoled"
                    }
                }
            }

            Card {
                Layout.fillWidth: true
                spacing: 12

                Text {
                    text: "Notifications"
                    color: BeyGramTheme.text
                    font.pixelSize: 15
                    font.weight: Font.DemiBold
                }
                CheckBox {
                    text: "Desktop notifications"
                    checked: BeyGramApp.notificationsEnabled
                    palette.text: BeyGramTheme.text
                    palette.windowText: BeyGramTheme.text
                    onToggled: BeyGramApp.notificationsEnabled = checked
                }
                CheckBox {
                    text: "Show message preview"
                    checked: BeyGramApp.notificationPreview
                    enabled: BeyGramApp.notificationsEnabled
                    palette.text: BeyGramTheme.text
                    palette.windowText: BeyGramTheme.text
                    onToggled: BeyGramApp.notificationPreview = checked
                }
            }

            Card {
                Layout.fillWidth: true
                spacing: 12

                Text {
                    text: "Desktop"
                    color: BeyGramTheme.text
                    font.pixelSize: 15
                    font.weight: Font.DemiBold
                }
                Text {
                    Layout.fillWidth: true
                    text: "Keyboard: Alt+1..5 sections, Ctrl+D light/dark, Ctrl+R refresh. Enter sends, Shift+Enter adds a line."
                    color: BeyGramTheme.muted
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                    lineHeight: 1.35
                }
                Text {
                    Layout.fillWidth: true
                    text: "Desktop notifications use the Windows system tray and are suppressed for the open chat, muted chats and your own messages."
                    color: BeyGramTheme.muted
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                    lineHeight: 1.35
                }
            }

            Card {
                Layout.fillWidth: true
                spacing: 12

                Text {
                    text: "About"
                    color: BeyGramTheme.text
                    font.pixelSize: 15
                    font.weight: Font.DemiBold
                }
                StatusRow {
                    Layout.fillWidth: true
                    label: "BeyGram"
                    value: BeyGramApp.appVersion
                    tone: "accent"
                }
                StatusRow {
                    Layout.fillWidth: true
                    label: "Qt"
                    value: BeyGramApp.qtVersion
                    tone: "neutral"
                }
                Text {
                    Layout.fillWidth: true
                    text: BeyGramApp.dataDirectory
                    color: BeyGramTheme.muted
                    font.pixelSize: 11
                    elide: Text.ElideMiddle
                }
                Flow {
                    Layout.fillWidth: true
                    spacing: 8
                    SecondaryButton {
                        text: "Open data folder"
                        onClicked: BeyGramApp.openDataFolder()
                    }
                    SecondaryButton {
                        text: "Copy diagnostics"
                        onClicked: BeyGramApp.copyDiagnostics()
                    }
                }
            }

            Item { Layout.fillWidth: true; height: 8 }
        }
    }
}
