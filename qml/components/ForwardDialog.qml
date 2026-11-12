pragma ComponentBehavior: Bound

// ForwardDialog - modal chat picker for forwarding one or more messages.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0
import "../ds/design.js" as Design

DsDialog {
    id: root

    property var model: null
    property int messageCount: 0

    signal chatSelected(var chatId)

    readonly property string dialogTitle: messageCount > 1
                                          ? qsTr("Forward %n message(s)", "", messageCount)
                                          : qsTr("Forward message")

    readonly property string query: searchField.text.trim()

    readonly property var avatarColors: [
        "#C2593F", "#5B4BC4", "#2F8A7B",
        "#A9731D", "#3A78C2", "#B0476E"
    ]

    property var _selectedChatId: 0

    component ForwardChatRow: Item {
        id: chatRow

        required property var chatId
        required property string title
        required property string preview
        required property int unreadCount
        required property bool muted
        required property string initials
        required property int photoFileId
        required property bool photoReady
        required property int photoRevision
        required property int accentIndex
        required property int index

        readonly property bool matched: root.matches(chatRow.title)

        width: chatList.width
        height: matched ? Design.rowHeight : 0
        visible: matched

        DsListRow {
            width: parent.width
            height: parent.height

            title: chatRow.title
            subtitle: chatRow.preview
            unread: chatRow.unreadCount
            muted: chatRow.muted
            selected: root._selectedChatId === chatRow.chatId

            leading: Avatar {
                size: Design.avatarSmall
                fileId: chatRow.photoFileId
                ready: chatRow.photoReady
                revision: chatRow.photoRevision
                initials: chatRow.initials
                tint: root.avatarColors[Math.abs(chatRow.accentIndex)
                                        % root.avatarColors.length]
                ink: "white"
            }

            onClicked: {
                chatList.currentIndex = chatRow.index
                root._selectedChatId = chatRow.chatId
                chatList.forceActiveFocus()
            }
        }
    }

    function matches(title) {
        if (root.query.length === 0)
            return true
        const value = (title === undefined || title === null) ? "" : String(title)
        return value.toLowerCase().indexOf(root.query.toLowerCase()) >= 0
    }

    function choose(chatId) {
        if (chatId === undefined || chatId === null || chatId === 0)
            return
        root.chatSelected(chatId)
        root.close()
    }

    function resetSelection() {
        root._selectedChatId = 0
        if (chatList) {
            chatList.currentIndex = -1
            chatList.positionViewAtBeginning()
        }
    }

    function firstMatchIndex() {
        for (let i = 0; i < chatList.count; ++i) {
            const row = chatList.itemAtIndex(i) as ForwardChatRow
            if (row && row.matched)
                return i
        }
        return -1
    }

    function selectFirstMatch() {
        const index = root.firstMatchIndex()
        if (index < 0)
            return false
        chatList.currentIndex = index
        const row = chatList.itemAtIndex(index) as ForwardChatRow
        if (row)
            root._selectedChatId = row.chatId
        chatList.positionViewAtIndex(index, ListView.Contain)
        return true
    }

    function moveCurrent(delta) {
        if (chatList.count === 0)
            return
        let next = chatList.currentIndex
        if (next < 0)
            next = delta > 0 ? -1 : chatList.count
        for (let guard = 0; guard < chatList.count; ++guard) {
            next += delta
            if (next < 0 || next >= chatList.count)
                return
            let row = chatList.itemAtIndex(next) as ForwardChatRow
            if (!row) {
                chatList.positionViewAtIndex(next, ListView.Contain)
                row = chatList.itemAtIndex(next) as ForwardChatRow
            }
            if (row && row.matched) {
                chatList.currentIndex = next
                chatList.positionViewAtIndex(next, ListView.Contain)
                root._selectedChatId = row.chatId
                return
            }
        }
    }

    title: root.dialogTitle
    description: root.messageCount > 1
                 ? qsTr("Choose a chat to forward these %n message(s) to.", "", root.messageCount)
                 : qsTr("Choose a chat to forward this message to.")
    width: 500

    onOpened: {
        searchField.text = ""
        root.resetSelection()
        searchField.forceActiveFocus()
    }

    onModelChanged: root.resetSelection()

    DsSearchField {
        id: searchField
        Layout.fillWidth: true
        placeholderText: qsTr("Search chats...")
        onTextChanged: root.resetSelection()
        onAccepted: {
            if (root.selectFirstMatch())
                chatList.forceActiveFocus()
        }
        Keys.onDownPressed: (event) => {
            if (root.selectFirstMatch())
                chatList.forceActiveFocus()
            event.accepted = true
        }
    }

    Item {
        Layout.fillWidth: true
        Layout.preferredHeight: 320

        ListView {
            id: chatList
            anchors.fill: parent
            clip: true
            spacing: 0
            model: root.model
            boundsBehavior: Flickable.StopAtBounds
            activeFocusOnTab: true
            keyNavigationEnabled: false
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            onActiveFocusChanged: {
                if (activeFocus && root._selectedChatId === 0)
                    root.selectFirstMatch()
            }

            Keys.onUpPressed: (event) => {
                root.moveCurrent(-1)
                event.accepted = true
            }
            Keys.onDownPressed: (event) => {
                root.moveCurrent(1)
                event.accepted = true
            }
            Keys.onReturnPressed: (event) => {
                root.choose(root._selectedChatId)
                event.accepted = true
            }
            Keys.onEnterPressed: (event) => {
                root.choose(root._selectedChatId)
                event.accepted = true
            }
            Keys.onEscapePressed: (event) => {
                root.close()
                event.accepted = true
            }

            delegate: ForwardChatRow {}
        }

        DsEmptyState {
            anchors.centerIn: parent
            width: Math.min(parent.width - Design.space8, implicitWidth)
            visible: chatList.count === 0
                     || (root.query.length > 0 && chatList.contentHeight <= 0)
            title: chatList.count === 0 ? qsTr("No chats yet") : qsTr("No matches")
            description: chatList.count === 0
                         ? qsTr("Your Telegram chats will appear here.")
                         : qsTr("Try a different name or clear the search.")
        }
    }

    footer: Item {
        implicitHeight: 36 + Design.space5

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Design.space5
            anchors.rightMargin: Design.space5
            anchors.bottomMargin: Design.space5
            spacing: Design.space2

            Item { Layout.fillWidth: true }

            DsButton {
                text: qsTr("Cancel")
                tone: "secondary"
                onClicked: root.close()
            }

            DsButton {
                text: qsTr("Forward")
                enabled: root._selectedChatId !== 0
                onClicked: root.choose(root._selectedChatId)
            }
        }
    }
}
