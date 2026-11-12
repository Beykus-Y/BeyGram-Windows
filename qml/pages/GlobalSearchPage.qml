import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQml
import BeyGram.Theme 1.0
import BeyGram.App 1.0
import "../ds/design.js" as Design

// GlobalSearchPage — focused desktop search hub (Ctrl+K).
Item {
    id: page

    property var model: null
    property bool loading: false
    property bool hasMore: false
    property string errorText: BeyGramApp.chatError

    property string appliedQuery: ""
    readonly property string query: field.text

    // 0 = All, 1 = Chats, 2 = Messages, 3 = Contacts
    property int category: 0
    property int highlightedIndex: -1

    readonly property var categoryItems: [
        { id: 0, label: qsTr("All") },
        { id: 1, label: qsTr("Chats") },
        { id: 2, label: qsTr("Messages") },
        { id: 3, label: qsTr("Contacts") }
    ]

    signal searchRequested(string query)
    signal loadMoreRequested()
    signal resultActivated(var chatId, var messageId)
    signal chatSelected(var chatId)

    function focusSearch() {
        field.forceActiveFocus()
        field.selectAll()
    }

    // Alias kept for existing Shell.qml callers.
    function focusInput() {
        focusSearch()
    }

    function clear() {
        debounce.stop()
        field.text = ""
        page.highlightedIndex = -1
        emitQuery("")
    }

    function emitQuery(value) {
        if (value === page.appliedQuery)
            return
        page.appliedQuery = value
        page.highlightedIndex = -1
        page.searchRequested(value)
    }

    function submitQuery() {
        debounce.stop()
        emitQuery(field.text.trim())
    }

    function dateLabel(value) {
        const ts = Number(value)
        if (!isFinite(ts) || ts <= 0)
            return ""
        const _rev = BeyGramLang.revision
        return BeyGramLang.formatChatTimestamp(ts)
    }

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

    readonly property string highlightInk: BeyGramTheme.accentInk.toString()

    function highlighted(value) {
        const source = page.plainText(value)
        if (source.length === 0)
            return ""

        const needle = page.query.trim()
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
        return sender.length > 0 ? sender : qsTr("Message")
    }

    function initialsFor(value) {
        const text = page.plainText(value)
        if (text.length === 0)
            return "?"
        return Array.from(text)[0].toUpperCase()
    }

    function accentFor(value) {
        const id = Number(value)
        if (!isFinite(id))
            return 0
        return Math.abs(Math.floor(id)) % 6
    }

    // Reactive local chat list snapshot for instant "Chats" filtering.
    property var localChats: []
    function rebuildLocalChats() {
        const list = []
        for (let i = 0; i < chatInstantiator.count; ++i) {
            const obj = chatInstantiator.objectAt(i)
            if (!obj)
                continue
            list.push({
                chatId: obj["chatId"],
                title: obj["title"],
                preview: obj["preview"],
                lastDate: obj["lastDate"],
                initials: obj["initials"],
                accentIndex: obj["accentIndex"]
            })
        }
        page.localChats = list
    }

    Instantiator {
        id: chatInstantiator
        model: BeyGramApp.chatList
        delegate: QtObject {
            required property var chatId
            required property string title
            required property string preview
            required property var lastDate
            required property string initials
            required property int accentIndex
        }
        onObjectAdded: Qt.callLater(page.rebuildLocalChats)
        onObjectRemoved: Qt.callLater(page.rebuildLocalChats)
        onCountChanged: Qt.callLater(page.rebuildLocalChats)
    }

    readonly property var matchingChats: {
        const q = page.query.trim().toLowerCase()
        if (q.length === 0)
            return page.localChats.slice(0, 12)
        const out = []
        for (let i = 0; i < page.localChats.length; ++i) {
            const c = page.localChats[i]
            if (String(c.title).toLowerCase().indexOf(q) >= 0
                    || String(c.preview).toLowerCase().indexOf(q) >= 0) {
                out.push(c)
            }
        }
        return out
    }

    readonly property var matchingContacts: {
        const names = BeyGramApp.contacts ? BeyGramApp.contacts.names : []
        const q = page.query.trim().toLowerCase()
        const out = []
        for (let i = 0; i < names.length; ++i) {
            const name = String(names[i])
            if (q.length === 0 || name.toLowerCase().indexOf(q) >= 0)
                out.push({ index: i, name: name })
        }
        return out
    }

    readonly property bool showingMessages: page.category === 0 || page.category === 2
    readonly property bool showingChats: page.category === 1
    readonly property bool showingContacts: page.category === 3

    readonly property int activeListCount: {
        if (page.showingChats)
            return localChatsList.count
        if (page.showingContacts)
            return localContactsList.count
        return resultsList.count
    }

    readonly property string resultSummary: {
        if (page.showingMessages && page.loading && resultsList.count === 0)
            return qsTr("Searching…")
        if (page.activeListCount === 0)
            return qsTr("Search across chats, messages and contacts")
        return qsTr("%n result(s)", "", page.activeListCount)
    }

    function activeListView() {
        if (page.showingChats)
            return localChatsList
        if (page.showingContacts)
            return localContactsList
        return resultsList
    }

    function moveHighlight(delta) {
        const count = page.activeListCount
        if (count <= 0) {
            page.highlightedIndex = -1
            return
        }
        let next = page.highlightedIndex + delta
        if (next < 0)
            next = count - 1
        else if (next >= count)
            next = 0
        page.highlightedIndex = next
        page.activeListView().positionViewAtIndex(next, ListView.Contain)
    }

    function activateHighlighted() {
        const idx = page.highlightedIndex
        if (idx < 0 || idx >= page.activeListCount) {
            page.submitQuery()
            return
        }
        if (page.showingChats) {
            const row = page.matchingChats[idx]
            if (row)
                page.openSearchChat(row.chatId, 0)
        } else if (page.showingContacts) {
            if (BeyGramApp.contacts)
                BeyGramApp.contacts.activate(idx)
        } else {
            const item = resultsList.itemAtIndex(idx)
            if (item)
                page.openSearchChat(item.chatId, item.messageId)
        }
    }

    function openSearchChat(chatId, messageId) {
        if (page.appliedQuery.length > 0 && BeyGramApp.search)
            BeyGramApp.search.begin(page.appliedQuery)
        page.resultActivated(chatId, messageId)
        page.chatSelected(chatId)
    }

    Timer {
        id: debounce
        interval: 320
        repeat: false
        onTriggered: page.submitQuery()
    }

    Component.onCompleted: {
        rebuildLocalChats()
        if (page.visible)
            Qt.callLater(page.focusSearch)
    }

    onVisibleChanged: {
        if (page.visible)
            Qt.callLater(page.focusSearch)
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        DsHeader {
            Layout.fillWidth: true
            title: qsTr("Search")
            subtitle: page.resultSummary
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ColumnLayout {
                anchors.fill: parent
                anchors.leftMargin: Math.max(Design.space6, (parent.width - 920) / 2)
                anchors.rightMargin: Math.max(Design.space6, (parent.width - 920) / 2)
                anchors.topMargin: Design.space4
                anchors.bottomMargin: Design.space4
                spacing: Design.space3

                DsSearchField {
                    id: field

                    Layout.fillWidth: true
                    Layout.preferredHeight: Design.controlHeightLarge
                    implicitHeight: Design.controlHeightLarge
                    leftPadding: Design.space8 + Design.space3
                    rightPadding: field.text.length > 0 ? 46 : Design.space5
                    placeholderText: qsTr("Search chats, messages and contacts…")
                    font.pixelSize: Design.fontBodyLarge

                    onTextEdited: {
                        if (field.text.trim().length === 0) {
                            debounce.stop()
                            page.emitQuery("")
                        } else {
                            debounce.restart()
                        }
                    }
                    onAccepted: page.activateHighlighted()
                    onCleared: {
                        debounce.stop()
                        page.highlightedIndex = -1
                        page.emitQuery("")
                    }

                    Keys.onDownPressed: (event) => {
                        page.moveHighlight(1)
                        event.accepted = true
                    }
                    Keys.onUpPressed: (event) => {
                        page.moveHighlight(-1)
                        event.accepted = true
                    }
                    Keys.onReturnPressed: (event) => {
                        page.activateHighlighted()
                        event.accepted = true
                    }
                    Keys.onEnterPressed: (event) => {
                        page.activateHighlighted()
                        event.accepted = true
                    }
                    Keys.onEscapePressed: (event) => {
                        if (field.text.length > 0) {
                            page.clear()
                            event.accepted = true
                        }
                    }
                }

                // Category chips: All, Chats, Messages, Contacts
                Flow {
                    Layout.fillWidth: true
                    spacing: Design.space2

                    Repeater {
                        model: page.categoryItems

                        delegate: DsChip {
                            required property var modelData
                            text: modelData.label
                            selected: page.category === modelData.id
                            onClicked: {
                                page.category = modelData.id
                                page.highlightedIndex = -1
                            }
                        }
                    }
                }

                // Optional recent search suggestions on initial empty query
                SearchSuggestions {
                    Layout.fillWidth: true
                    visible: page.query.trim().length === 0 && page.showingMessages
                    facade: BeyGramApp.search
                }

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    // 1) Global message search results (All / Messages)
                    ListView {
                        id: resultsList

                        anchors.fill: parent
                        visible: page.showingMessages
                        clip: true
                        spacing: Design.space1
                        model: page.model
                        boundsBehavior: Flickable.StopAtBounds
                        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                        delegate: DsListRow {
                            id: resultRow

                            required property int index
                            required property var chatId
                            required property var messageId
                            required property var date
                            required property string chatTitle
                            required property string senderName
                            required property string text

                            width: resultsList.width
                            selected: resultRow.index === page.highlightedIndex
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
                                    text: qsTr("Message")
                                    tone: "muted"
                                }

                                Text {
                                    Layout.alignment: Qt.AlignRight
                                    text: page.dateLabel(resultRow.date)
                                    color: BeyGramTheme.muted
                                    font.pixelSize: Design.fontCaption
                                }
                            }

                            onClicked: page.openSearchChat(resultRow.chatId,
                                                           resultRow.messageId)
                        }
                    }

                    // 2) Local chats list (Chats)
                    ListView {
                        id: localChatsList

                        anchors.fill: parent
                        visible: page.showingChats
                        clip: true
                        spacing: Design.space1
                        model: page.matchingChats
                        boundsBehavior: Flickable.StopAtBounds
                        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                        delegate: DsListRow {
                            id: chatResultRow

                            required property int index
                            required property var modelData

                            width: localChatsList.width
                            selected: chatResultRow.index === page.highlightedIndex
                            title: chatResultRow.modelData.title
                            subtitle: page.highlighted(chatResultRow.modelData.preview)

                            leading: DsAvatar {
                                size: Design.avatarSmall
                                initials: chatResultRow.modelData.initials
                                accentIndex: chatResultRow.modelData.accentIndex
                            }

                            trailing: ColumnLayout {
                                spacing: Design.space1

                                DsBadge {
                                    Layout.alignment: Qt.AlignRight
                                    text: qsTr("Chat")
                                    tone: "accent"
                                }

                                Text {
                                    Layout.alignment: Qt.AlignRight
                                    text: page.dateLabel(chatResultRow.modelData.lastDate)
                                    color: BeyGramTheme.muted
                                    font.pixelSize: Design.fontCaption
                                }
                            }

                            onClicked: page.openSearchChat(chatResultRow.modelData.chatId, 0)
                        }
                    }

                    // 3) Contacts list (Contacts)
                    ListView {
                        id: localContactsList

                        anchors.fill: parent
                        visible: page.showingContacts
                        clip: true
                        spacing: Design.space1
                        model: page.matchingContacts
                        boundsBehavior: Flickable.StopAtBounds
                        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                        delegate: DsListRow {
                            id: contactResultRow

                            required property int index
                            required property var modelData

                            width: localContactsList.width
                            selected: contactResultRow.index === page.highlightedIndex
                            title: contactResultRow.modelData.name

                            leading: DsAvatar {
                                size: Design.avatarSmall
                                initials: page.initialsFor(contactResultRow.modelData.name)
                                accentIndex: contactResultRow.index % 6
                            }

                            trailing: DsBadge {
                                text: qsTr("Contact")
                                tone: "muted"
                            }

                            onClicked: {
                                if (BeyGramApp.contacts)
                                    BeyGramApp.contacts.activate(contactResultRow.modelData.index)
                            }
                        }
                    }

                    // Loading state
                    DsSpinner {
                        anchors.centerIn: parent
                        running: page.showingMessages && page.loading && resultsList.count === 0
                        visible: running
                    }

                    // Error + Retry state
                    DsEmptyState {
                        anchors.centerIn: parent
                        visible: page.showingMessages && !page.loading
                                 && resultsList.count === 0
                                 && page.errorText.length > 0
                                 && page.appliedQuery.length > 0
                        glyph: "\uE783"
                        title: qsTr("Search failed")
                        description: page.errorText
                        actionText: qsTr("Retry")
                        onActionRequested: page.submitQuery()
                    }

                    // Welcoming initial empty state
                    DsEmptyState {
                        anchors.centerIn: parent
                        visible: page.showingMessages && !page.loading
                                 && resultsList.count === 0
                                 && page.query.trim().length === 0
                                 && page.errorText.length === 0
                        glyph: "\uE721"
                        title: qsTr("Search messages")
                        description: qsTr("Type a word or phrase to search every conversation. Use Up/Down and Enter to jump straight in.")
                    }

                    // Empty no-match state
                    DsEmptyState {
                        anchors.centerIn: parent
                        visible: !page.loading && page.activeListCount === 0
                                 && (page.errorText.length === 0 || !page.showingMessages)
                                 && (page.query.trim().length > 0 || page.showingContacts || page.showingChats)
                        glyph: "\uE721"
                        title: qsTr("No results")
                        description: page.showingContacts && page.query.trim().length === 0
                                     ? qsTr("Contacts are not synced to the local list yet.")
                                     : qsTr("Nothing matched \"%1\". Try another keyword or switch category.")
                                       .arg(page.query.trim().replace(/[<>]/g, ""))
                    }
                }

                DsButton {
                    Layout.alignment: Qt.AlignHCenter
                    visible: page.showingMessages && page.hasMore
                    text: qsTr("Load more")
                    tone: "secondary"
                    size: "sm"
                    loading: page.loading
                    onClicked: page.loadMoreRequested()
                }
            }
        }
    }
}
