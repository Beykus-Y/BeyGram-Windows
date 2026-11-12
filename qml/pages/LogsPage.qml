// LogsPage — live log viewer over the logger's in-memory ring.
//
// BeyGramApp.recentLogs() is the model. Refresh replaces the buffer, Clear
// empties only this page's copy (the rotating file on disk is kept), Copy puts
// the visible lines on the clipboard and Open folder reveals beygram.log.
// Secrets are redacted by the logger before a line reaches the ring or the file.

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0
import BeyGram.App 1.0
import "../ds/design.js" as Design

Item {
    id: page

    property var lines: []
    property var visibleLines: []

    readonly property string query: filterField.text.trim()
    readonly property string monoFamily: "Cascadia Mono"
    // Digits needed for the largest line number, plus the gutter padding.
    readonly property int gutterWidth: Math.max(28, String(Math.max(1, page.lines.length)).length * 7 + 10)

    // The logger writes "yyyy-MM-dd HH:mm:ss.zzz LEVEL [category] message".
    // Lines without that prefix (stack traces, wrapped messages) stay readable
    // and keep the default tone.
    function parseLine(raw, lineNumber) {
        const match = /^(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\.\d{3}) (\w+)\s+\[[^\]]*\]/.exec(raw)
        return {
            lineNumber: lineNumber,
            level: match ? match[2] : "LOG",
            text: raw
        }
    }

    // warnInk is the readable ink role for the warn tone; warn itself is a
    // background tint and would disappear on a surface.
    function levelColor(level) {
        if (level === "DEBUG")
            return BeyGramTheme.muted
        if (level === "WARN")
            return BeyGramTheme.warnInk
        if (level === "ERROR" || level === "FATAL")
            return BeyGramTheme.danger
        return BeyGramTheme.text
    }

    function rebuild() {
        const needle = filterField.text.trim().toLowerCase()
        const source = page.lines || []
        const result = []
        for (let i = 0; i < source.length; ++i) {
            const raw = String(source[i])
            if (needle.length > 0 && raw.toLowerCase().indexOf(needle) < 0)
                continue
            result.push(page.parseLine(raw, i + 1))
        }
        page.visibleLines = result
    }

    function reload() {
        const previousY = logList.contentY
        page.lines = BeyGramApp.recentLogs()
        page.rebuild()
        if (autoScroll.checked) {
            Qt.callLater(function() { logList.positionViewAtEnd() })
        } else {
            // Keep the reading position when the user turned following off.
            Qt.callLater(function() {
                const maxY = Math.max(0, logList.contentHeight - logList.height)
                logList.contentY = Math.min(previousY, maxY)
            })
        }
    }

    function clearView() {
        page.lines = []
        page.visibleLines = []
    }

    function copyVisible() {
        const parts = []
        for (let i = 0; i < page.visibleLines.length; ++i)
            parts.push(page.visibleLines[i].text)
        if (parts.length > 0)
            BeyGramApp.copyText(parts.join("\n"))
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        DsHeader {
            Layout.fillWidth: true
            title: "Logs"
            subtitle: "Secrets are redacted before a line reaches this list or the file."
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: Design.space4
            spacing: Design.space3

            // ----------------------------------------------------------
            // Toolbar: filter, follow toggle and the page actions.
            // ----------------------------------------------------------
            RowLayout {
                Layout.fillWidth: true
                spacing: Design.space2

                DsSearchField {
                    id: filterField
                    Layout.fillWidth: true
                    placeholderText: "Filter log lines"
                    Accessible.name: "Filter log lines"
                    onTextChanged: page.rebuild()
                }

                Text {
                    Layout.alignment: Qt.AlignVCenter
                    Layout.leftMargin: Design.space1
                    text: page.query.length > 0
                          ? page.visibleLines.length + " of " + page.lines.length
                          : page.lines.length + (page.lines.length === 1 ? " line" : " lines")
                    color: BeyGramTheme.muted
                    font.pixelSize: Design.fontCaption
                }

                DsSwitch {
                    id: autoScroll
                    Layout.alignment: Qt.AlignVCenter
                    text: "Auto-scroll"
                    checked: true
                    onToggled: {
                        if (checked)
                            Qt.callLater(function() { logList.positionViewAtEnd() })
                    }
                }

                Rectangle {
                    Layout.alignment: Qt.AlignVCenter
                    Layout.preferredWidth: 1
                    Layout.preferredHeight: 22
                    color: BeyGramTheme.line
                }

                DsIconButton {
                    Layout.alignment: Qt.AlignVCenter
                    icon: "\uE72C"
                    tooltip: "Refresh logs"
                    onClicked: page.reload()
                }

                DsIconButton {
                    Layout.alignment: Qt.AlignVCenter
                    icon: "\uE894"
                    tooltip: "Clear view (the log file is kept)"
                    enabled: page.lines.length > 0
                    onClicked: page.clearView()
                }

                DsIconButton {
                    Layout.alignment: Qt.AlignVCenter
                    icon: "\uE8C8"
                    tooltip: "Copy visible lines"
                    enabled: page.visibleLines.length > 0
                    onClicked: page.copyVisible()
                }

                DsIconButton {
                    Layout.alignment: Qt.AlignVCenter
                    icon: "\uE838"
                    tooltip: "Open log folder"
                    onClicked: BeyGramApp.openLogFolder()
                }
            }

            // ----------------------------------------------------------
            // Monospace surface: gutter, tone-coloured lines, selection.
            // ----------------------------------------------------------
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                DsCard {
                    id: logCard
                    anchors.fill: parent
                    padding: Design.space2

                    content: ListView {
                        id: logList
                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.max(0, logCard.height - logCard.padding * 2)
                        clip: true
                        spacing: 0
                        model: page.visibleLines
                        boundsBehavior: Flickable.StopAtBounds
                        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                        delegate: Item {
                            id: logRow

                            required property var modelData

                            width: logList.width
                            height: Math.max(18, logText.contentHeight) + 2

                            Row {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                spacing: Design.space3

                                Text {
                                    id: lineNumber
                                    width: page.gutterWidth
                                    text: logRow.modelData.lineNumber
                                    color: BeyGramTheme.muted
                                    opacity: Design.opacitySecondary
                                    font.family: page.monoFamily
                                    font.pixelSize: 11
                                    horizontalAlignment: Text.AlignRight
                                }

                                TextEdit {
                                    id: logText
                                    width: Math.max(0, logRow.width - lineNumber.width - parent.spacing)
                                    height: contentHeight
                                    text: logRow.modelData.text
                                    color: page.levelColor(logRow.modelData.level)
                                    font.family: page.monoFamily
                                    font.pixelSize: 11
                                    readOnly: true
                                    selectByMouse: true
                                    persistentSelection: true
                                    activeFocusOnPress: false
                                    cursorVisible: false
                                    selectionColor: BeyGramTheme.accent
                                    selectedTextColor: BeyGramTheme.onAccent
                                    textFormat: TextEdit.PlainText
                                    wrapMode: TextEdit.WrapAnywhere
                                    textMargin: 0
                                    padding: 0
                                    leftPadding: 0
                                    rightPadding: 0
                                    topPadding: 0
                                    bottomPadding: 0
                                }
                            }
                        }
                    }
                }

                Text {
                    anchors.centerIn: parent
                    width: Math.max(0, Math.min(360, parent.width - Design.space8))
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    visible: logList.count === 0
                    text: page.query.length > 0 && page.lines.length > 0
                          ? "No log lines match the filter."
                          : "No log lines yet."
                    color: BeyGramTheme.muted
                    font.pixelSize: Design.fontSmall
                }
            }

            Text {
                Layout.fillWidth: true
                text: "File: " + BeyGramApp.logDirectory + "\\beygram.log · keeps 3 rotated files, 5 MiB each"
                color: BeyGramTheme.muted
                font.pixelSize: Design.fontCaption
                elide: Text.ElideMiddle
            }
        }
    }

    Connections {
        target: BeyGramApp
        function onStateChanged() { page.reload() }
    }

    Component.onCompleted: reload()
}
