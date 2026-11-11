// GlobalSearchPage — desktop-wide message search.
//
// The parent owns the data layer. It assigns `model` from GlobalSearchModel
// (roles: chatId, messageId, date, chatTitle, senderName, text, textHtml),
// keeps `loading` and `hasMore` in sync, starts or replaces the TDLib
// MessageSearch on searchRequested(query), and appends the next page on
// loadMoreRequested(). An empty query ("") clears the result model. `date` is
// expected in Unix seconds, matching the chat list and message models.
//
// This page only debounces input and renders results: no C++ singletons other
// than theme tokens, no network access, no logging.

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0

Item {
    id: page

    property var model: null
    property bool loading: false
    property bool hasMore: false

    // Trimmed query of the last searchRequested() emission. Used to tell the
    // "no query" empty state apart from a finished search with no matches.
    property string appliedQuery: ""
    readonly property string query: field.text

    signal searchRequested(string query)
    signal loadMoreRequested()
    signal resultActivated(int chatId, int messageId)

    function focusInput() {
        field.forceActiveFocus()
    }

    function clear() {
        debounce.stop()
        field.text = ""
        emitQuery("")
    }

    function emitQuery(value) {
        if (value === page.appliedQuery)
            return
        page.appliedQuery = value
        page.searchRequested(value)
    }

    function submitQuery() {
        debounce.stop()
        emitQuery(field.text.trim())
    }

    function dateLabel(value) {
        if (!value || value <= 0)
            return ""
        const date = new Date(value * 1000)
        if (isNaN(date.getTime()))
            return ""
        return Qt.formatDateTime(date, "dd.MM.yyyy hh:mm")
    }

    Timer {
        id: debounce
        interval: 320
        repeat: false
        onTriggered: page.submitQuery()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 32
        spacing: 14

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4

            Text {
                text: "Search"
                color: BeyGramTheme.text
                font.pixelSize: 26
                font.weight: Font.DemiBold
            }

            Text {
                Layout.fillWidth: true
                text: "Search messages across all chats"
                color: BeyGramTheme.muted
                font.pixelSize: 13
                elide: Text.ElideRight
            }
        }

        TextField {
            id: field
            Layout.fillWidth: true
            Layout.preferredHeight: 44
            implicitHeight: 44
            leftPadding: 18
            rightPadding: 18
            placeholderText: "Search messages"
            placeholderTextColor: Qt.alpha(BeyGramTheme.muted, 0.75)
            color: BeyGramTheme.text
            font.pixelSize: 14
            selectByMouse: true
            selectionColor: BeyGramTheme.accent
            selectedTextColor: BeyGramTheme.onAccent
            onTextEdited: {
                if (field.text.trim().length === 0) {
                    debounce.stop()
                    page.emitQuery("")
                } else {
                    debounce.restart()
                }
            }
            onAccepted: page.submitQuery()

            background: Rectangle {
                radius: 22
                color: BeyGramTheme.surface2
                border.width: 1
                border.color: field.activeFocus ? BeyGramTheme.accent : BeyGramTheme.line
                Behavior on border.color { ColorAnimation { duration: 120 } }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ListView {
                id: resultsList
                anchors.fill: parent
                clip: true
                spacing: 2
                model: page.model
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                delegate: Item {
                    id: row
                    // Roles are qint64 in GlobalSearchModel; keep them as var so
                    // a large chat id does not fail the delegate binding.
                    required property var chatId
                    required property var messageId
                    required property var date
                    required property string chatTitle
                    required property string senderName
                    required property string text

                    readonly property string metaText: {
                        const title = row.chatTitle || ""
                        const sender = row.senderName || ""
                        if (title.length > 0 && sender.length > 0)
                            return title + " · " + sender
                        return title.length > 0 ? title : sender
                    }

                    width: resultsList.width
                    height: 64

                    Rectangle {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        radius: 10
                        color: hoverArea.containsMouse ? BeyGramTheme.surface2 : "transparent"
                        Behavior on color { ColorAnimation { duration: 120 } }
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 18
                        anchors.rightMargin: 18
                        anchors.topMargin: 10
                        anchors.bottomMargin: 10
                        spacing: 12

                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            spacing: 3

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 10

                                Text {
                                    Layout.fillWidth: true
                                    text: row.metaText
                                    color: BeyGramTheme.muted
                                    font.pixelSize: 11
                                    elide: Text.ElideRight
                                }

                                Text {
                                    text: page.dateLabel(row.date)
                                    color: BeyGramTheme.muted
                                    font.pixelSize: 11
                                }
                            }

                            Text {
                                Layout.fillWidth: true
                                text: row.text
                                color: BeyGramTheme.text
                                font.pixelSize: 13
                                elide: Text.ElideRight
                                maximumLineCount: 1
                                wrapMode: Text.NoWrap
                            }
                        }
                    }

                    MouseArea {
                        id: hoverArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: page.resultActivated(row.chatId, row.messageId)
                    }
                }

                footer: Item {
                    width: resultsList.width
                    height: (page.hasMore || (page.loading && resultsList.count > 0)) ? 56 : 0

                    RowLayout {
                        anchors.centerIn: parent
                        spacing: 10

                        BusyIndicator {
                            running: page.loading && resultsList.count > 0
                            visible: running
                            implicitWidth: 22
                            implicitHeight: 22
                        }

                        Rectangle {
                            Layout.preferredWidth: 150
                            Layout.preferredHeight: 36
                            radius: 18
                            visible: page.hasMore
                            opacity: page.loading ? 0.5 : 1.0
                            color: loadMoreArea.containsMouse && !page.loading
                                   ? BeyGramTheme.surface2
                                   : "transparent"
                            border.width: 1
                            border.color: BeyGramTheme.line

                            Text {
                                anchors.centerIn: parent
                                text: "Load more"
                                color: BeyGramTheme.accent
                                font.pixelSize: 12
                                font.weight: Font.DemiBold
                            }

                            MouseArea {
                                id: loadMoreArea
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    if (!page.loading)
                                        page.loadMoreRequested()
                                }
                            }
                        }
                    }
                }
            }

            BusyIndicator {
                anchors.centerIn: parent
                running: page.loading && resultsList.count === 0
                visible: running
                implicitWidth: 32
                implicitHeight: 32
            }

            Text {
                anchors.centerIn: parent
                width: Math.max(0, Math.min(360, parent.width - 64))
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                visible: !page.loading && resultsList.count === 0
                         && field.text.trim().length === 0
                text: "Type a word or phrase to search messages in every chat."
                color: BeyGramTheme.muted
                font.pixelSize: 13
            }

            Text {
                anchors.centerIn: parent
                visible: !page.loading && resultsList.count === 0
                         && field.text.trim().length > 0
                         && page.appliedQuery.length > 0
                text: "No results"
                color: BeyGramTheme.muted
                font.pixelSize: 13
            }
        }
    }
}
