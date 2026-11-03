import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0
import BeyGram.App 1.0

Item {
    id: page

    function reload() {
        logText.text = BeyGramApp.recentLogs().join("\n")
        logText.cursorPosition = logText.length
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 32
        spacing: 14

        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 3
                Text {
                    text: "Logs"
                    color: BeyGramTheme.text
                    font.pixelSize: 28
                    font.weight: Font.DemiBold
                }
                Text {
                    text: "Secrets are redacted before a line reaches this list or the file."
                    color: BeyGramTheme.muted
                    font.pixelSize: 12
                }
            }
            SecondaryButton {
                text: "Reload"
                onClicked: page.reload()
            }
            SecondaryButton {
                text: "Open log folder"
                onClicked: BeyGramApp.openLogFolder()
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 14
            color: Qt.alpha(BeyGramTheme.surface, BeyGramTheme.darkMode ? 0.82 : 0.95)
            border.width: 1
            border.color: BeyGramTheme.line

            ScrollView {
                anchors.fill: parent
                anchors.margins: 10
                clip: true

                TextArea {
                    id: logText
                    readOnly: true
                    selectByMouse: true
                    wrapMode: TextArea.NoWrap
                    color: BeyGramTheme.text
                    selectionColor: BeyGramTheme.accent
                    selectedTextColor: BeyGramTheme.onAccent
                    font.family: "Cascadia Mono"
                    font.pixelSize: 11
                    textFormat: TextArea.PlainText
                    placeholderText: "No log lines yet."
                    background: null
                }
            }
        }

        Text {
            Layout.fillWidth: true
            text: "File: " + BeyGramApp.logDirectory + "\\beygram.log · keeps 3 rotated files, 5 MiB each"
            color: BeyGramTheme.muted
            font.pixelSize: 10
            elide: Text.ElideMiddle
        }
    }

    Connections {
        target: BeyGramApp
        function onStateChanged() { page.reload() }
    }

    Component.onCompleted: reload()
}
