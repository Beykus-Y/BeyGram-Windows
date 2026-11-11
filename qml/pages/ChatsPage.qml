import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import BeyGram.Theme 1.0
import BeyGram.App 1.0

Item {
    id: page

    property int editingId: 0
    property int loadedChatId: 0
    property bool searchOpen: false
    property int forwardMessageId: 0

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

    Timer {
        id: draftTimer
        property int chatId: 0
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

        // ------------------------------------------------------------------
        // Chat list
        // ------------------------------------------------------------------
        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: 360
            color: BeyGramTheme.surface

            Rectangle {
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: 1
                color: BeyGramTheme.line
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 4
                    Layout.rightMargin: 2

                    Text {
                        text: "Chats"
                        color: BeyGramTheme.text
                        font.pixelSize: 26
                        font.weight: Font.DemiBold
                        Layout.fillWidth: true
                    }
                    Text {
                        text: chatListView.count
                        color: BeyGramTheme.muted
                        font.pixelSize: 11
                    }
                }

                ChatFolderTabs {
                    Layout.fillWidth: true
                    model: BeyGramApp.chatFolders
                    currentFolderId: BeyGramApp.currentFolderId
                    onFolderSelected: (folderId) => BeyGramApp.selectFolder(folderId)
                }

                TextField {
                    id: chatSearchField
                    Layout.fillWidth: true
                    implicitHeight: 44
                    leftPadding: 13
                    rightPadding: 30
                    placeholderText: "Search chats"
                    placeholderTextColor: Qt.alpha(BeyGramTheme.muted, 0.75)
                    color: BeyGramTheme.text
                    selectionColor: BeyGramTheme.accent
                    selectedTextColor: BeyGramTheme.onAccent
                    font.pixelSize: 13
                    text: BeyGramApp.chatFilter
                    onTextEdited: BeyGramApp.chatFilter = text

                    background: Rectangle {
                        radius: 22
                        color: BeyGramTheme.surface2
                        border.width: chatSearchField.activeFocus ? 1 : 0
                        border.color: BeyGramTheme.accent
                    }

                    Text {
                        anchors.right: parent.right
                        anchors.rightMargin: 10
                        anchors.verticalCenter: parent.verticalCenter
                        visible: chatSearchField.text.length > 0
                        text: "Г—"
                        color: BeyGramTheme.muted
                        font.pixelSize: 16
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                chatSearchField.text = ""
                                BeyGramApp.chatFilter = ""
                            }
                        }
                    }
                }

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    ListView {
                        id: chatListView
                        anchors.fill: parent
                        clip: true
                        spacing: 1
                        model: BeyGramApp.chatList
                        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                        delegate: ChatRow {
                            width: chatListView.width
                            chatId: model.chatId
                            title: model.title
                            preview: model.preview
                            unreadCount: model.unreadCount
                            pinned: model.pinned
                            muted: model.muted
                            initials: model.initials
                            accentIndex: model.accentIndex
                            lastDate: model.lastDate
                            photoFileId: model.photoFileId
                            photoReady: model.photoReady
                            photoRevision: model.photoRevision
                            selected: BeyGramApp.currentChatId === model.chatId
                            onClicked: {
                                page.resetComposer()
                                BeyGramApp.openChat(model.chatId)
                            }
                        }
                    }

                    BusyIndicator {
                        anchors.centerIn: parent
                        running: BeyGramApp.chatLoading && chatListView.count === 0
                        visible: running
                    }

                    Column {
                        anchors.centerIn: parent
                        spacing: 6
                        width: parent.width - 40
                        visible: chatListView.count === 0 && !BeyGramApp.chatLoading

                        Text {
                            width: parent.width
                            horizontalAlignment: Text.AlignHCenter
                            text: BeyGramApp.chatError.length > 0 ? "Chats unavailable"
                                                                  : "No chats yet"
                            color: BeyGramTheme.text
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                        }
                        Text {
                            width: parent.width
                            horizontalAlignment: Text.AlignHCenter
                            text: BeyGramApp.chatError.length > 0
                                  ? BeyGramApp.chatError
                                  : "Your Telegram chats will appear here."
                            color: BeyGramTheme.muted
                            font.pixelSize: 11
                            wrapMode: Text.WordWrap
                        }
                    }
                }
            }
        }

        // ------------------------------------------------------------------
        // Conversation
        // ------------------------------------------------------------------
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: BeyGramTheme.bg

            ColumnLayout {
                anchors.fill: parent
                spacing: 0
                visible: BeyGramApp.currentChatId !== 0

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 64
                    color: BeyGramTheme.surface

                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        height: 1
                        color: BeyGramTheme.line
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 18
                        anchors.rightMargin: 18
                        spacing: 12

                        Avatar {
                            size: 40
                            fileId: BeyGramApp.currentChatPhotoFileId
                            ready: BeyGramApp.currentChatPhotoReady
                            revision: BeyGramApp.currentChatPhotoRevision
                            initials: BeyGramApp.currentChatTitle.length > 0
                                      ? BeyGramApp.currentChatTitle.charAt(0).toUpperCase()
                                      : "?"
                            tint: BeyGramTheme.accentSoft
                            ink: BeyGramTheme.accentInk
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 1
                            Text {
                                Layout.fillWidth: true
                                text: BeyGramApp.currentChatTitle
                                color: BeyGramTheme.text
                                font.pixelSize: 16
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                            }
                            Text {
                                Layout.fillWidth: true
                                text: BeyGramApp.currentChatSubtitle
                                color: BeyGramTheme.muted
                                font.pixelSize: 12
                            }
                        }

                        SecondaryButton {
                            text: "Profile"
                            implicitHeight: 30
                            onClicked: BeyGramApp.requestProfile(BeyGramApp.currentChatId)
                        }

                        SecondaryButton {
                            text: page.searchOpen ? "Close search" : "Search"
                            implicitHeight: 30
                            onClicked: page.searchOpen ? page.closeSearch() : page.openSearch()
                        }

                        Pill {
                            visible: BeyGramApp.connectionTitle !== "Connected"
                            text: BeyGramApp.connectionTitle.toUpperCase()
                            tone: "warn"
                        }
                    }
                }

                TypingIndicator {
                    Layout.fillWidth: true
                    Layout.leftMargin: 18
                    active: BeyGramApp.typingText.length > 0
                    text: BeyGramApp.typingText
                }

                Rectangle {
                    Layout.fillWidth: true
                    visible: page.searchOpen
                    implicitHeight: 48
                    color: BeyGramTheme.surface

                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        height: 1
                        color: BeyGramTheme.line
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 16
                        anchors.rightMargin: 12
                        spacing: 8

                        TextField {
                            id: searchField
                            Layout.fillWidth: true
                            implicitHeight: 34
                            leftPadding: 12
                            rightPadding: 12
                            placeholderText: "Search in this chat"
                            placeholderTextColor: Qt.alpha(BeyGramTheme.muted, 0.75)
                            color: BeyGramTheme.text
                            selectionColor: BeyGramTheme.accent
                            selectedTextColor: BeyGramTheme.onAccent
                            font.pixelSize: 13
                            background: Rectangle {
                                radius: 17
                                color: BeyGramTheme.surface2
                                border.width: searchField.activeFocus ? 1 : 0
                                border.color: BeyGramTheme.accent
                            }
                            onTextEdited: searchTimer.restart()
                        }
                        BusyIndicator {
                            running: BeyGramApp.searchLoading
                            visible: running
                            implicitWidth: 22
                            implicitHeight: 22
                        }
                    }
                }

                PinnedMessageBanner {
                    Layout.fillWidth: true
                    visible: BeyGramApp.pinnedVisible
                    title: "Pinned message"
                    preview: BeyGramApp.pinnedPreview
                    onActivated: {}
                    onUnpinRequested: BeyGramApp.unpinPinnedMessage()
                }

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    ListView {
                        id: messageListView
                        anchors.fill: parent
                        clip: true
                        spacing: 4
                        model: BeyGramApp.messages
                        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                        header: Item {
                            width: messageListView.width
                            height: BeyGramApp.hasMoreHistory ? 36 : 6

                            SecondaryButton {
                                anchors.centerIn: parent
                                text: "Load older messages"
                                visible: BeyGramApp.hasMoreHistory
                                         && !BeyGramApp.historyLoading
                                onClicked: BeyGramApp.loadOlderMessages()
                            }
                            BusyIndicator {
                                anchors.centerIn: parent
                                running: BeyGramApp.historyLoading
                                visible: running
                                implicitWidth: 22
                                implicitHeight: 22
                            }
                        }

                        delegate: MessageBubble {
                            messageId: model.messageId
                            text: model.text
                            textHtml: model.textHtml
                            replyPreview: model.replyPreview
                            reactionsJson: model.reactionsJson
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

                    Rectangle {
                        id: searchPanel
                        anchors.top: parent.top
                        anchors.left: parent.left
                        anchors.right: parent.right
                        height: Math.min(320, parent.height * 0.55)
                        visible: page.searchOpen && searchField.text.trim().length > 0
                        z: 20
                        color: BeyGramTheme.surface
                        border.width: 1
                        border.color: BeyGramTheme.line

                        ListView {
                            id: searchList
                            anchors.fill: parent
                            clip: true
                            model: BeyGramApp.searchResults
                            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                            delegate: Rectangle {
                                width: searchList.width
                                height: 56
                                color: hover.containsMouse ? BeyGramTheme.surface2 : "transparent"

                                Column {
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.verticalCenter: parent.verticalCenter
                                    anchors.leftMargin: 16
                                    anchors.rightMargin: 16
                                    spacing: 3
                                    Text {
                                        width: parent.width
                                        text: model.text.length > 0 ? model.text : "[media]"
                                        color: BeyGramTheme.text
                                        font.pixelSize: 13
                                        elide: Text.ElideRight
                                    }
                                    Text {
                                        text: model.date > 0
                                              ? Qt.formatDateTime(new Date(model.date * 1000),
                                                                  "dd.MM.yyyy hh:mm")
                                              : ""
                                        color: BeyGramTheme.muted
                                        font.pixelSize: 11
                                    }
                                }

                                MouseArea {
                                    id: hover
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: page.closeSearch()
                                }
                            }
                        }

                        Text {
                            anchors.centerIn: parent
                            visible: searchList.count === 0 && !BeyGramApp.searchLoading
                            text: "No matches"
                            color: BeyGramTheme.muted
                            font.pixelSize: 12
                        }
                    }

                    BusyIndicator {
                        anchors.centerIn: parent
                        running: BeyGramApp.historyLoading && messageListView.count === 0
                        visible: running
                    }

                    Column {
                        anchors.centerIn: parent
                        spacing: 6
                        width: parent.width - 80
                        visible: messageListView.count === 0 && !BeyGramApp.historyLoading
                        Text {
                            width: parent.width
                            horizontalAlignment: Text.AlignHCenter
                            text: "No messages loaded"
                            color: BeyGramTheme.muted
                            font.pixelSize: 12
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    visible: BeyGramApp.chatError.length > 0
                    implicitHeight: errorRow.implicitHeight + 14
                    color: Qt.alpha(BeyGramTheme.danger, 0.10)

                    RowLayout {
                        id: errorRow
                        anchors.fill: parent
                        anchors.leftMargin: 16
                        anchors.rightMargin: 12
                        Text {
                            Layout.fillWidth: true
                            text: BeyGramApp.chatError
                            color: BeyGramTheme.danger
                            font.pixelSize: 11
                            wrapMode: Text.WordWrap
                        }
                        Button {
                            text: "Г—"
                            flat: true
                            implicitWidth: 26
                            implicitHeight: 26
                            onClicked: BeyGramApp.clearChatError()
                            contentItem: Text {
                                text: "Г—"
                                color: BeyGramTheme.danger
                                font.pixelSize: 16
                                horizontalAlignment: Text.AlignHCenter
                            }
                            background: null
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: composerColumn.implicitHeight + 18
                    color: Qt.alpha(BeyGramTheme.surface, BeyGramTheme.darkMode ? 0.72 : 0.88)

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
                        anchors.margins: 9
                        spacing: 6

                        RowLayout {
                            Layout.fillWidth: true
                            visible: page.editingId > 0
                            Text {
                                Layout.fillWidth: true
                                text: "Editing message"
                                color: BeyGramTheme.muted
                                font.pixelSize: 10
                            }
                            SecondaryButton {
                                text: "Cancel"
                                implicitHeight: 26
                                onClicked: page.resetComposer()
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            visible: page.editingId === 0
                                     && BeyGramApp.replyToMessageId > 0
                            Text {
                                Layout.fillWidth: true
                                text: "Replying: " + BeyGramApp.replyPreview
                                color: BeyGramTheme.muted
                                font.pixelSize: 10
                                elide: Text.ElideRight
                            }
                            SecondaryButton {
                                text: "Cancel"
                                implicitHeight: 26
                                onClicked: BeyGramApp.cancelReply()
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            SecondaryButton {
                                text: "Attach"
                                implicitHeight: 40
                                onClicked: attachDialog.open()
                            }

                            TextArea {
                                id: composer
                                Layout.fillWidth: true
                                implicitHeight: Math.min(92, Math.max(42, contentHeight + 16))
                                leftPadding: 12
                                rightPadding: 12
                                topPadding: 9
                                bottomPadding: 9
                                placeholderText: "Write a message"
                                placeholderTextColor: Qt.alpha(BeyGramTheme.muted, 0.75)
                                color: BeyGramTheme.text
                                selectionColor: BeyGramTheme.accent
                                selectedTextColor: BeyGramTheme.onAccent
                                font.pixelSize: 14
                                wrapMode: TextArea.Wrap
                                background: Rectangle {
                                    radius: 20
                                    color: BeyGramTheme.surface2
                                    border.width: composer.activeFocus ? 1 : 0
                                    border.color: BeyGramTheme.accent
                                }
                                Keys.onPressed: (event) => {
                                    if (event.key === Qt.Key_Return
                                        && !(event.modifiers & Qt.ShiftModifier)) {
                                        page.submitComposer()
                                        event.accepted = true
                                    }
                                }
                                onTextChanged: {
                                    page.scheduleDraft()
                                    if (text.trim().length > 0)
                                        BeyGramApp.notifyTyping()
                                }
                            }

                            PrimaryButton {
                                text: page.editingId > 0 ? "Save" : "Send"
                                enabled: composer.text.trim().length > 0
                                onClicked: page.submitComposer()
                            }
                        }

                    }
                }
            }

            Rectangle {
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                width: 320
                visible: BeyGramApp.profileVisible
                z: 40
                color: BeyGramTheme.surface
                border.width: 1
                border.color: BeyGramTheme.line

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 20
                    spacing: 12

                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            Layout.fillWidth: true
                            text: "Profile"
                            color: BeyGramTheme.text
                            font.pixelSize: 18
                            font.weight: Font.DemiBold
                        }
                        SecondaryButton {
                            text: "Close"
                            implicitHeight: 28
                            onClicked: BeyGramApp.closeProfile()
                        }
                    }

                    Avatar {
                        Layout.alignment: Qt.AlignHCenter
                        size: 84
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
                        font.pixelSize: 17
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
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }
                    Text {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        visible: BeyGramApp.profileIsGroup || BeyGramApp.profileIsChannel
                        text: (BeyGramApp.profileIsChannel ? "Channel" : "Group")
                              + (BeyGramApp.profileMemberCount > 0
                                 ? " В· " + BeyGramApp.profileMemberCount + " members" : "")
                        color: BeyGramTheme.muted
                        font.pixelSize: 12
                    }

                    Rectangle { Layout.fillWidth: true; height: 1; color: BeyGramTheme.line }

                    Text {
                        Layout.fillWidth: true
                        visible: BeyGramApp.profileBio.length > 0
                        text: BeyGramApp.profileBio
                        color: BeyGramTheme.text
                        font.pixelSize: 13
                        wrapMode: Text.WordWrap
                        lineHeight: 1.35
                    }
                    Text {
                        Layout.fillWidth: true
                        visible: BeyGramApp.profilePhone.length > 0
                        text: BeyGramApp.profilePhone
                        color: BeyGramTheme.muted
                        font.pixelSize: 12
                    }
                    BusyIndicator {
                        Layout.alignment: Qt.AlignHCenter
                        running: BeyGramApp.profileLoading
                        visible: running
                    }
                    Item { Layout.fillHeight: true }
                }
            }

            // Empty conversation state
            ColumnLayout {
                anchors.centerIn: parent
                spacing: 8
                width: 360
                visible: BeyGramApp.currentChatId === 0

                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    width: 64
                    height: 64
                    radius: 20
                    color: Qt.alpha(BeyGramTheme.accent, 0.12)
                    Text {
                        anchors.centerIn: parent
                        text: "B"
                        color: BeyGramTheme.accent
                        font.pixelSize: 28
                        font.weight: Font.DemiBold
                    }
                }
                Text {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: "Select a chat"
                    color: BeyGramTheme.text
                    font.pixelSize: 20
                    font.weight: Font.DemiBold
                }
                Text {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: "The chat list and message history are loaded from your authorized Telegram session."
                    color: BeyGramTheme.muted
                    font.pixelSize: 11
                    wrapMode: Text.WordWrap
                    lineHeight: 1.3
                }
            }
        }
    }

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

    Dialog {
        id: deleteDialog
        property int messageId: 0
        title: "Delete message?"
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.NoButton
        width: 400

        ColumnLayout {
            spacing: 12
            Text {
                Layout.fillWidth: true
                text: "Delete this message for everyone? TDLib enforces the account's delete permission."
                color: BeyGramTheme.text
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }
            RowLayout {
                Layout.alignment: Qt.AlignRight
                spacing: 8
                SecondaryButton {
                    text: "Cancel"
                    onClicked: deleteDialog.close()
                }
                PrimaryButton {
                    text: "Delete"
                    tone: "danger"
                    onClicked: {
                        deleteDialog.close()
                        BeyGramApp.deleteChatMessage(deleteDialog.messageId)
                    }
                }
            }
        }
    }
}
