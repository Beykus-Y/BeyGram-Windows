import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0
import BeyGram.App 1.0

Item {
    id: page

    property int editingId: 0

    function resetComposer() {
        editingId = 0
        composer.text = ""
        BeyGramApp.cancelReply()
    }

    function beginEdit(messageId, text) {
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
        if (editingId > 0)
            BeyGramApp.editChatMessage(editingId, text)
        else
            BeyGramApp.sendChatMessage(text)
        composer.text = ""
        editingId = 0
        composer.forceActiveFocus()
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

                TextField {
                    id: searchField
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
                        border.width: searchField.activeFocus ? 1 : 0
                        border.color: BeyGramTheme.accent
                    }

                    Text {
                        anchors.right: parent.right
                        anchors.rightMargin: 10
                        anchors.verticalCenter: parent.verticalCenter
                        visible: searchField.text.length > 0
                        text: "×"
                        color: BeyGramTheme.muted
                        font.pixelSize: 16
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                searchField.text = ""
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

                        Pill {
                            visible: BeyGramApp.connectionTitle !== "Connected"
                            text: BeyGramApp.connectionTitle.toUpperCase()
                            tone: "warn"
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
                            onEditRequested: (messageId, text) => page.beginEdit(messageId, text)
                            onReplyRequested: (messageId, preview) => BeyGramApp.beginReply(
                                                  messageId, preview)
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
                            text: "×"
                            flat: true
                            implicitWidth: 26
                            implicitHeight: 26
                            onClicked: BeyGramApp.clearChatError()
                            contentItem: Text {
                                text: "×"
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
            if (BeyGramApp.currentChatId === 0)
                page.resetComposer()
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
