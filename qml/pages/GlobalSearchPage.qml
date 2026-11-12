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
//
// Layout: DsHeader, a large DsSearchField, optional DsTabs for the result
// categories, DsListRow results (DsAvatar leading, DsBadge for the kind), an
// empty state or spinner, and a "Load more" button pinned under the list.
//
// Category tabs need row-level access, which QML only has for a ListModel or
// a JS array of objects; a plain QAbstractItemModel exposes no role to QML, so
// it stays one unfiltered list and the tabs are hidden.

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0
import "../ds/design.js" as Design

Item {
    id: page

    property var model: null
    property bool loading: false
    property bool hasMore: false

    // Trimmed query of the last searchRequested() emission. Used to tell the
    // "no query" empty state apart from a finished search with no matches.
    property string appliedQuery: ""
    readonly property string query: field.text

    // 0 = All, 1 = Chats, 2 = Messages, 3 = Files. Reset by every new search.
    property int category: 0

    readonly property var categoryLabels: ["All", "Chats", "Messages", "Files"]

    signal searchRequested(string query)
    signal loadMoreRequested()
    signal resultActivated(var chatId, var messageId)

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
        page.category = 0
        categoryTabs.currentIndex = 0
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

    // --- Model access --------------------------------------------------------
    // ListModel and JS arrays can be read row by row from QML; a QAbstractItemModel
    // cannot, so only those two can back the category tabs.
    function rowAt(index) {
        if (!page.model || index < 0)
            return null
        if (typeof page.model.get === "function")
            return page.model.get(index) || null
        if (typeof page.model.length === "number")
            return page.model[index] || null
        return null
    }

    readonly property int modelCount: {
        if (!page.model)
            return 0
        if (typeof page.model.count === "number")
            return page.model.count
        if (typeof page.model.length === "number")
            return page.model.length
        return 0
    }

    readonly property bool kindAvailable: {
        const total = page.modelCount // re-probe when the first page arrives
        if (total === 0)
            return false
        const row = page.rowAt(0)
        return row !== null && row.kind !== undefined && row.kind !== null
    }

    function kindOf(value) {
        const key = String(value === undefined || value === null ? "" : value)
                        .toLowerCase()
        if (key.indexOf("chat") === 0 || key === "user" || key === "group"
                || key === "channel" || key === "contact")
            return "chats"
        if (key.indexOf("file") === 0 || key === "media" || key === "document"
                || key === "photo" || key === "video" || key === "audio"
                || key === "voice" || key === "sticker")
            return "files"
        return "messages"
    }

    function kindMatches(value, categoryIndex) {
        if (categoryIndex <= 0)
            return true
        const wanted = categoryIndex === 1 ? "chats"
                     : (categoryIndex === 2 ? "messages" : "files")
        return kindOf(value) === wanted
    }

    function kindLabel(value) {
        const key = kindOf(value)
        return key === "chats" ? "Chat" : (key === "files" ? "File" : "Message")
    }

    function kindTone(value) {
        return kindOf(value) === "messages" ? "muted" : "accent"
    }

    // The model itself when no filtering applies, so a C++ model keeps its live
    // updates and paging; a plain JS array of the matching rows otherwise.
    readonly property var viewModel: {
        const total = page.modelCount // dependency: re-run when a page arrives
        if (!page.kindAvailable || page.category <= 0)
            return page.model
        const rows = []
        for (let i = 0; i < total; ++i) {
            const row = page.rowAt(i)
            if (row !== null && page.kindMatches(row.kind, page.category))
                rows.push(row)
        }
        return rows
    }

    // --- Text ----------------------------------------------------------------

    readonly property string highlightInk: BeyGramTheme.accentInk.toString()

    function plainText(value) {
        return String(value === undefined || value === null ? "" : value)
                .replace(/\s+/g, " ").trim()
    }

    function escapeHtml(value) {
        return value.replace(/&/g, "&amp;")
                    .replace(/</g, "&lt;")
                    .replace(/>/g, "&gt;")
                    .replace(/"/g, "&quot;")
    }

    // DsListRow draws `subtitle` with Text.AutoText, so the snippet is always a
    // <font> wrapper: the message body stays literal and every occurrence of the
    // applied query is wrapped in an accentInk <font>.
    function highlighted(value) {
        const source = page.plainText(value)
        if (source.length === 0)
            return ""

        const needle = page.appliedQuery.trim()
        const haystack = source.toLowerCase()
        const target = needle.toLowerCase()

        let out = ""
        let from = 0
        if (target.length > 0) {
            while (true) {
                const at = haystack.indexOf(target, from)
                if (at < 0)
                    break
                out += page.escapeHtml(source.slice(from, at))
                out += '<font color="' + page.highlightInk + '">'
                       + page.escapeHtml(source.slice(at, at + needle.length))
                       + "</font>"
                from = at + needle.length
            }
        }
        out += page.escapeHtml(source.slice(from))
        return "<font>" + out + "</font>"
    }

    function rowTitle(chatTitle, senderName) {
        const chat = page.plainText(chatTitle)
        const sender = page.plainText(senderName)
        if (chat.length > 0 && sender.length > 0 && chat !== sender)
            return chat + " · " + sender
        if (chat.length > 0)
            return chat
        return sender.length > 0 ? sender : "Message"
    }

    function initialsFor(value) {
        const text = page.plainText(value)
        if (text.length === 0)
            return "?"
        // Array.from keeps a surrogate pair (an emoji title) intact.
        return Array.from(text)[0].toUpperCase()
    }

    function accentFor(value) {
        const id = Number(value)
        if (!isFinite(id))
            return 0
        return Math.abs(Math.floor(id)) % 6
    }

    readonly property string resultSummary: {
        if (page.loading && resultsList.count === 0)
            return "Searching…"
        if (resultsList.count === 0)
            return "Messages across all chats"
        return resultsList.count === 1 ? "1 result" : resultsList.count + " results"
    }

    Timer {
        id: debounce
        interval: 320
        repeat: false
        onTriggered: page.submitQuery()
    }

    Component.onCompleted: {
        if (page.visible)
            Qt.callLater(page.focusInput)
    }

    onVisibleChanged: {
        if (page.visible)
            Qt.callLater(page.focusInput)
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        DsHeader {
            Layout.fillWidth: true
            title: "Search"
            subtitle: page.resultSummary
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ColumnLayout {
                anchors.fill: parent
                // Centred column with a readable measure on wide windows.
                anchors.leftMargin: Math.max(Design.space6, (parent.width - 960) / 2)
                anchors.rightMargin: Math.max(Design.space6, (parent.width - 960) / 2)
                anchors.topMargin: Design.space4
                anchors.bottomMargin: Design.space4
                spacing: Design.space4

                DsSearchField {
                    id: field

                    Layout.fillWidth: true
                    Layout.preferredHeight: Design.controlHeightLarge
                    implicitHeight: Design.controlHeightLarge
                    leftPadding: Design.space8 + Design.space3
                    rightPadding: field.text.length > 0 ? 46 : Design.space5
                    placeholderText: "Search messages"
                    font.pixelSize: Design.fontBodyLarge

                    onTextEdited: {
                        if (field.text.trim().length === 0) {
                            debounce.stop()
                            page.emitQuery("")
                        } else {
                            debounce.restart()
                        }
                    }
                    onAccepted: page.submitQuery()
                    onCleared: {
                        debounce.stop()
                        page.emitQuery("")
                    }
                }

                DsTabs {
                    id: categoryTabs

                    Layout.fillWidth: true
                    visible: page.kindAvailable
                    model: page.categoryLabels
                    currentIndex: page.category
                    onActivated: (index) => {
                        page.category = index
                    }
                }

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    ListView {
                        id: resultsList

                        anchors.fill: parent
                        clip: true
                        spacing: Design.space1
                        model: page.viewModel
                        boundsBehavior: Flickable.StopAtBounds
                        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                        delegate: DsListRow {
                            id: resultRow

                            // Roles are qint64 in GlobalSearchModel; keep them as
                            // var so a large chat id does not fail the binding.
                            required property var chatId
                            required property var messageId
                            required property var date
                            required property string chatTitle
                            required property string senderName
                            required property string text
                            // Optional: only present when the model carries one.
                            property var kind

                            width: resultsList.width
                            title: page.rowTitle(resultRow.chatTitle,
                                                 resultRow.senderName)
                            subtitle: page.highlighted(resultRow.text)

                            Accessible.name: resultRow.title
                            Accessible.description: page.plainText(resultRow.text)

                            leading: DsAvatar {
                                size: Design.avatarSmall
                                initials: page.initialsFor(resultRow.chatTitle)
                                accentIndex: page.accentFor(resultRow.chatId)
                            }

                            trailing: ColumnLayout {
                                spacing: Design.space1

                                DsBadge {
                                    Layout.alignment: Qt.AlignRight
                                    text: page.kindLabel(resultRow.kind)
                                    tone: page.kindTone(resultRow.kind)
                                }

                                Text {
                                    Layout.alignment: Qt.AlignRight
                                    text: page.dateLabel(resultRow.date)
                                    color: BeyGramTheme.muted
                                    font.pixelSize: Design.fontCaption
                                }
                            }

                            onClicked: page.resultActivated(resultRow.chatId,
                                                            resultRow.messageId)
                        }
                    }

                    DsSpinner {
                        anchors.centerIn: parent
                        running: page.loading && resultsList.count === 0
                        visible: running
                    }

                    DsEmptyState {
                        anchors.centerIn: parent
                        visible: !page.loading && resultsList.count === 0
                                 && page.query.trim().length === 0
                        glyph: "🔍"
                        title: "Search messages"
                        description: "Type a word or phrase to search every chat."
                    }

                    DsEmptyState {
                        anchors.centerIn: parent
                        visible: !page.loading && resultsList.count === 0
                                 && page.query.trim().length > 0
                                 && page.appliedQuery.length > 0
                        title: "No results"
                        description: "Nothing matched \""
                                     + page.appliedQuery.replace(/[<>]/g, "")
                                     + "\". Try a different word or spelling."
                    }
                }

                DsButton {
                    Layout.alignment: Qt.AlignHCenter
                    visible: page.hasMore
                    text: "Load more"
                    tone: "secondary"
                    size: "sm"
                    loading: page.loading
                    onClicked: page.loadMoreRequested()
                }
            }
        }
    }
}
