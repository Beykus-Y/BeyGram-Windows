pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtQml
import BeyGram.Theme 1.0
import BeyGram.App 1.0
import "../ds/design.js" as Design

// ChatsPage — adaptive two-pane conversations workspace.
//
// Wide windows (>= 860 px) show the chat list alongside the open conversation
// (with an optional slide-in chat info drawer on the right). Narrow windows
// collapse smoothly between the chat list and the open conversation with a
// back action in the chat header.
Item {
    id: page

    // --- Public API (preserved for tst_qml_ui and callers) ----------------
    property var editingId: 0
    property var loadedChatId: 0
    property bool searchOpen: false
    property var forwardMessageId: 0
    property bool chatInfoOpen: false

    readonly property int wideBreakpoint: 860
    readonly property bool wideLayout: page.width >= page.wideBreakpoint
    readonly property int chatListWidth: Math.min(380, Math.max(320, Math.round(page.width * 0.30)))

    signal globalSearchRequested()

    readonly property var chatListModel: BeyGramApp.archiveOpen && BeyGramApp.archivedChats
                                         ? BeyGramApp.archivedChats
                                         : BeyGramApp.chatList
    readonly property bool chatListLoading: BeyGramApp.archiveOpen
                                            ? BeyGramApp.archiveLoading
                                            : BeyGramApp.chatLoading

    // Never read chatListModel.count directly (ChatFilterModel proxy has no
    // count property). Keep _chatCount synced from chatListView.count and
    // model signals.
    property int _chatCount: chatListView.count
    property int _unreadCount: 0
    property int _archivedCount: 0
    property bool currentChatMuted: false
    property var _loadedMediaIds: []

    readonly property int chatListCount: page._chatCount
    readonly property int unreadCount: Math.max(
                                           page._unreadCount,
                                           BeyGramApp.chatListControls
                                           ? Number(BeyGramApp.chatListControls.totalUnread || 0)
                                           : 0)

    readonly property string chatListTitle: BeyGramApp.archiveOpen
                                            ? qsTr("Archive")
                                            : qsTr("Chats")
    readonly property string chatListSubtitle: {
        if (BeyGramApp.archiveOpen)
            return qsTr("%n archived chat(s)", "", page.chatListCount)
        if (page.unreadCount > 0)
            return qsTr("%n unread", "", page.unreadCount) + " · "
                   + qsTr("%n chat(s)", "", page.chatListCount)
        return qsTr("%n chat(s)", "", page.chatListCount)
    }

    readonly property int _animFast: Design.motion(Design.motionFast, BeyGramAppearance.reduceMotion)
    readonly property int _animNormal: Design.motion(Design.motionNormal, BeyGramAppearance.reduceMotion)
    readonly property bool _isLiquidGlass: BeyGramAppearance.designStyle === "LiquidGlass"
    readonly property bool _amoled: BeyGramTheme.modeName === "Amoled"

    // --- Public helpers ---------------------------------------------------
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

    function openChatInfo() {
        if (BeyGramApp.currentChatId === 0)
            return
        page.chatInfoOpen = true
        BeyGramApp.requestProfile(BeyGramApp.currentChatId)
    }

    function closeChatInfo() {
        page.chatInfoOpen = false
        BeyGramApp.closeProfile()
    }

    function toggleChatInfo() {
        if (page.chatInfoOpen)
            page.closeChatInfo()
        else
            page.openChatInfo()
    }

    function insertComposerText(value) {
        if (!value || value.length === 0)
            return
        const pos = composer.cursorPosition >= 0 ? composer.cursorPosition : composer.length
        composer.insert(pos, value)
        composer.forceActiveFocus()
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

    // --- Reactive model trackers ------------------------------------------
    component ChatTrackerEntry: QtObject {
        required property var chatId
        required property var unreadCount
        required property bool muted
        onUnreadCountChanged: Qt.callLater(page.syncChatStats)
        onMutedChanged: Qt.callLater(page.syncChatStats)
    }

    component MediaTrackerEntry: QtObject {
        required property var messageId
        required property int mediaFileId
        required property string contentType
    }

    function syncChatStats() {
        page._chatCount = chatListView.count
        var unread = 0
        var muted = false
        for (var i = 0; i < chatTracker.count; ++i) {
            var obj = chatTracker.objectAt(i) as ChatTrackerEntry
            if (!obj)
                continue
            var u = Number(obj.unreadCount)
            if (u > 0)
                unread += u
            if (Number(obj.chatId) === Number(BeyGramApp.currentChatId))
                muted = Boolean(obj.muted)
        }
        page._unreadCount = unread
        page.currentChatMuted = muted
    }

    function syncArchiveCount() {
        page._archivedCount = BeyGramApp.archivedChats && BeyGramApp.archivedChats.count !== undefined
                              ? Number(BeyGramApp.archivedChats.count)
                              : 0
    }

    function syncMediaIds() {
        var ids = []
        for (var i = 0; i < mediaTracker.count; ++i) {
            var obj = mediaTracker.objectAt(i) as MediaTrackerEntry
            if (!obj)
                continue
            var mid = Number(obj.messageId)
            var fileId = Number(obj.mediaFileId)
            if (mid > 0 && fileId > 0 && obj.contentType !== "sticker")
                ids.push(String(mid))
        }
        page._loadedMediaIds = ids
    }

    function dayStart(timestamp) {
        const value = Number(timestamp)
        if (!value || value <= 0)
            return -1
        const date = new Date(value * 1000)
        return new Date(date.getFullYear(), date.getMonth(), date.getDate()).getTime()
    }

    function dayLabel(timestamp) {
        var _rev = BeyGramLang.revision
        const value = Number(timestamp)
        if (!value || value <= 0)
            return ""
        return BeyGramLang.formatDayDivider(value)
    }

    function isMessageSelected(messageId) {
        if (!BeyGramApp.selection || !BeyGramApp.selection.active)
            return false
        return BeyGramApp.selection.ids.indexOf(String(messageId)) >= 0
    }

    Instantiator {
        id: chatTracker
        model: BeyGramApp.chatList
        delegate: ChatTrackerEntry {}
        onObjectAdded: Qt.callLater(page.syncChatStats)
        onObjectRemoved: Qt.callLater(page.syncChatStats)
    }

    Instantiator {
        id: mediaTracker
        model: BeyGramApp.messages
        delegate: MediaTrackerEntry {}
        onObjectAdded: Qt.callLater(page.syncMediaIds)
        onObjectRemoved: Qt.callLater(page.syncMediaIds)
    }

    Connections {
        target: BeyGramApp.chatList
        ignoreUnknownSignals: true
        function onRowsInserted() { Qt.callLater(page.syncChatStats) }
        function onRowsRemoved() { Qt.callLater(page.syncChatStats) }
        function onModelReset() { Qt.callLater(page.syncChatStats) }
        function onLayoutChanged() { Qt.callLater(page.syncChatStats) }
        function onDataChanged() { Qt.callLater(page.syncChatStats) }
    }

    Connections {
        target: BeyGramApp.archivedChats
        ignoreUnknownSignals: true
        function onCountChanged() { Qt.callLater(page.syncArchiveCount) }
        function onRowsInserted() { Qt.callLater(page.syncArchiveCount) }
        function onRowsRemoved() { Qt.callLater(page.syncArchiveCount) }
        function onModelReset() { Qt.callLater(page.syncArchiveCount) }
    }

    Component.onCompleted: {
        Qt.callLater(page.syncChatStats)
        Qt.callLater(page.syncArchiveCount)
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
        // Left Chat List Pane
        // ==================================================================
        Rectangle {
            id: chatListPane

            Layout.fillWidth: !page.wideLayout
            Layout.fillHeight: true
            Layout.preferredWidth: page.wideLayout ? page.chatListWidth : page.width
            Layout.maximumWidth: page.wideLayout ? page.chatListWidth
                                                 : Number.POSITIVE_INFINITY
            color: BeyGramTheme.surface
            visible: page.wideLayout || BeyGramApp.currentChatId === 0

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                // Compact header
                DsHeader {
                    Layout.fillWidth: true
                    bordered: false
                    title: page.chatListTitle
                    subtitle: page.chatListSubtitle

                    leading: DsIconButton {
                        width: implicitWidth
                        height: implicitHeight
                        icon: "\uE72B" // Back
                        size: "sm"
                        tooltip: qsTr("Back to chats")
                        visible: BeyGramApp.archiveOpen
                        onClicked: BeyGramApp.closeArchive()
                    }

                    trailing: RowLayout {
                        spacing: Design.space1

                        DsIconButton {
                            Layout.alignment: Qt.AlignVCenter
                            icon: "\uE7B8" // Archive
                            size: "sm"
                            tooltip: qsTr("Archived chats (%1)").arg(page._archivedCount)
                            visible: !BeyGramApp.archiveOpen && page._archivedCount > 0
                            badge: page._archivedCount
                            onClicked: BeyGramApp.openArchive()
                        }

                        DsIconButton {
                            Layout.alignment: Qt.AlignVCenter
                            icon: "\uE70F" // Compose / Find chat
                            size: "sm"
                            tooltip: qsTr("Find a chat")
                            onClicked: page.focusChatSearch()
                        }
                    }
                }

                // Search field
                DsSearchField {
                    id: chatSearchField
                    Layout.fillWidth: true
                    Layout.leftMargin: Design.space3
                    Layout.rightMargin: Design.space3
                    Layout.topMargin: Design.space1
                    Layout.bottomMargin: Design.space2
                    placeholderText: qsTr("Search chats...")
                    text: BeyGramApp.chatFilter
                    onTextEdited: BeyGramApp.chatFilter = text
                    onCleared: {
                        BeyGramApp.chatFilter = ""
                        text = Qt.binding(function() { return BeyGramApp.chatFilter })
                    }
                }

                // Inline archive strip when archived chats exist
                DsCard {
                    Layout.fillWidth: true
                    Layout.leftMargin: Design.space3
                    Layout.rightMargin: Design.space3
                    Layout.bottomMargin: Design.space2
                    padding: Design.space2 + 2
                    interactive: true
                    visible: !BeyGramApp.archiveOpen && page._archivedCount > 0
                    onClicked: BeyGramApp.openArchive()

                    content: RowLayout {
                        Layout.fillWidth: true
                        spacing: Design.space2 + 2

                        Rectangle {
                            Layout.alignment: Qt.AlignVCenter
                            Layout.preferredWidth: 30
                            Layout.preferredHeight: 30
                            radius: 15
                            color: BeyGramTheme.accentSoft

                            DsIcon {
                                anchors.centerIn: parent
                                name: "\uE7B8"
                                size: 15
                                color: BeyGramTheme.accentInk
                                filled: true
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignVCenter
                            text: qsTr("Archived chats")
                            color: BeyGramTheme.text
                            font.pixelSize: Design.fontSmall + 1
                            font.weight: Font.Medium
                            elide: Text.ElideRight
                        }

                        DsBadge {
                            Layout.alignment: Qt.AlignVCenter
                            tone: "muted"
                            text: String(page._archivedCount)
                        }

                        DsIcon {
                            Layout.alignment: Qt.AlignVCenter
                            name: "\uE76C"
                            size: 12
                            color: BeyGramTheme.muted
                        }
                    }
                }

                // Folder tabs
                ChatFolderTabs {
                    Layout.fillWidth: true
                    Layout.leftMargin: Design.space3
                    Layout.rightMargin: Design.space3
                    Layout.bottomMargin: Design.space2
                    visible: !BeyGramApp.archiveOpen && Boolean(BeyGramApp.chatFolders)
                             && (BeyGramApp.chatFolders.count === undefined
                                 || BeyGramApp.chatFolders.count > 1)
                    model: BeyGramApp.chatFolders
                    currentFolderId: BeyGramApp.currentFolderId
                    onFolderSelected: (folderId) => BeyGramApp.selectFolder(folderId)
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: Qt.alpha(BeyGramTheme.line, 0.7)
                }

                // Chat list view
                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    ListView {
                        id: chatListView
                        anchors.fill: parent
                        anchors.topMargin: Design.space1
                        anchors.leftMargin: Design.space1 + 2
                        anchors.rightMargin: Design.space1 + 2
                        clip: true
                        spacing: 2
                        model: page.chatListModel
                        boundsBehavior: Flickable.StopAtBounds
                        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                        onCountChanged: page._chatCount = chatListView.count

                        delegate: Item {
                            id: chatDelegate

                            required property var chatId
                            required property string title
                            required property string preview
                            required property int unreadCount
                            required property bool pinned
                            required property bool muted
                            required property var lastDate
                            required property string initials
                            required property int accentIndex
                            required property int photoFileId
                            required property bool photoReady
                            required property int photoRevision

                            width: chatListView.width
                            height: chatRowItem.implicitHeight

                            ChatRow {
                                id: chatRowItem
                                width: parent.width
                                chatId: chatDelegate.chatId
                                title: chatDelegate.title
                                preview: chatDelegate.preview
                                unreadCount: chatDelegate.unreadCount
                                pinned: chatDelegate.pinned
                                muted: chatDelegate.muted
                                lastDate: chatDelegate.lastDate
                                initials: chatDelegate.initials
                                accentIndex: chatDelegate.accentIndex
                                photoFileId: chatDelegate.photoFileId
                                photoReady: chatDelegate.photoReady
                                photoRevision: chatDelegate.photoRevision
                                selected: Number(BeyGramApp.currentChatId) === Number(chatDelegate.chatId)
                                onClicked: {
                                    page.resetComposer()
                                    page.closeChatInfo()
                                    BeyGramApp.openChat(chatDelegate.chatId)
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                acceptedButtons: Qt.RightButton
                                onClicked: {
                                    chatMenu.chatId = chatDelegate.chatId
                                    chatMenu.chatMuted = chatDelegate.muted
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
                        glyph: BeyGramApp.chatError.length > 0 ? "\u26A0"
                             : (BeyGramApp.chatFilter.trim().length > 0 ? "\uD83D\uDD0D"
                             : (BeyGramApp.archiveOpen ? "\uD83D\uDDC4" : "\uD83D\uDCAC"))
                        title: {
                            if (BeyGramApp.chatError.length > 0)
                                return qsTr("Chats unavailable")
                            if (BeyGramApp.chatFilter.trim().length > 0)
                                return qsTr("No matching chats")
                            return BeyGramApp.archiveOpen
                                   ? qsTr("No archived chats")
                                   : qsTr("No chats yet")
                        }
                        description: {
                            if (BeyGramApp.chatError.length > 0)
                                return BeyGramApp.chatError
                            if (BeyGramApp.chatFilter.trim().length > 0)
                                return qsTr("No chats match \"%1\".").arg(BeyGramApp.chatFilter.trim())
                            return BeyGramApp.archiveOpen
                                   ? qsTr("Chats you archive will appear here.")
                                   : qsTr("Your Telegram chats will appear here.")
                        }
                        actionText: {
                            if (BeyGramApp.chatError.length > 0)
                                return qsTr("Retry")
                            if (BeyGramApp.chatFilter.trim().length > 0)
                                return qsTr("Reset search")
                            return ""
                        }
                        onActionRequested: {
                            if (BeyGramApp.chatError.length > 0)
                                BeyGramApp.refresh()
                            else if (BeyGramApp.chatFilter.trim().length > 0)
                                BeyGramApp.chatFilter = ""
                        }
                    }
                }
            }
        }

        // Hairline between panes on wide windows
        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: 1
            visible: page.wideLayout
            color: BeyGramTheme.line
        }

        // ==================================================================
        // Right Conversation Pane
        // ==================================================================
        Item {
            id: conversationPane

            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: page.wideLayout || BeyGramApp.currentChatId !== 0

            // Wallpaper layer bound to Appearance settings
            ChatWallpaper {
                anchors.fill: parent
                presetId: BeyGramAppearance.wallpaperPreset
                intensity: BeyGramAppearance.wallpaperIntensity
            }

            // Subtle readability wash over wallpaper when active
            Rectangle {
                anchors.fill: parent
                visible: BeyGramAppearance.wallpaperPreset !== "none" && !page._amoled
                color: Qt.alpha(BeyGramTheme.bg, BeyGramTheme.darkMode ? 0.14 : 0.10)
            }

            // --------------------------------------------------------------
            // Active conversation content + optional slide-in Info drawer
            // --------------------------------------------------------------
            RowLayout {
                anchors.fill: parent
                spacing: 0
                visible: BeyGramApp.currentChatId !== 0

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 0

                    // Open chat header (translucent over wallpaper)
                    DsHeader {
                        Layout.fillWidth: true
                        translucent: true
                        title: BeyGramApp.currentChatTitle
                        subtitle: {
                            if (BeyGramApp.typingText.length > 0)
                                return BeyGramApp.typingText
                            if (BeyGramApp.currentChatSubtitle.length > 0)
                                return BeyGramApp.currentChatSubtitle
                            if (BeyGramApp.currentChatTitle === "Saved Messages")
                                return qsTr("Saved Messages")
                            return ""
                        }

                        leading: RowLayout {
                            spacing: Design.space2

                            DsIconButton {
                                Layout.alignment: Qt.AlignVCenter
                                icon: "\uE72B" // Back
                                size: "sm"
                                tooltip: qsTr("Back to chats")
                                visible: !page.wideLayout
                                onClicked: {
                                    page.closeChatInfo()
                                    BeyGramApp.closeChat()
                                }
                            }

                            Item {
                                Layout.alignment: Qt.AlignVCenter
                                Layout.preferredWidth: Design.avatarSmall + 6
                                Layout.preferredHeight: Design.avatarSmall + 6

                                Avatar {
                                    anchors.centerIn: parent
                                    size: Design.avatarSmall + 4
                                    fileId: BeyGramApp.currentChatPhotoFileId
                                    ready: BeyGramApp.currentChatPhotoReady
                                    revision: BeyGramApp.currentChatPhotoRevision
                                    initials: BeyGramApp.currentChatTitle.length > 0
                                              ? BeyGramApp.currentChatTitle.charAt(0).toUpperCase()
                                              : "?"
                                    tint: BeyGramTheme.accentSoft
                                    ink: BeyGramTheme.accentInk
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: page.toggleChatInfo()
                                }
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

                            TypingIndicator {
                                Layout.alignment: Qt.AlignVCenter
                                visible: BeyGramApp.typingText.length > 0
                                active: BeyGramApp.typingText.length > 0
                                text: ""
                            }

                            DsIconButton {
                                Layout.alignment: Qt.AlignVCenter
                                icon: "\uE721" // Search
                                tooltip: page.searchOpen
                                         ? qsTr("Close search")
                                         : qsTr("Search in chat")
                                selected: page.searchOpen
                                onClicked: page.searchOpen ? page.closeSearch()
                                                           : page.openSearch()
                            }

                            DsIconButton {
                                Layout.alignment: Qt.AlignVCenter
                                icon: page.currentChatMuted ? "\uE74F" : "\uEA8F"
                                tooltip: page.currentChatMuted
                                         ? qsTr("Unmute notifications")
                                         : qsTr("Mute notifications")
                                selected: page.currentChatMuted
                                onClicked: BeyGramApp.setChatMuted(
                                               BeyGramApp.currentChatId,
                                               !page.currentChatMuted)
                            }

                            // Call/video buttons remain hidden until AppController
                            // exposes real TDLib call initiation.

                            DsIconButton {
                                Layout.alignment: Qt.AlignVCenter
                                icon: "\uE946" // Info
                                tooltip: page.chatInfoOpen
                                         ? qsTr("Close chat info")
                                         : qsTr("Chat info")
                                selected: page.chatInfoOpen
                                onClicked: page.toggleChatInfo()
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
                        title: qsTr("Pinned message")
                        preview: BeyGramApp.pinnedPreview
                        onActivated: page.openSearch()
                        onUnpinRequested: BeyGramApp.unpinPinnedMessage()
                    }

                    PlaybackBar {
                        Layout.fillWidth: true
                        facade: BeyGramApp.playbackBar
                    }

                    // Expandable inline in-chat search bar
                    Rectangle {
                        Layout.fillWidth: true
                        visible: Layout.preferredHeight > 0
                        Layout.preferredHeight: page.searchOpen ? (searchBar.implicitHeight + Design.space2 * 2) : 0
                        color: page._isLiquidGlass
                               ? Qt.alpha(BeyGramTheme.surface, 0.90)
                               : BeyGramTheme.surface
                        clip: true

                        Behavior on Layout.preferredHeight {
                            NumberAnimation { duration: page._animFast; easing.type: Easing.OutCubic }
                        }

                        Rectangle {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            height: 1
                            color: BeyGramTheme.line
                        }

                        RowLayout {
                            id: searchBar
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.leftMargin: Design.space3
                            anchors.rightMargin: Design.space3
                            spacing: Design.space2

                            DsSearchField {
                                id: searchField
                                Layout.fillWidth: true
                                Layout.alignment: Qt.AlignVCenter
                                placeholderText: qsTr("Search in this chat...")
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

                            DsIconButton {
                                Layout.alignment: Qt.AlignVCenter
                                icon: "\uE711"
                                size: "sm"
                                tooltip: qsTr("Close search")
                                onClicked: page.closeSearch()
                            }
                        }
                    }

                    // Message list area
                    Item {
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        ListView {
                            id: messageListView
                            anchors.fill: parent
                            anchors.topMargin: Design.space1
                            anchors.bottomMargin: Design.space2
                            clip: true
                            spacing: 0
                            model: BeyGramApp.messages
                            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                            section.property: "day"
                            section.criteria: ViewSection.FullString
                            section.delegate: Item {
                                id: dateSection

                                required property string section

                                // The model's "day" role is a local yyyy-MM-dd key,
                                // so one section is exactly one calendar day.
                                readonly property real stamp: {
                                    const parts = dateSection.section.split("-")
                                    if (parts.length !== 3)
                                        return 0
                                    const day = new Date(Number(parts[0]),
                                                         Number(parts[1]) - 1,
                                                         Number(parts[2]))
                                    return day.getTime() / 1000
                                }
                                readonly property bool newDay: stamp > 0

                                width: messageListView.width
                                height: newDay ? datePill.height + Design.space4 : 0
                                visible: newDay

                                Rectangle {
                                    id: datePill
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: dateLabel.implicitWidth + Design.space4
                                    height: dateLabel.implicitHeight + Design.space1 * 2
                                    radius: Design.radiusPill
                                    color: Qt.alpha(BeyGramTheme.surface,
                                                    BeyGramTheme.darkMode ? 0.88 : 0.92)
                                    border.width: 1
                                    border.color: Qt.alpha(BeyGramTheme.line, 0.8)

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
                                    text: qsTr("Load older messages")
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

                            delegate: MessageBubble {
                                id: msgBubble

                                required property var messageId
                                required property string text
                                required property string textHtml
                                required property string replyPreview
                                required property string reactionsJson
                                required property string linkUrl
                                required property string linkTitle
                                required property string linkDescription
                                required property string linkSite
                                required property int linkFileId
                                required property bool linkVideo
                                required property string pollQuestion
                                required property string pollOptionsJson
                                required property int pollTotal
                                required property bool pollClosed
                                required property bool pollQuiz
                                required property bool pollMultiple
                                required property bool outgoing
                                required property string senderName
                                required property var date
                                required property bool pending
                                required property bool failed
                                required property bool edited
                                required property string contentType
                                required property int mediaFileId
                                required property int mediaFullFileId
                                required property int mediaWidth
                                required property int mediaHeight
                                required property bool mediaReady
                                required property int mediaRevision

                                selectable: BeyGramApp.selection ? BeyGramApp.selection.active : false
                                selected: page.isMessageSelected(msgBubble.messageId)

                                onMediaOpenRequested: (fileId) => BeyGramMediaViewer.open(
                                                          fileId, BeyGramApp.currentChatTitle)
                                onMediaPlayRequested: (fileId) => BeyGramApp.openMediaPlayback(
                                                          fileId, BeyGramApp.currentChatTitle)
                                onEditRequested: (id, msgText) => page.beginEdit(id, msgText)
                                onReplyRequested: (id, preview) => BeyGramApp.beginReply(
                                                      id, preview)
                                onReactionRequested: (emoji) => BeyGramApp.sendReaction(
                                                         BeyGramApp.currentChatId, msgBubble.messageId, emoji)
                                onAddReactionRequested: (id, anchorItem) => {
                                    if (!BeyGramApp.reactions)
                                        return
                                    if (BeyGramApp.reactions.top.length === 0) {
                                        BeyGramApp.reactions.setCatalog(
                                            ["\uD83D\uDC4D", "\u2764\uFE0F", "\uD83D\uDD25", "\uD83D\uDE02", "\uD83D\uDE2E", "\uD83D\uDE22"],
                                            ["\uD83D\uDE4F", "\uD83C\uDF89", "\uD83D\uDC4F", "\uD83E\uDD14", "\uD83E\uDD2F", "\uD83D\uDCAF"],
                                            3)
                                    }
                                    BeyGramApp.reactions.openFor(
                                        String(BeyGramApp.currentChatId),
                                        String(id))
                                    const pt = anchorItem.mapToItem(conversationPane, 0, 0)
                                    reactionPicker.x = Math.max(
                                        Design.space3,
                                        Math.min(conversationPane.width - reactionPicker.implicitWidth - Design.space3,
                                                 pt.x))
                                    reactionPicker.y = Math.max(
                                        Design.space3,
                                        pt.y - reactionPicker.implicitHeight - Design.space1)
                                    reactionPicker.open()
                                }
                                onForwardRequested: (id) => {
                                    page.forwardMessageId = id
                                    forwardDialog.messageCount = 1
                                    forwardDialog.open()
                                }
                                onPollOptionSelected: (index) => BeyGramApp.votePoll(
                                                           BeyGramApp.currentChatId,
                                                           msgBubble.messageId, index)
                                onDeleteRequested: (id) => {
                                    deleteDialog.messageId = id
                                    deleteDialog.open()
                                }
                                onSelectionToggled: (id, isSelected) => {
                                    if (BeyGramApp.selection)
                                        BeyGramApp.selection.toggle(String(id))
                                }
                            }

                            onCountChanged: {
                                if (!BeyGramApp.historyLoading)
                                    Qt.callLater(positionViewAtEnd)
                            }
                        }

                        // Floating in-chat search results
                        Rectangle {
                            id: searchPanel
                            anchors.top: parent.top
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.margins: Design.space3
                            height: Math.min(Design.space10 * 8, parent.height * 0.55)
                            visible: page.searchOpen && searchField.text.trim().length > 0
                            z: 20
                            radius: Design.cardRadius(BeyGramAppearance.designStyle)
                            color: BeyGramTheme.surface
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
                                    required property string text
                                    required property var date

                                    width: searchList.width
                                    title: text.length > 0 ? text : qsTr("[Media]")
                                    subtitle: date > 0 ? BeyGramLang.formatDateTime(date) : ""
                                    onClicked: page.closeSearch()
                                }
                            }

                            Text {
                                anchors.centerIn: parent
                                width: parent.width - Design.space8
                                horizontalAlignment: Text.AlignHCenter
                                visible: searchList.count === 0 && !BeyGramApp.searchLoading
                                text: qsTr("No matching messages")
                                color: BeyGramTheme.muted
                                font.pixelSize: Design.fontSmall
                                wrapMode: Text.WordWrap
                            }
                        }

                        // Smooth loading overlay without harsh jumps
                        Rectangle {
                            anchors.centerIn: parent
                            width: 56
                            height: 56
                            radius: 28
                            color: Qt.alpha(BeyGramTheme.surface, 0.92)
                            border.width: 1
                            border.color: BeyGramTheme.line
                            visible: (BeyGramApp.chatLoading || BeyGramApp.historyLoading)
                                     && messageListView.count === 0
                            z: 12

                            DsSpinner {
                                anchors.centerIn: parent
                                running: parent.visible
                            }
                        }

                        // Friendly empty-chat card when 0 messages exist
                        DsCard {
                            anchors.centerIn: parent
                            width: Math.min(340, Math.max(220, parent.width - Design.space8))
                            padding: Design.space5
                            visible: messageListView.count === 0
                                     && !BeyGramApp.historyLoading
                                     && !BeyGramApp.chatLoading
                                     && BeyGramApp.chatError.length === 0

                            content: DsEmptyState {
                                Layout.fillWidth: true
                                glyph: "\uD83D\uDCAC"
                                title: qsTr("No messages yet")
                                description: qsTr("Say hello or attach a file to start this conversation.")
                            }
                        }

                        // Empty chat error state with Retry
                        DsCard {
                            anchors.centerIn: parent
                            width: Math.min(360, Math.max(220, parent.width - Design.space8))
                            padding: Design.space5
                            visible: messageListView.count === 0
                                     && !BeyGramApp.historyLoading
                                     && !BeyGramApp.chatLoading
                                     && BeyGramApp.chatError.length > 0

                            content: DsEmptyState {
                                Layout.fillWidth: true
                                glyph: "\u26A0"
                                title: qsTr("Unable to load messages")
                                description: BeyGramApp.chatError
                                actionText: qsTr("Retry")
                                onActionRequested: {
                                    BeyGramApp.clearChatError()
                                    BeyGramApp.openChat(BeyGramApp.currentChatId)
                                }
                            }
                        }

                        // Scroll-to-latest pill button
                        DsIconButton {
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            anchors.rightMargin: Design.space4
                            anchors.bottomMargin: Design.space3
                            width: implicitWidth
                            height: implicitHeight
                            icon: "\uE70D"
                            tone: "soft"
                            tooltip: qsTr("Scroll to latest")
                            visible: messageListView.count > 0 && !messageListView.atYEnd
                            onClicked: messageListView.positionViewAtEnd()
                            z: 10
                        }

                        // Floating selection toolbar
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

                    // Actionable inline chat error strip (when messages already exist)
                    Rectangle {
                        Layout.fillWidth: true
                        visible: BeyGramApp.chatError.length > 0 && messageListView.count > 0
                        implicitHeight: errorRow.implicitHeight + Design.space2 * 2
                        color: Qt.alpha(BeyGramTheme.danger, 0.12)

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

                            DsButton {
                                Layout.alignment: Qt.AlignVCenter
                                text: qsTr("Retry")
                                tone: "secondary"
                                size: "sm"
                                onClicked: {
                                    BeyGramApp.clearChatError()
                                    BeyGramApp.openChat(BeyGramApp.currentChatId)
                                }
                            }

                            DsIconButton {
                                Layout.alignment: Qt.AlignVCenter
                                icon: "\uE711"
                                size: "sm"
                                tone: "danger"
                                tooltip: qsTr("Dismiss")
                                onClicked: BeyGramApp.clearChatError()
                            }
                        }
                    }

                    // Voice recorder strip (replaces composer while recording)
                    VoiceRecorder {
                        Layout.fillWidth: true
                        facade: BeyGramApp.voiceComposer
                    }

                    // ------------------------------------------------------
                    // Modern Desktop Pill Composer (Android ChatGlassChrome)
                    // ------------------------------------------------------
                    Item {
                        id: composerHost
                        Layout.fillWidth: true
                        visible: !(BeyGramApp.voiceComposer && BeyGramApp.voiceComposer.recording)
                        implicitHeight: composerColumn.implicitHeight + Design.space3 * 2

                        ColumnLayout {
                            id: composerColumn
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            anchors.leftMargin: Design.space4
                            anchors.rightMargin: Design.space4
                            anchors.bottomMargin: Design.space3
                            spacing: Design.space2

                            // Edit mode banner
                            Rectangle {
                                Layout.fillWidth: true
                                visible: page.editingId > 0
                                implicitHeight: editRow.implicitHeight + Design.space2
                                radius: Design.radiusMd
                                color: Qt.alpha(BeyGramTheme.surface, 0.94)
                                border.width: 1
                                border.color: BeyGramTheme.line
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
                                        text: qsTr("Editing message")
                                        color: BeyGramTheme.accentInk
                                        font.pixelSize: Design.fontSmall
                                        font.weight: Font.Medium
                                        elide: Text.ElideRight
                                    }

                                    DsIconButton {
                                        Layout.alignment: Qt.AlignVCenter
                                        icon: "\uE711"
                                        size: "sm"
                                        tooltip: qsTr("Cancel editing")
                                        onClicked: page.resetComposer()
                                    }
                                }
                            }

                            // Reply banner
                            Rectangle {
                                Layout.fillWidth: true
                                visible: page.editingId === 0
                                         && BeyGramApp.replyToMessageId > 0
                                implicitHeight: replyRow.implicitHeight + Design.space2
                                radius: Design.radiusMd
                                color: Qt.alpha(BeyGramTheme.surface, 0.94)
                                border.width: 1
                                border.color: BeyGramTheme.line
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
                                        text: qsTr("Replying to: %1").arg(BeyGramApp.replyPreview)
                                        color: BeyGramTheme.muted
                                        font.pixelSize: Design.fontSmall
                                        elide: Text.ElideRight
                                    }

                                    DsIconButton {
                                        Layout.alignment: Qt.AlignVCenter
                                        icon: "\uE711"
                                        size: "sm"
                                        tooltip: qsTr("Cancel reply")
                                        onClicked: BeyGramApp.cancelReply()
                                    }
                                }
                            }

                            // Pill composer container
                            Rectangle {
                                id: composerPill
                                Layout.fillWidth: true
                                implicitHeight: pillRow.implicitHeight + Design.space1 * 2
                                radius: Design.composerRadius
                                color: page._isLiquidGlass
                                       ? Qt.alpha(BeyGramTheme.surface, BeyGramTheme.darkMode ? 0.86 : 0.92)
                                       : BeyGramTheme.surface
                                border.width: composer.activeFocus ? 1.5 : 1
                                border.color: composer.activeFocus
                                              ? BeyGramTheme.accent
                                              : (page._amoled ? Qt.lighter(BeyGramTheme.line, 1.35)
                                                              : BeyGramTheme.line)
                                antialiasing: true

                                Behavior on border.color {
                                    ColorAnimation { duration: page._animFast }
                                }

                                RowLayout {
                                    id: pillRow
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.verticalCenter: parent.verticalCenter
                                    anchors.leftMargin: Design.space2
                                    anchors.rightMargin: Design.space2
                                    spacing: Design.space1

                                    DsIconButton {
                                        id: attachBtn
                                        Layout.alignment: Qt.AlignBottom
                                        Layout.bottomMargin: 2
                                        icon: "\uE723" // Attach
                                        size: "md"
                                        tooltip: qsTr("Attach media or poll")
                                        onClicked: attachMenu.open()

                                        DsMenu {
                                            id: attachMenu
                                            y: -height - Design.space1

                                            DsMenuItem {
                                                text: qsTr("Photo or file...")
                                                glyph: "\uE723"
                                                onTriggered: attachDialog.open()
                                            }
                                            DsMenuItem {
                                                text: qsTr("Create poll...")
                                                glyph: "\uE9D9"
                                                onTriggered: pollDialog.open()
                                            }
                                        }
                                    }

                                    TextArea {
                                        id: composer
                                        Layout.fillWidth: true
                                        implicitHeight: Math.min(Design.space10 * 3,
                                                                 Math.max(Design.controlHeight,
                                                                          contentHeight + Design.space4))
                                        leftPadding: Design.space1
                                        rightPadding: Design.space1
                                        topPadding: Design.space2 + 2
                                        bottomPadding: Design.space2 + 2
                                        placeholderText: qsTr("Write a message...")
                                        placeholderTextColor: Qt.alpha(BeyGramTheme.muted, 0.75)
                                        color: BeyGramTheme.text
                                        selectionColor: BeyGramTheme.accent
                                        selectedTextColor: BeyGramTheme.onAccent
                                        font.pixelSize: BeyGramAppearance.messageTextSize
                                        wrapMode: TextArea.Wrap
                                        background: Item {}

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

                                    DsIconButton {
                                        id: emojiBtn
                                        Layout.alignment: Qt.AlignBottom
                                        Layout.bottomMargin: 2
                                        icon: "\uE76E" // Emoji
                                        size: "md"
                                        tooltip: qsTr("Emoji")
                                        selected: emojiPicker.visible
                                        onClicked: {
                                            if (emojiPicker.visible) {
                                                emojiPicker.close()
                                                return
                                            }
                                            const pt = emojiBtn.mapToItem(conversationPane, 0, 0)
                                            emojiPicker.x = Math.max(
                                                Design.space3,
                                                Math.min(conversationPane.width - emojiPicker.width - Design.space3,
                                                         pt.x - emojiPicker.width + emojiBtn.width))
                                            emojiPicker.y = Math.max(
                                                Design.space3,
                                                pt.y - emojiPicker.height - Design.space2)
                                            emojiPicker.open()
                                        }
                                    }

                                    DsIconButton {
                                        id: stickerBtn
                                        Layout.alignment: Qt.AlignBottom
                                        Layout.bottomMargin: 2
                                        icon: "\uF4AA" // Sticker / Note
                                        size: "md"
                                        tooltip: qsTr("Stickers")
                                        selected: stickerPicker.visible
                                        onClicked: {
                                            if (stickerPicker.visible) {
                                                stickerPicker.close()
                                                return
                                            }
                                            const pt = stickerBtn.mapToItem(conversationPane, 0, 0)
                                            stickerPicker.x = Math.max(
                                                Design.space3,
                                                Math.min(conversationPane.width - stickerPicker.width - Design.space3,
                                                         pt.x - stickerPicker.width + stickerBtn.width))
                                            stickerPicker.y = Math.max(
                                                Design.space3,
                                                pt.y - stickerPicker.height - Design.space2)
                                            stickerPicker.open()
                                        }
                                    }

                                    // Voice button when empty, prominent accent Send button when text exists
                                    DsIconButton {
                                        Layout.alignment: Qt.AlignBottom
                                        Layout.bottomMargin: 2
                                        visible: composer.text.trim().length === 0 && page.editingId === 0
                                        icon: "\uE720" // Microphone
                                        size: "md"
                                        tooltip: qsTr("Record voice message")
                                        onClicked: {
                                            if (BeyGramApp.voiceComposer)
                                                BeyGramApp.voiceComposer.start()
                                        }
                                    }

                                    Rectangle {
                                        id: sendPill
                                        Layout.alignment: Qt.AlignBottom
                                        Layout.bottomMargin: 2
                                        visible: composer.text.trim().length > 0 || page.editingId > 0
                                        implicitWidth: 36
                                        implicitHeight: 36
                                        radius: 18
                                        color: composer.text.trim().length > 0
                                               ? (sendMouse.pressed
                                                  ? Qt.darker(BeyGramTheme.accent, 1.12)
                                                  : (sendMouse.containsMouse
                                                     ? Qt.lighter(BeyGramTheme.accent, 1.08)
                                                     : BeyGramTheme.accent))
                                               : BeyGramTheme.surface2
                                        opacity: composer.text.trim().length > 0 ? 1.0 : Design.opacityDisabled
                                        antialiasing: true

                                        Accessible.role: Accessible.Button
                                        Accessible.name: page.editingId > 0
                                                         ? qsTr("Save message")
                                                         : qsTr("Send message")

                                        DsIcon {
                                            anchors.centerIn: parent
                                            name: page.editingId > 0 ? "\uE73E" : "\uE724"
                                            size: 16
                                            color: composer.text.trim().length > 0
                                                   ? BeyGramTheme.onAccent : BeyGramTheme.muted
                                        }

                                        MouseArea {
                                            id: sendMouse
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            enabled: composer.text.trim().length > 0
                                            cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                                            onClicked: page.submitComposer()
                                        }

                                        ToolTip.visible: sendMouse.containsMouse
                                        ToolTip.text: page.editingId > 0
                                                      ? qsTr("Save changes")
                                                      : qsTr("Send (Enter)")
                                        ToolTip.delay: 600
                                    }
                                }
                            }
                        }
                    }
                }

                // ----------------------------------------------------------
                // Animated Slide-In Conversation Info / Profile Drawer
                // ----------------------------------------------------------
                Rectangle {
                    id: infoDrawer

                    readonly property bool overlayMode: conversationPane.width < 720
                    readonly property int drawerTargetWidth: Math.min(340, Math.max(280, Math.round(conversationPane.width * 0.34)))

                    Layout.fillHeight: true
                    Layout.preferredWidth: (!overlayMode && page.chatInfoOpen) ? drawerTargetWidth : 0
                    visible: !overlayMode && Layout.preferredWidth > 0
                    color: BeyGramTheme.surface
                    clip: true

                    Behavior on Layout.preferredWidth {
                        NumberAnimation {
                            duration: page._animNormal
                            easing.type: Easing.OutCubic
                        }
                    }

                    Rectangle {
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        width: 1
                        color: BeyGramTheme.line
                    }

                    ScrollView {
                        anchors.fill: parent
                        anchors.leftMargin: 1
                        contentWidth: availableWidth
                        clip: true

                        ColumnLayout {
                            width: infoDrawer.drawerTargetWidth - 1
                            spacing: Design.space3

                            DsHeader {
                                Layout.fillWidth: true
                                title: qsTr("Chat info")
                                bordered: true

                                trailing: DsIconButton {
                                    icon: "\uE711"
                                    size: "sm"
                                    tooltip: qsTr("Close chat info")
                                    onClicked: page.closeChatInfo()
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.leftMargin: Design.space4
                                Layout.rightMargin: Design.space4
                                Layout.topMargin: Design.space2
                                Layout.bottomMargin: Design.space5
                                spacing: Design.space3

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
                                    wrapMode: Text.WordWrap
                                }

                                Text {
                                    Layout.fillWidth: true
                                    horizontalAlignment: Text.AlignHCenter
                                    visible: BeyGramApp.profileUsername.length > 0
                                    text: "@" + BeyGramApp.profileUsername
                                    color: BeyGramTheme.accentInk
                                    font.pixelSize: Design.fontSmall
                                    elide: Text.ElideRight
                                }

                                Text {
                                    Layout.fillWidth: true
                                    horizontalAlignment: Text.AlignHCenter
                                    visible: BeyGramApp.profileIsGroup || BeyGramApp.profileIsChannel
                                    text: (BeyGramApp.profileIsChannel ? qsTr("Channel") : qsTr("Group"))
                                          + (BeyGramApp.profileMemberCount > 0
                                             ? " · " + qsTr("%n member(s)", "", BeyGramApp.profileMemberCount)
                                             : "")
                                    color: BeyGramTheme.muted
                                    font.pixelSize: Design.fontSmall
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

                                DsDivider { Layout.fillWidth: true }

                                DsCard {
                                    Layout.fillWidth: true
                                    padding: Design.space1

                                    content: ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 0

                                        DsListRow {
                                            Layout.fillWidth: true
                                            title: qsTr("Mute notifications")
                                            subtitle: page.currentChatMuted
                                                      ? qsTr("Muted")
                                                      : qsTr("Alerts enabled")
                                            onClicked: muteInfoSwitch.toggle()

                                            trailing: DsSwitch {
                                                id: muteInfoSwitch
                                                checked: page.currentChatMuted
                                                onToggled: BeyGramApp.setChatMuted(
                                                               BeyGramApp.currentChatId, checked)
                                            }
                                        }

                                        DsDivider { inset: Design.space3 }

                                        DsListRow {
                                            Layout.fillWidth: true
                                            title: qsTr("Notification settings")
                                            subtitle: qsTr("Sound and preview options")
                                            onClicked: chatNotifDialog.open()
                                        }

                                        DsDivider { inset: Design.space3 }

                                        DsListRow {
                                            Layout.fillWidth: true
                                            title: qsTr("Search messages")
                                            onClicked: {
                                                page.openSearch()
                                                page.closeChatInfo()
                                            }
                                        }

                                        DsDivider { inset: Design.space3 }

                                        DsListRow {
                                            Layout.fillWidth: true
                                            title: qsTr("Shared media")
                                            subtitle: page._loadedMediaIds.length > 0
                                                      ? qsTr("%n item(s) in view", "", page._loadedMediaIds.length)
                                                      : qsTr("Photos and videos")
                                            onClicked: {
                                                if (page._loadedMediaIds.length > 0 && BeyGramApp.gallery) {
                                                    BeyGramApp.gallery.openGallery(
                                                        page._loadedMediaIds,
                                                        page._loadedMediaIds[0])
                                                } else {
                                                    toast.show(qsTr("No shared media in loaded messages"), "info")
                                                }
                                            }
                                        }

                                        DsDivider { inset: Design.space3 }

                                        DsListRow {
                                            Layout.fillWidth: true
                                            title: BeyGramApp.archiveOpen
                                                   ? qsTr("Unarchive chat")
                                                   : qsTr("Archive chat")
                                            onClicked: BeyGramApp.setChatArchived(
                                                           BeyGramApp.currentChatId,
                                                           !BeyGramApp.archiveOpen)
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // Narrow-pane overlay info sheet when conversationPane.width < 720
            Rectangle {
                anchors.fill: parent
                visible: conversationPane.width < 720 && page.chatInfoOpen && BeyGramApp.currentChatId !== 0
                color: Qt.alpha("#000000", 0.45)
                z: 35

                MouseArea {
                    anchors.fill: parent
                    onClicked: page.closeChatInfo()
                }

                Rectangle {
                    anchors.top: parent.top
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    width: Math.min(320, parent.width - Design.space6)
                    color: BeyGramTheme.surface
                    border.width: 1
                    border.color: BeyGramTheme.line

                    MouseArea { anchors.fill: parent }

                    ScrollView {
                        anchors.fill: parent
                        contentWidth: availableWidth
                        clip: true

                        ColumnLayout {
                            width: parent.width
                            spacing: Design.space3

                            DsHeader {
                                Layout.fillWidth: true
                                title: qsTr("Chat info")
                                trailing: DsIconButton {
                                    icon: "\uE711"
                                    size: "sm"
                                    tooltip: qsTr("Close chat info")
                                    onClicked: page.closeChatInfo()
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.margins: Design.space4
                                spacing: Design.space3

                                Avatar {
                                    Layout.alignment: Qt.AlignHCenter
                                    size: Design.avatarLarge
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
                                    wrapMode: Text.WordWrap
                                }

                                Text {
                                    Layout.fillWidth: true
                                    visible: BeyGramApp.profileBio.length > 0
                                    text: BeyGramApp.profileBio
                                    color: BeyGramTheme.text
                                    font.pixelSize: Design.fontBody
                                    wrapMode: Text.WordWrap
                                }

                                DsButton {
                                    Layout.fillWidth: true
                                    text: page.currentChatMuted
                                          ? qsTr("Unmute notifications")
                                          : qsTr("Mute notifications")
                                    tone: "secondary"
                                    onClicked: BeyGramApp.setChatMuted(
                                                   BeyGramApp.currentChatId,
                                                   !page.currentChatMuted)
                                }

                                DsButton {
                                    Layout.fillWidth: true
                                    text: BeyGramApp.archiveOpen
                                          ? qsTr("Unarchive chat")
                                          : qsTr("Archive chat")
                                    tone: "secondary"
                                    onClicked: BeyGramApp.setChatArchived(
                                                   BeyGramApp.currentChatId,
                                                   !BeyGramApp.archiveOpen)
                                }
                            }
                        }
                    }
                }
            }

            // --------------------------------------------------------------
            // Welcoming no-chat-selected state (wide layout)
            // --------------------------------------------------------------
            DsCard {
                anchors.centerIn: parent
                width: Math.min(380, Math.max(240, conversationPane.width - Design.space8))
                padding: Design.space6
                visible: BeyGramApp.currentChatId === 0

                content: DsEmptyState {
                    Layout.fillWidth: true
                    glyph: "\uD83D\uDCAC"
                    title: qsTr("Select a conversation")
                    description: qsTr("Choose a chat from the list or press Ctrl+K to find people, groups, and messages.")
                    actionText: qsTr("Search chats (Ctrl+K)")
                    onActionRequested: page.globalSearchRequested()
                }
            }

            // Inline composer & reaction popups anchored inside conversationPane
            EmojiPicker {
                id: emojiPicker
                facade: BeyGramApp.emoji
            }

            StickerPicker {
                id: stickerPicker
                loading: BeyGramApp.stickerPanel ? BeyGramApp.stickerPanel.loading : false
            }

            ReactionPicker {
                id: reactionPicker
                facade: BeyGramApp.reactions
            }

            DsToast {
                id: toast
                anchors.fill: parent
                z: 50
            }
        }
    }

    // ------------------------------------------------------------------
    // App & Facade connections
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
                page.chatInfoOpen = false
                page.loadedChatId = 0
            } else if (BeyGramApp.currentChatId !== page.loadedChatId) {
                page.loadDraft()
            }
            Qt.callLater(page.syncChatStats)
        }

        function onReplyStateChanged() {
            page.scheduleDraft()
        }

        function onProfileChanged() {
            if (BeyGramApp.profileVisible)
                page.chatInfoOpen = true
        }
    }

    Connections {
        target: BeyGramApp.emoji
        ignoreUnknownSignals: true
        function onEmojiActivated(emoji) {
            page.insertComposerText(emoji)
        }
    }

    Connections {
        target: BeyGramApp.reactions
        ignoreUnknownSignals: true
        function onReactionRequested() {
            reactionPicker.close()
        }
    }

    Connections {
        target: BeyGramApp.selection
        ignoreUnknownSignals: true
        function onChanged() {
            if (BeyGramApp.selection && BeyGramApp.selection.forwardOpen && !forwardDialog.visible) {
                forwardDialog.messageCount = BeyGramApp.selection.count
                forwardDialog.open()
            }
        }
    }

    Connections {
        target: BeyGramApp.voiceComposer
        ignoreUnknownSignals: true
        function onSendRequested(durationSeconds) {
            toast.show(qsTr("Voice capture (%1s) is not yet wired to audio input").arg(durationSeconds), "info")
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
            if (BeyGramApp.selection && BeyGramApp.selection.active && BeyGramApp.selection.forwardOpen) {
                BeyGramApp.selection.chooseForwardChat(String(chatId))
                BeyGramApp.selection.clear()
            } else {
                BeyGramApp.forwardMessage(BeyGramApp.currentChatId,
                                          page.forwardMessageId, chatId)
            }
        }
        onClosed: {
            if (BeyGramApp.selection && BeyGramApp.selection.forwardOpen)
                BeyGramApp.selection.cancelForward()
        }
    }

    ChatNotificationSettings {
        id: chatNotifDialog
        muted: page.currentChatMuted
        sound: true
        preview: true
        onSaveRequested: (muted, sound, preview) => {
            if (BeyGramApp.notificationSettings) {
                BeyGramApp.notificationSettings.loadChat(
                    String(BeyGramApp.currentChatId),
                    page.currentChatMuted, true, true)
                BeyGramApp.notificationSettings.setChatMuted(muted)
                BeyGramApp.notificationSettings.setChatSound(sound)
                BeyGramApp.notificationSettings.setChatPreview(preview)
                BeyGramApp.notificationSettings.saveChat()
            } else {
                BeyGramApp.setChatMuted(BeyGramApp.currentChatId, muted)
            }
        }
    }

    FileDialog {
        id: attachDialog
        title: qsTr("Send a file")
        fileMode: FileDialog.OpenFile
        nameFilters: [
            qsTr("Media (*.jpg *.jpeg *.png *.webp *.mp4 *.mkv *.mov *.webm)"),
            qsTr("Documents (*.pdf *.zip *.txt *.doc *.docx)"),
            qsTr("All files (*)")
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

    DsDialog {
        id: deleteDialog

        property var messageId: 0

        title: qsTr("Delete message?")
        description: qsTr("Choose whether to remove this message only for you or for everyone. TDLib enforces the account's delete permission.")
        primaryText: qsTr("Delete for everyone")
        secondaryText: qsTr("Cancel")
        tone: "danger"

        onAccepted: BeyGramApp.deleteChatMessage(deleteDialog.messageId, true)

        DsButton {
            Layout.fillWidth: true
            text: qsTr("Delete for me")
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
            text: chatMenu.chatMuted ? qsTr("Unmute") : qsTr("Mute")
            onTriggered: BeyGramApp.setChatMuted(chatMenu.chatId,
                                                 !chatMenu.chatMuted)
        }

        DsMenuItem {
            text: BeyGramApp.archiveOpen ? qsTr("Unarchive") : qsTr("Archive")
            onTriggered: BeyGramApp.setChatArchived(chatMenu.chatId,
                                                    !BeyGramApp.archiveOpen)
        }
    }
}
