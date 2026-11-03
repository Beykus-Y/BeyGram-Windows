import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0
import BeyGram.App 1.0

ApplicationWindow {
    id: root
    visible: true
    width: 1280
    height: 820
    minimumWidth: 980
    minimumHeight: 640
    title: qsTr("BeyGram Windows — Foundation")
    color: BeyGramTheme.bg

    property int currentSection: 0

    Behavior on color { ColorAnimation { duration: 180 } }

    Shortcut {
        sequence: StandardKey.Quit
        onActivated: Qt.quit()
    }
    Shortcut {
        sequence: "Ctrl+R"
        onActivated: BeyGramApp.refresh()
    }
    Shortcut {
        sequence: "Ctrl+D"
        onActivated: BeyGramTheme.toggleDarkMode()
    }
    Shortcut { sequence: "Alt+1"; onActivated: root.currentSection = 0 }
    Shortcut { sequence: "Alt+2"; onActivated: root.currentSection = 1 }
    Shortcut { sequence: "Alt+3"; onActivated: root.currentSection = 2 }

    // Ambient wash from the Android design system: two soft accent fields over
    // the plain screen colour. No backdrop is sampled, so nothing is faked.
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: Qt.alpha(BeyGramTheme.accent, 0.10) }
            GradientStop { position: 0.45; color: "transparent" }
            GradientStop { position: 1.0; color: Qt.alpha(BeyGramTheme.accent, 0.06) }
        }
        opacity: BeyGramTheme.darkMode ? 0.55 : 0.9
    }

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
            border.width: 0

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
                            text: "Windows · Phase 0"
                            color: BeyGramTheme.muted
                            font.pixelSize: 11
                        }
                    }
                    Item { Layout.fillWidth: true }
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: BeyGramTheme.line
                    Layout.topMargin: 4
                    Layout.bottomMargin: 4
                }

                NavButton {
                    Layout.fillWidth: true
                    text: "Overview"
                    selected: root.currentSection === 0
                    onClicked: root.currentSection = 0
                }
                NavButton {
                    Layout.fillWidth: true
                    text: "Environment"
                    selected: root.currentSection === 1
                    onClicked: root.currentSection = 1
                }
                NavButton {
                    Layout.fillWidth: true
                    text: "Logs"
                    selected: root.currentSection === 2
                    onClicked: root.currentSection = 2
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
                    text: "Alt+1/2/3 navigate · Ctrl+D theme · Ctrl+R refresh"
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
            currentIndex: root.currentSection

            OverviewPage {}
            EnvironmentPage {}
            LogsPage {}
        }
    }
}
