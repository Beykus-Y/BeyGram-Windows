import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import BeyGram.Theme 1.0
import BeyGram.App 1.0
import "../ds/design.js" as Design

// ChatsPage — two-pane conversations screen.
//
// Wide windows (>= 900 px) show the chat list at 320 px next to the open
// conversation. Narrower windows collapse to one pane: the list until a chat
// is opened, then the conversation with a back action in its header.
//
// The page owns drafts, reply/edit state and the in-chat search timer; every
// message/chat request still goes through BeyGramApp. Geometry comes from
// design.js and colours from BeyGramTheme, so the page follows the active
// preset and Light/Dark/AMOLED mode.
Item {
    id: page

    // --- Public surface (kept for callers) ---------------------------------
    property var editingId: 0
    property var loadedChatId: 0
    property bool searchOpen: false
    property var forwardMessageId: 0

    readonly property int wideBreakpoint: 900
    readonly property bool wideLayout: page.width >= page.wideBreakpoint
    readonly property int chatListWidth: Design.space10 * 8 // 320

    readonly property var chatListModel: BeyGramApp.archiveOpen && BeyGramApp.archivedChats
                                         ? BeyGramApp.archivedChats
                                         : BeyGramApp.chatList
    readonly property bool chatListLoading: BeyGramApp.archiveOpen
                                            ? BeyGramApp.archiveLoading
                                            : BeyGramApp.chatLoading
    readonly property int chatListCount: page.chatListModel
                                         ? Number(page.chatListModel.count) : 0
    readonly property string chatListTitle: BeyGramApp.archiveOpen ? "Archive" : "Chats"
    readonly property string chatListSubtitle: {
        if (BeyGramApp.archiveOpen) {
            return page.chatListCount === 1
                    ? "1 archived chat" : page.chatListCount + " archived chats"
        }
        return page.chatListCount === 1 ? "1 chat" : page.chatListCount + " chats"
    }

    // --- Public functions (kept) -------------------------------------------
    function openSearch() {
        searchOpen = true
        searchField.forceActiveFocus()
    }

    function closeSearch() {
        searchOpen = false
        searchField.text = ""
        searchTimer.stop()
        BeyGramApp.clearSearch()
    }

    function focusChatSearch() {
        chatSearchField.forceActiveFocus()
        chatSearchField.cursorPosition = chatSearchField.length
    }

    function flushDraft() {
        draftTimer.stop()
        const id = draftTimer.chatId
        if (id > 0 && editingId === 0) {
            if (composer.text.trim().length > 0)
                BeyGramApp.saveDraft(id, composer.text, BeyGramApp.replyToMessageId)
            else
                BeyGramApp.clearDraft(id)
        }
    }

    function scheduleDraft() {
        if (editingId > 0 || BeyGramApp.currentChatId === 0)
            return
        draftTimer.chatId = BeyGramApp.currentChatId
        draftTimer.restart()
    }

    function loadDraft() {
        const id = BeyGramApp.currentChatId
        loadedChatId = id
        editingId = 0
        if (id === 0) {
            composer.text = ""
            return
        }
        draftTimer.chatId = id
        composer.text = BeyGramApp.draftText(id)
    }

    function resetComposer() {
        flushDraft()
        editingId = 0
        composer.text = ""
        BeyGramApp.cancelReply()
        loadedChatId = 0
    }

    function beginEdit(messageId, text) {
        flushDraft()
        editingId = messageId
        BeyGramApp.cancelReply()
        composer.text = text
        composer.forceActiveFocus()
        composer.cursorPosition = composer.length
    }

    function submitComposer() {
        const text = composer.text.trim()
        if (text.length === 0)
            return
        if (editingId > 0) {
            BeyGramApp.editChatMessage(editingId, text)
        } else {
            draftTimer.stop()
            BeyGramApp.clearDraft(BeyGramApp.currentChatId)
            BeyGramApp.sendChatMessage(text)
        }
        composer.text = ""
        editingId = 0
        composer.forceActiveFocus()
    }

    // --- Small formatting helpers ------------------------------------------
    function chatTimeText(timestamp) {
        const value = Number(timestamp)
        if (!value || value <= 0)
            return ""
        const date = new Date(value * 1000)
        const now = new Date()
        if (date.toDateString() === now.toDateString())
            return Qt.formatTime(date, "hh:mm")
        return Qt.formatDate(date, "dd.MM")
    }

    function dayStart(timestamp) {
        const value = Number(timestamp)
        if (!value || value <= 0)
            return -1
        const date = new Date(value * 1000)
        return new Date(date.getFullYear(), date.getMonth(), date.getDate()).getTime()
    }

    function dayLabel(timestamp) {
        const value = Number(timestamp)
        if (!value || value <= 0)
            return ""
        const date = new Date(value * 1000)
        const now = new Date()
        const today = new Date(now.getFullYear(), now.getMonth(), now.getDate()).getTime()
        const day = new Date(date.getFullYear(), date.getMonth(), date.getDate()).getTime()
        if (day === today)
            return "Today"
        const yesterday = new Date(now.getFullYear(), now.getMonth(), now.getDate() - 1).getTime()
        if (day === yesterday)
            return "Yesterday"
        return Qt.formatDate(date, "dd.MM.yyyy")
    }

    function avatarSource(fileId, ready, revision) {
        if (!ready || Number(fileId) <= 0)
            return ""
        // DsAvatar applies its own circular mask, so no &round=1 is needed.
        return "image://beygrammedia/" + fileId + "?v=" + revision
    }

    Timer {
        id: draftTimer
        property var chatId: 0
        interval: 450
        repeat: false
        onTriggered: {
            if (page.editingId === 0 && chatId === BeyGramApp.currentChatId
                && composer.text.trim().length > 0) {
                BeyGramApp.saveDraft(chatId, composer.text,
                                     BeyGramApp.replyToMessageId)
            }
        }
    }

    Timer {
        id: searchTimer
        interval: 320
        repeat: false
        onTriggered: {
            const query = searchField.text.trim()
            if (query.length > 0)
                BeyGramApp.searchInChat(query)
            else
                BeyGramApp.clearSearch()
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // ==================================================================
        // Chat list pane
        // ==================================================================
        Rectangle {
            id: chatListPane

            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredWidth: page.wideLayout ? page.chatListWidth : page.width
            Layout.maximumWidth: page.wideLayout ? page.chatListWidth
                                                 : Number.POSITIVE_INFINITY
            color: BeyGramTheme.surface
            visible: page.wideLayout || BeyGramApp.currentChatId === 0

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                DsHeader {
                    Layout.fillWidth: true
                    title: page.chatListTitle
                    subtitle: page.chatListSubtitle

                    leading: DsIconButton {
                        // DsHeader's slots are positioners, so an implicit-only
                        // control needs an explicit size here.
                        width: implicitWidth
                        height: implicitHeight
                        icon: "\uE72B" // Segoe MDL2 Assets: Back
                        size: "sm"
                        tooltip: "Back to chats"
                        visible: BeyGramApp.archiveOpen
                        onClicked: BeyGramApp.closeArchive()
                    }

                    trailing: DsIconButton {
                        width: implicitWidth
                        height: implicitHeight
                        icon: "\uE70F" // Segoe MDL2 Assets: Edit (compose)
                        tooltip: "New message"
                        // The backend has no create-chat call yet, so the
                        // compose entry point focuses the chat search where a
                        // conversation can be picked.
                        onClicked: page.focusChatSearch()
                    }
                }

                // Archive entry point: a single tappable card above the list.
                DsCard {
                    Layout.fillWidth: true
                    Layout.leftMargin: Design.space3
                    Layout.rightMargin: Design.space3
                    Layout.bottomMargin: Design.space2
                    padding: Design.space3
                    interactive: true
                    visible: !BeyGramApp.archiveOpen
                             && BeyGramApp.archivedChats
                             && BeyGramApp.archivedChats.count > 0
                    onClicked: BeyGramApp.openArchive()

                    content: RowLayout {
                        Layout.fillWidth: true
                        spacing: Design.space3

                        DsIcon {
                            Layout.alignment: Qt.AlignVCenter
                            name: "\uE7B8" // Segoe MDL2 Assets: Archive
                            size: Design.iconSize
                            color: BeyGramTheme.accentInk
                            filled: true
                        }

                        Text {
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignVCenter
                            text: "Archived chats"
                            color: BeyGramTheme.text
                            font.pixelSize: Design.fontBody
                            font.weight: Font.Medium
                            elide: Text.ElideRight
                        }

                        DsBadge {
                            Layout.alignment: Qt.AlignVCenter
                            tone: "muted"
                            text: BeyGramApp.archivedChats
                                  ? BeyGramApp.archivedChats.count.toString() : "0"
                        }

                        DsIcon {
                            Layout.alignment: Qt.AlignVCenter
                            name: "\uE76C" // Segoe MDL2 Assets: ChevronRight
                            size: Design.iconSize - Design.space1
                            color: BeyGramTheme.muted
                        }
                    }
                }

                DsSearchField {
                    id: chatSearchField
                    Layout.fillWidth: true
                    Layout.leftMargin: Design.space3
                    Layout.rightMargin: Design.space3
                    Layout.bottomMargin: Design.space2
                    placeholderText: "Search chats"
                    text: BeyGramApp.chatFilter
                    onTextEdited: BeyGramApp.chatFilter = text
                    onCleared: {
                        BeyGramApp.chatFilter = ""
                        // clearText() assigns `text` directly; restore the
                        // binding so an external filter reset still shows.
                        text = Qt.binding(function() { return BeyGramApp.chatFilter })
                    }
                }

                ChatFolderTabs {
                    Layout.fillWidth: true
                    Layout.leftMargin: Design.space3
                    Layout.rightMargin: Design.space3
                    Layout.bottomMargin: Design.space2
                    model: BeyGramApp.chatFolders
                    currentFolderId: BeyGramApp.currentFolderId
                    onFolderSelected: (folderId) => BeyGramApp.selectFolder(folderId)
                }

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    ListView {
                        id: chatListView
                        anchors.fill: parent
                        clip: true
                        spacing: Design.space1
                        model: page.chatListModel
                        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                        delegate: Item {
                            id: chatDelegate

                            width: chatListView.width
                            height: chatRow.implicitHeight

                            readonly property int photoFileId: model.photoFileId

                            // DsChatRow renders the photo from imageSource; the
                            // page keeps requesting it through the app facade.
                            Component.onCompleted: {
                                if (photoFileId > 0)
                                    BeyGramApp.requestChatPhoto(photoFileId)
                            }
                            onPhotoFileIdChanged: {
                                if (photoFileId > 0)
                                    BeyGramApp.requestChatPhoto(photoFileId)
                            }

                            DsChatRow {
                                id: chatRow
                                width: parent.width
                                title: model.title
                                preview: model.preview
                                timeText: page.chatTimeText(model.lastDate)
                                unread: model.unreadCount
                                pinned: model.pinned
                                muted: model.muted
                                initials: model.initials
                                accentIndex: model.accentIndex
                                imageSource: page.avatarSource(model.photoFileId,
                                                               model.photoReady,
                                                               model.photoRevision)
                                selected: BeyGramApp.currentChatId === model.chatId
                                onClicked: {
                                    page.resetComposer()
                                    BeyGramApp.openChat(model.chatId)
                                }
                            }

                            // Right-click menu. Accepted buttons only, so left
                            // clicks and hover continue to reach DsChatRow.
                            MouseArea {
                                anchors.fill: parent
                                acceptedButtons: Qt.RightButton
                                onClicked: {
                                    chatMenu.chatId = model.chatId
                                    chatMenu.chatMuted = model.muted
                                    chatMenu.popup()
                                }
                            }
                        }
                    }

                    DsSpinner {
                        anchors.centerIn: parent
                        running: page.chatListLoading && chatListView.count === 0
                        visible: running
                    }

                    DsEmptyState {
                        anchors.centerIn: parent
                        width: Math.max(0, Math.min(Design.space10 * 7,
                                                     parent.width - Design.space6 * 2))
                        visible: chatListView.count === 0 && !page.chatListLoading
                        glyph: BeyGramApp.archiveOpen ? "\uD83D\uDDC4" : "\uD83D\uDCAC"
                        title: BeyGramApp.chatError.length > 0
                               ? "Chats unavailable"
                               : (BeyGramApp.archiveOpen ? "No archived chats"
                                                         : "No chats yet")
                        description: BeyGramApp.chatError.length > 0
                                     ? BeyGramApp.chatError
                                     : (BeyGramApp.archiveOpen
                                        ? "Chats you archive will appear here."
                                        : "Your Telegram chats will appear here.")
                    }
                }
            }
        }

        // Hairline between the panes on wide windows.
        Item {
            Layout.fillHeight: true
            Layout.preferredWidth: 1
            visible: page.wideLayout

            Rectangle {
                anchors.fill: parent
                color: BeyGramTheme.line
            }
        }

        // ==================================================================
        // Conversation pane
        // ==================================================================
        Rectangle {
            id: conversationPane

            Layout.fillWidth: true
            Layout.fillHeight: true
            color: BeyGramTheme.bg
            visible: page.wideLayout || BeyGramApp.currentChatId !== 0

            ColumnLayout {
                anchors.fill: parent
                spacing: 0
                visible: BeyGramApp.currentChatId !== 0

                DsHeader {
                    Layout.fillWidth: true
                    title: BeyGramApp.currentChatTitle
                    subtitle: BeyGramApp.typingText.length > 0
                              ? BeyGramApp.typingText
                              : BeyGramApp.currentChatSubtitle

                    leading: RowLayout {
                        spacing: Design.space2

                        DsIconButton {
                            Layout.alignment: Qt.AlignVCenter
                            icon: "\uE72B" // Segoe MDL2 Assets: Back
                            size: "sm"
                            tooltip: "Back to chats"
                            visible: !page.wideLayout
                            onClicked: BeyGramApp.closeChat()
                        }

                        Avatar {
                            Layout.alignment: Qt.AlignVCenter
                            size: Design.avatarMedium
                            fileId: BeyGramApp.currentChatPhotoFileId
                            ready: BeyGramApp.currentChatPhotoReady
                            revision: BeyGramApp.currentChatPhotoRevision
                            initials: BeyGramApp.currentChatTitle.length > 0
                                      ? BeyGramApp.currentChatTitle.charAt(0).toUpperCase()
                                      : "?"
                            tint: BeyGramTheme.accentSoft
                            ink: BeyGramTheme.accentInk
                        }
                    }

                    trailing: RowLayout {
                        spacing: Design.space1

                        Pill {
                            Layout.alignment: Qt.AlignVCenter
                            visible: BeyGramApp.connectionTitle !== "Connected"
                            text: BeyGramApp.connectionTitle.toUpperCase()
                            tone: "warn"
                        }

                        DsIconButton {
                            Layout.alignment: Qt.AlignVCenter
                            icon: "\uE721" // Segoe MDL2 Assets: Search
                            tooltip: page.searchOpen ? "Close search"
                                                     : "Search in chat"
                            selected: page.searchOpen
                            onClicked: page.searchOpen ? page.closeSearch()
                                                       : page.openSearch()
                        }

                        DsIconButton {
                            Layout.alignment: Qt.AlignVCenter
                            icon: "\uE77B" // Segoe MDL2 Assets: Contact
                            tooltip: "Profile"
                            onClicked: BeyGramApp.requestProfile(BeyGramApp.currentChatId)
                        }
                    }
                }

                SecretChatBanner {
                    Layout.fillWidth: true
                    facade: BeyGramApp.security
                }

                PinnedMessageBanner {
                    Layout.fillWidth: true
                    visible: BeyGramApp.pinnedVisible
                    title: "Pinned message"
                    preview: BeyGramApp.pinnedPreview
                    onActivated: {}
                    onUnpinRequested: BeyGramApp.unpinPinnedMessage()
                }

                // In-chat search bar. DsSearchField owns the pill and clear
                // affordance; the page keeps the debounce timer.
                Rectangle {
                    Layout.fillWidth: true
                    visible: page.searchOpen
                    implicitHeight: searchBar.implicitHeight + Design.space2 * 2
                    color: BeyGramTheme.surface

                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        height: 1
                        color: BeyGramTheme.line
                    }

                    RowLayout {
                        id: searchBar
                        anchors.fill: parent
                        anchors.leftMargin: Design.space3
                        anchors.rightMargin: Design.space3
                        spacing: Design.space2

                        DsSearchField {
                            id: searchField
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignVCenter
                            placeholderText: "Search in this chat"
                            onTextEdited: searchTimer.restart()
                            onCleared: {
                                searchTimer.stop()
                                BeyGramApp.clearSearch()
                            }
                        }

                        DsSpinner {
                            Layout.alignment: Qt.AlignVCenter
                            running: BeyGramApp.searchLoading
                            visible: running
                        }
                    }
                }

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    ListView {
                        id: messageListView
                        anchors.fill: parent
                        clip: true
                        spacing: 0
                        model: BeyGramApp.messages
                        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                        // One section per timestamp, but the section delegate
                        // only draws when the calendar day changes. Zero-height
                        // sections keep the flow identical to a plain list.
                        section.property: "date"
                        section.criteria: ViewSection.FullString
                        section.delegate: Item {
                            id: dateSection

                            readonly property real stamp: Number(ListView.section)
                            readonly property bool newDay: stamp > 0
                                                           && (ListView.previousSection === ""
                                                               || page.dayStart(Number(ListView.previousSection))
                                                                  !== page.dayStart(stamp))

                            width: messageListView.width
                            height: newDay ? datePill.height + Design.space3 : 0
                            visible: newDay

                            Rectangle {
                                id: datePill
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.top: parent.top
                                anchors.topMargin: Design.space2
                                width: dateLabel.implicitWidth + Design.space4
                                height: dateLabel.implicitHeight + Design.space1 * 2
                                radius: Design.radiusPill
                                color: Qt.alpha(BeyGramTheme.surface2, 0.9)

                                Text {
                                    id: dateLabel
                                    anchors.centerIn: parent
                                    text: page.dayLabel(dateSection.stamp)
                                    color: BeyGramTheme.muted
                                    font.pixelSize: Design.fontCaption
                                    font.weight: Font.DemiBold
                                }
                            }
                        }

                        header: Item {
                            width: messageListView.width
                            height: BeyGramApp.hasMoreHistory || BeyGramApp.historyLoading
                                    ? Design.space10 : Design.space2

                            DsButton {
                                anchors.centerIn: parent
                                width: implicitWidth
                                height: implicitHeight
                                text: "Load older messages"
                                tone: "secondary"
                                size: "sm"
                                visible: BeyGramApp.hasMoreHistory
                                         && !BeyGramApp.historyLoading
                                onClicked: BeyGramApp.loadOlderMessages()
                            }

                            DsSpinner {
                                anchors.centerIn: parent
                                running: BeyGramApp.historyLoading
                                visible: running
                            }
                        }

                        // MessageBubble keeps its own API while the message
                        // bubble migration lands; every binding is unchanged.
                        delegate: MessageBubble {
                            messageId: model.messageId
                            text: model.text
                            textHtml: model.textHtml
                            replyPreview: model.replyPreview
                            reactionsJson: model.reactionsJson
                            linkUrl: model.linkUrl
                            linkTitle: model.linkTitle
                            linkDescription: model.linkDescription
                            linkSite: model.linkSite
                            linkFileId: model.linkFileId
                            linkVideo: model.linkVideo
                            pollQuestion: model.pollQuestion
                            pollOptionsJson: model.pollOptionsJson
                            pollTotal: model.pollTotal
                            pollClosed: model.pollClosed
                            pollQuiz: model.pollQuiz
                            pollMultiple: model.pollMultiple
                            outgoing: model.outgoing
                            senderName: model.senderName
                            date: model.date
                            pending: model.pending
                            failed: model.failed
                            edited: model.edited
                            contentType: model.contentType
                            mediaFileId: model.mediaFileId
                            mediaFullFileId: model.mediaFullFileId
                            mediaWidth: model.mediaWidth
                            mediaHeight: model.mediaHeight
                            mediaReady: model.mediaReady
                            mediaRevision: model.mediaRevision
                            onMediaOpenRequested: (fileId) => BeyGramMediaViewer.open(
                                                      fileId, BeyGramApp.currentChatTitle)
                            onMediaPlayRequested: (fileId) => BeyGramApp.openMediaPlayback(
                                                      fileId, BeyGramApp.currentChatTitle)
                            onEditRequested: (messageId, text) => page.beginEdit(messageId, text)
                            onReplyRequested: (messageId, preview) => BeyGramApp.beginReply(
                                                  messageId, preview)
                            onReactionRequested: (emoji) => BeyGramApp.sendReaction(
                                                     BeyGramApp.currentChatId, messageId, emoji)
                            onForwardRequested: (id) => {
                                page.forwardMessageId = id
                                forwardDialog.messageCount = 1
                                forwardDialog.open()
                            }
                            onPollOptionSelected: (index) => BeyGramApp.votePoll(
                                                       BeyGramApp.currentChatId,
                                                       messageId, index)
                            onDeleteRequested: (messageId) => {
                                deleteDialog.messageId = messageId
                                deleteDialog.open()
                            }
                        }

                        onCountChanged: {
                            if (!BeyGramApp.historyLoading)
                                Qt.callLater(positionViewAtEnd)
                        }
                    }

                    // Floating in-chat search results, above the message list.
                    Rectangle {
                        id: searchPanel
                        anchors.top: parent.top
                        anchors.left: parent.left
                        anchors.right: parent.right
                        height: Math.min(Design.space10 * 8, parent.height * 0.55)
                        visible: page.searchOpen && searchField.text.trim().length > 0
                        z: 20
                        color: BeyGramTheme.surface
                        radius: Design.radiusLg
                        border.width: 1
                        border.color: BeyGramTheme.line

                        ListView {
                            id: searchList
                            anchors.fill: parent
                            anchors.margins: Design.space1
                            clip: true
                            model: BeyGramApp.searchResults
                            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                            delegate: DsListRow {
                                width: searchList.width
                                title: model.text.length > 0 ? model.text : "[media]"
                                subtitle: model.date > 0
                                          ? Qt.formatDateTime(new Date(model.date * 1000),
                                                              "dd.MM.yyyy hh:mm")
                                          : ""
                                onClicked: page.closeSearch()
                            }
                        }

                        Text {
                            anchors.centerIn: parent
                            width: parent.width - Design.space8
                            horizontalAlignment: Text.AlignHCenter
                            visible: searchList.count === 0 && !BeyGramApp.searchLoading
                            text: "No matches"
                            color: BeyGramTheme.muted
                            font.pixelSize: Design.fontSmall
                            wrapMode: Text.WordWrap
                        }
                    }

                    DsSpinner {
                        anchors.centerIn: parent
                        running: BeyGramApp.historyLoading && messageListView.count === 0
                        visible: running
                    }

                    DsEmptyState {
                        anchors.centerIn: parent
                        width: Math.max(0, Math.min(Design.space10 * 7,
                                                     parent.width - Design.space8))
                        visible: messageListView.count === 0 && !BeyGramApp.historyLoading
                        glyph: "\uD83D\uDCAC"
                        title: "No messages loaded"
                        description: "Messages from this chat will appear here."
                    }

                    // Scroll-to-latest affordance, only while the view is not
                    // already pinned to the end.
                    DsIconButton {
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.rightMargin: Design.space4
                        anchors.bottomMargin: Design.space4
                        width: implicitWidth
                        height: implicitHeight
                        icon: "\uE70D" // Segoe MDL2 Assets: ChevronDown
                        tone: "soft"
                        tooltip: "Scroll to latest"
                        visible: messageListView.count > 0 && !messageListView.atYEnd
                        onClicked: messageListView.positionViewAtEnd()
                        z: 10
                    }

                    // Selection is owned by the facade; the toolbar floats so
                    // it never changes the message-list geometry.
                    SelectionToolbar {
                        id: selectionToolbar
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: Design.space4
                        width: implicitWidth
                        height: implicitHeight
                        facade: BeyGramApp.selection
                        z: 30

                        onDeleteRequested: {
                            const ids = BeyGramApp.selection ? BeyGramApp.selection.ids : []
                            for (let i = 0; i < ids.length; ++i) {
                                const id = Number(ids[i])
                                if (id > 0)
                                    BeyGramApp.deleteChatMessage(id, true)
                            }
                            if (BeyGramApp.selection)
                                BeyGramApp.selection.clear()
                        }
                    }
                }

                // Chat-level error strip, dismissible without leaving the chat.
                Rectangle {
                    Layout.fillWidth: true
                    visible: BeyGramApp.chatError.length > 0
                    implicitHeight: errorRow.implicitHeight + Design.space3
                    color: Qt.alpha(BeyGramTheme.danger, 0.10)

                    RowLayout {
                        id: errorRow
                        anchors.fill: parent
                        anchors.leftMargin: Design.space4
                        anchors.rightMargin: Design.space2
                        spacing: Design.space2

                        Text {
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignVCenter
                            text: BeyGramApp.chatError
                            color: BeyGramTheme.danger
                            font.pixelSize: Design.fontCaption
                            wrapMode: Text.WordWrap
                        }

                        DsIconButton {
                            Layout.alignment: Qt.AlignVCenter
                            icon: "\uE711" // Segoe MDL2 Assets: Cancel
                            size: "sm"
                            tone: "danger"
                            tooltip: "Dismiss"
                            onClicked: BeyGramApp.clearChatError()
                        }
                    }
                }

                // ----------------------------------------------------------
                // Composer: reply/edit strips above the attach + poll row.
                // ----------------------------------------------------------
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: composerColumn.implicitHeight + Design.space4
                    color: BeyGramTheme.surface

                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        height: 1
                        color: BeyGramTheme.line
                    }

                    ColumnLayout {
                        id: composerColumn
                        anchors.fill: parent
                        anchors.margins: Design.space2
                        spacing: Design.space2

                        // Edit mode banner.
                        Rectangle {
                            Layout.fillWidth: true
                            visible: page.editingId > 0
                            implicitHeight: editRow.implicitHeight + Design.space2
                            radius: Design.radiusSm
                            color: Qt.alpha(BeyGramTheme.accent, 0.10)
                            clip: true

                            Rectangle {
                                anchors.left: parent.left
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                width: 3
                                color: BeyGramTheme.accent
                            }

                            RowLayout {
                                id: editRow
                                anchors.fill: parent
                                anchors.leftMargin: Design.space3
                                anchors.rightMargin: Design.space1
                                spacing: Design.space2

                                Text {
                                    Layout.fillWidth: true
                                    Layout.alignment: Qt.AlignVCenter
                                    text: "Editing message"
                                    color: BeyGramTheme.muted
                                    font.pixelSize: Design.fontSmall
                                    elide: Text.ElideRight
                                }

                                DsIconButton {
                                    Layout.alignment: Qt.AlignVCenter
                                    icon: "\uE711" // Segoe MDL2 Assets: Cancel
                                    size: "sm"
                                    tooltip: "Cancel editing"
                                    onClicked: page.resetComposer()
                                }
                            }
                        }

                        // Reply banner.
                        Rectangle {
                            Layout.fillWidth: true
                            visible: page.editingId === 0
                                     && BeyGramApp.replyToMessageId > 0
                            implicitHeight: replyRow.implicitHeight + Design.space2
                            radius: Design.radiusSm
                            color: Qt.alpha(BeyGramTheme.accent, 0.10)
                            clip: true

                            Rectangle {
                                anchors.left: parent.left
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                width: 3
                                color: BeyGramTheme.accent
                            }

                            RowLayout {
                                id: replyRow
                                anchors.fill: parent
                                anchors.leftMargin: Design.space3
                                anchors.rightMargin: Design.space1
                                spacing: Design.space2

                                Text {
                                    Layout.fillWidth: true
                                    Layout.alignment: Qt.AlignVCenter
                                    text: "Replying: " + BeyGramApp.replyPreview
                                    color: BeyGramTheme.muted
                                    font.pixelSize: Design.fontSmall
                                    elide: Text.ElideRight
                                }

                                DsIconButton {
                                    Layout.alignment: Qt.AlignVCenter
                                    icon: "\uE711" // Segoe MDL2 Assets: Cancel
                                    size: "sm"
                                    tooltip: "Cancel reply"
                                    onClicked: BeyGramApp.cancelReply()
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Design.space2

                            DsIconButton {
                                Layout.alignment: Qt.AlignBottom
                                icon: "\uE723" // Segoe MDL2 Assets: Attach
                                tooltip: "Attach file"
                                onClicked: attachDialog.open()
                            }

                            DsIconButton {
                                Layout.alignment: Qt.AlignBottom
                                icon: "\uE9D9" // Segoe MDL2 Assets: Poll
                                tooltip: "Create poll"
                                onClicked: pollDialog.open()
                            }

                            TextArea {
                                id: composer
                                Layout.fillWidth: true
                                implicitHeight: Math.min(Design.space10 * 2 + Design.space3,
                                                         Math.max(Design.controlHeight,
                                                                  contentHeight + Design.space4))
                                leftPadding: Design.space3
                                rightPadding: Design.space3
                                topPadding: Design.space2
                                bottomPadding: Design.space2
                                placeholderText: "Write a message"
                                placeholderTextColor: Qt.alpha(BeyGramTheme.muted, 0.75)
                                color: BeyGramTheme.text
                                selectionColor: BeyGramTheme.accent
                                selectedTextColor: BeyGramTheme.onAccent
                                font.pixelSize: Design.fontBody
                                wrapMode: TextArea.Wrap
                                background: Rectangle {
                                    radius: Design.composerRadius
                                    color: BeyGramTheme.surface2
                                    border.width: composer.activeFocus ? 1 : 0
                                    border.color: BeyGramTheme.accent
                                }
                                Keys.onPressed: (event) => {
                                    if (event.key === Qt.Key_Return
                                        || event.key === Qt.Key_Enter) {
                                        if (!(event.modifiers & Qt.ShiftModifier)) {
                                            page.submitComposer()
                                            event.accepted = true
                                        }
                                    }
                                }
                                onTextChanged: {
                                    page.scheduleDraft()
                                    if (text.trim().length > 0)
                                        BeyGramApp.notifyTyping()
                                }
                            }

                            DsButton {
                                Layout.alignment: Qt.AlignBottom
                                text: page.editingId > 0 ? "Save" : "Send"
                                enabled: composer.text.trim().length > 0
                                onClicked: page.submitComposer()
                            }
                        }
                    }
                }
            }

            // --------------------------------------------------------------
            // Profile side panel (not a dialog: it overlays the conversation).
            // --------------------------------------------------------------
            Rectangle {
                id: profilePanel

                anchors.top: parent.top
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                width: page.chatListWidth
                visible: BeyGramApp.profileVisible
                z: 40
                color: BeyGramTheme.surface
                border.width: 1
                border.color: BeyGramTheme.line

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Design.space5
                    spacing: Design.space3

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Design.space2

                        Text {
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignVCenter
                            text: "Profile"
                            color: BeyGramTheme.text
                            font.pixelSize: Design.fontTitle
                            font.weight: Font.DemiBold
                        }

                        DsIconButton {
                            Layout.alignment: Qt.AlignVCenter
                            icon: "\uE711" // Segoe MDL2 Assets: Cancel
                            size: "sm"
                            tooltip: "Close profile"
                            onClicked: BeyGramApp.closeProfile()
                        }
                    }

                    Avatar {
                        Layout.alignment: Qt.AlignHCenter
                        size: Design.avatarXLarge
                        fileId: BeyGramApp.currentChatPhotoFileId
                        ready: BeyGramApp.currentChatPhotoReady
                        revision: BeyGramApp.currentChatPhotoRevision
                        initials: BeyGramApp.currentChatTitle.length > 0
                                  ? BeyGramApp.currentChatTitle.charAt(0).toUpperCase()
                                  : "?"
                        tint: BeyGramTheme.accentSoft
                        ink: BeyGramTheme.accentInk
                    }

                    Text {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        text: BeyGramApp.profileName.length > 0
                              ? BeyGramApp.profileName
                              : BeyGramApp.currentChatTitle
                        color: BeyGramTheme.text
                        font.pixelSize: Design.fontTitle
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                    }

                    Text {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        visible: text.length > 0
                        text: BeyGramApp.profileUsername.length > 0
                              ? "@" + BeyGramApp.profileUsername : ""
                        color: BeyGramTheme.accentInk
                        font.pixelSize: Design.fontSmall
                        elide: Text.ElideRight
                    }

                    Text {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        visible: BeyGramApp.profileIsGroup || BeyGramApp.profileIsChannel
                        text: (BeyGramApp.profileIsChannel ? "Channel" : "Group")
                              + (BeyGramApp.profileMemberCount > 0
                                 ? " · " + BeyGramApp.profileMemberCount + " members" : "")
                        color: BeyGramTheme.muted
                        font.pixelSize: Design.fontSmall
                    }

                    DsDivider {
                        Layout.fillWidth: true
                    }

                    Text {
                        Layout.fillWidth: true
                        visible: BeyGramApp.profileBio.length > 0
                        text: BeyGramApp.profileBio
                        color: BeyGramTheme.text
                        font.pixelSize: Design.fontBody
                        wrapMode: Text.WordWrap
                        lineHeight: Design.lineNormal
                    }

                    Text {
                        Layout.fillWidth: true
                        visible: BeyGramApp.profilePhone.length > 0
                        text: BeyGramApp.profilePhone
                        color: BeyGramTheme.muted
                        font.pixelSize: Design.fontSmall
                    }

                    DsSpinner {
                        Layout.alignment: Qt.AlignHCenter
                        running: BeyGramApp.profileLoading
                        visible: running
                    }

                    Item {
                        Layout.fillHeight: true
                    }
                }
            }

            // Empty conversation state, shown in the wide two-pane layout.
            DsEmptyState {
                anchors.centerIn: parent
                width: Math.max(0, Math.min(Design.space10 * 9,
                                             page.width - Design.space8))
                visible: BeyGramApp.currentChatId === 0
                glyph: "B"
                title: "Select a chat"
                description: "The chat list and message history are loaded from your authorized Telegram session."
            }
        }
    }

    // ------------------------------------------------------------------
    // App state
    // ------------------------------------------------------------------
    Connections {
        target: BeyGramApp

        function onInitialHistoryLoaded() {
            Qt.callLater(messageListView.positionViewAtEnd)
        }

        function onChatStateChanged() {
            if (page.searchOpen && BeyGramApp.currentChatId !== page.loadedChatId)
                page.closeSearch()
            if (BeyGramApp.currentChatId === 0) {
                page.resetComposer()
                page.loadedChatId = 0
            } else if (BeyGramApp.currentChatId !== page.loadedChatId) {
                page.loadDraft()
            }
        }

        function onReplyStateChanged() {
            page.scheduleDraft()
        }
    }

    // ------------------------------------------------------------------
    // Dialogs and menus
    // ------------------------------------------------------------------
    ForwardDialog {
        id: forwardDialog
        model: BeyGramApp.chatList
        messageCount: 1
        onChatSelected: (chatId) => {
            BeyGramApp.forwardMessage(BeyGramApp.currentChatId,
                                      page.forwardMessageId, chatId)
        }
    }

    FileDialog {
        id: attachDialog
        title: "Send a file"
        fileMode: FileDialog.OpenFile
        nameFilters: [
            "Media (*.jpg *.jpeg *.png *.webp *.mp4 *.mkv *.mov *.webm)",
            "Documents (*.pdf *.zip *.txt *.doc *.docx)",
            "All files (*)"
        ]
        onAccepted: {
            const caption = composer.text.trim()
            if (BeyGramApp.sendChatMedia(selectedFile, caption)) {
                composer.text = ""
                BeyGramApp.clearDraft(BeyGramApp.currentChatId)
            }
        }
    }

    PollComposerDialog {
        id: pollDialog
        onCreateRequested: (question, options, anonymous, multiple, quiz,
                            correctIndex, explanation) => {
            BeyGramApp.sendPoll(question, options, anonymous, multiple, quiz,
                                correctIndex, explanation)
        }
    }

    // Delete keeps both scopes: "for me" stays in the content, the destructive
    // "for everyone" action is the dialog's primary button.
    DsDialog {
        id: deleteDialog

        property var messageId: 0

        title: "Delete message?"
        description: "Choose whether to remove this message only for you or for everyone. TDLib enforces the account's delete permission."
        primaryText: "Delete for everyone"
        secondaryText: "Cancel"
        tone: "danger"

        onAccepted: BeyGramApp.deleteChatMessage(deleteDialog.messageId, true)

        DsButton {
            Layout.fillWidth: true
            text: "Delete for me"
            tone: "secondary"
            onClicked: {
                deleteDialog.close()
                BeyGramApp.deleteChatMessage(deleteDialog.messageId, false)
            }
        }
    }

    DsMenu {
        id: chatMenu

        property var chatId: 0
        property bool chatMuted: false

        DsMenuItem {
            text: chatMenu.chatMuted ? "Unmute" : "Mute"
            onTriggered: BeyGramApp.setChatMuted(chatMenu.chatId,
                                                 !chatMenu.chatMuted)
        }

        DsMenuItem {
            text: BeyGramApp.archiveOpen ? "Unarchive" : "Archive"
            onTriggered: BeyGramApp.setChatArchived(chatMenu.chatId,
                                                    !BeyGramApp.archiveOpen)
        }
    }
}
